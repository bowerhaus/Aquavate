//
//  HydrationReminderService.swift
//  Aquavate
//
//  Created by Claude Code on 22/01/2026.
//

import Foundation

@MainActor
class HydrationReminderService: ObservableObject {
    // MARK: - Configuration Constants (Pace-Based Model)

    static let activeHoursStart = 7           // 7am - start of active hydration hours
    static let activeHoursEnd = 22            // 10pm - end of active hydration hours
    static let activeHoursDuration = 15       // hours of active hydration time
    static let attentionThreshold = 0.20      // 20% behind pace = red threshold
    static let maxRemindersPerDay = 12

    #if DEBUG
    static let testModeMinimumDeficit = 10  // Lower notification threshold in test mode (10ml instead of 50ml)
    #endif

    // MARK: - Published Properties

    @Published private(set) var currentUrgency: HydrationUrgency = .onTrack
    @Published private(set) var lastNotifiedUrgency: HydrationUrgency? = nil
    @Published private(set) var lastDrinkTime: Date? = nil
    @Published private(set) var dailyTotalMl: Int = 0
    @Published private(set) var dailyGoalMl: Int = 2500
    @Published private(set) var deficitMl: Int = 0
    @Published private(set) var goalNotificationSentToday: Bool = false {
        didSet {
            UserDefaults.standard.set(goalNotificationSentToday, forKey: "goalNotificationSentToday")
        }
    }

    #if DEBUG
    @Published var testModeEnabled: Bool = false {
        didSet {
            UserDefaults.standard.set(testModeEnabled, forKey: "hydrationTestModeEnabled")
            print("[HydrationReminder] Test mode: \(testModeEnabled)")
        }
    }
    #endif

    // MARK: - Dependencies

    weak var notificationManager: NotificationManager?

    // MARK: - Private Properties

    private var evaluationTimer: Timer?

    // MARK: - Initialization

    init() {
        #if DEBUG
        self.testModeEnabled = UserDefaults.standard.bool(forKey: "hydrationTestModeEnabled")
        #endif
        self.goalNotificationSentToday = UserDefaults.standard.bool(forKey: "goalNotificationSentToday")
        startPeriodicEvaluation()
    }

    deinit {
        evaluationTimer?.invalidate()
    }

    // MARK: - Rounding Helpers

    /// Minimum deficit to trigger notifications (below this, considered "on track")
    /// In test mode, uses lower threshold to trigger notifications more easily
    private var minimumDeficitForNotification: Int {
        #if DEBUG
        return testModeEnabled ? Self.testModeMinimumDeficit : 50
        #else
        return 50
        #endif
    }

    /// Round a value to the nearest 50ml
    /// - Parameter value: The value to round
    /// - Returns: Value rounded to nearest 50ml
    static func roundToNearest50(_ value: Int) -> Int {
        return ((value + 25) / 50) * 50
    }

    // MARK: - Pace Calculations

    /// Calculate expected intake based on current time of day
    /// - Returns: Expected ml consumed by now to be on pace
    func calculateExpectedIntake() -> Int {
        let now = Date()
        let calendar = Calendar.current
        let hour = calendar.component(.hour, from: now)
        let minute = calendar.component(.minute, from: now)

        // Before active hours: expect 0
        if hour < Self.activeHoursStart { return 0 }

        // After active hours: expect full goal
        if hour >= Self.activeHoursEnd { return dailyGoalMl }

        // During active hours: proportional based on elapsed time
        let elapsedMinutes = (hour - Self.activeHoursStart) * 60 + minute
        let totalActiveMinutes = Self.activeHoursDuration * 60
        return (elapsedMinutes * dailyGoalMl) / totalActiveMinutes
    }

    /// Calculate deficit from expected pace
    /// - Returns: How many ml behind pace (positive = behind, negative = ahead, 0 = on pace)
    func calculateDeficit() -> Int {
        let expected = calculateExpectedIntake()
        return expected - dailyTotalMl
    }

    // MARK: - State Updates

    /// Update the hydration state from BLE data
    /// - Parameters:
    ///   - totalMl: Daily total in ml
    ///   - goalMl: Daily goal in ml
    ///   - lastDrink: Time of last drink
    func updateState(totalMl: Int, goalMl: Int, lastDrink: Date?) {
        self.dailyTotalMl = totalMl
        self.dailyGoalMl = goalMl
        self.lastDrinkTime = lastDrink
        updateUrgency()
        syncStateToWatch()
    }

    /// Check if we've transitioned back to on-track and send notification if so
    /// - Parameter previousUrgency: The urgency level before the state update
    func checkBackOnTrack(previousUrgency: HydrationUrgency?) {
        guard let previousUrgency = previousUrgency else { return }

        // Check if back on track (was behind, now on track)
        if previousUrgency > .onTrack && currentUrgency == .onTrack {
            lastNotifiedUrgency = nil

            // Send back-on-track notification if enabled
            notificationManager?.scheduleBackOnTrackNotification()

            // Also notify Watch
            WatchConnectivityManager.shared.sendNotificationToWatch(
                type: "backOnTrack",
                title: "Back On Track!",
                body: "Nice work catching up on your hydration."
            )

            print("[HydrationReminder] Back on track! Notification sent.")
        } else if currentUrgency < previousUrgency {
            // Urgency improved but not yet on track
            lastNotifiedUrgency = nil
        }

        print("[HydrationReminder] Drink recorded - deficit=\(deficitMl)ml, urgency=\(currentUrgency)")
    }

    /// Called when daily goal is achieved
    func goalAchieved() {
        lastNotifiedUrgency = nil
        notificationManager?.cancelAllPendingReminders()

        // Only send goal notification once per day
        if !goalNotificationSentToday {
            notificationManager?.scheduleGoalReachedNotification()
            goalNotificationSentToday = true
            print("[HydrationReminder] Goal achieved - reminders cancelled, celebration sent")

            // Also send to Watch for local notification + haptic
            WatchConnectivityManager.shared.sendNotificationToWatch(
                type: "goalReached",
                title: "Goal Reached! 💧",
                body: "Good job! You've hit your daily hydration goal."
            )
        } else {
            print("[HydrationReminder] Goal achieved - reminders cancelled (notification already sent)")
        }
    }

    /// Called at 4am daily reset
    func dailyReset() {
        lastNotifiedUrgency = nil
        dailyTotalMl = 0
        currentUrgency = .onTrack
        goalNotificationSentToday = false
        notificationManager?.resetReminderCount()
        print("[HydrationReminder] Daily reset")
    }

    // MARK: - Urgency Calculation

    private func updateUrgency() {
        let deficit = calculateDeficit()
        self.deficitMl = max(0, deficit)  // Don't show negative (surplus)

        // Goal achieved - always on track
        if dailyTotalMl >= dailyGoalMl {
            currentUrgency = .onTrack
            return
        }

        // On pace or ahead - on track
        if deficit <= 0 {
            currentUrgency = .onTrack
            return
        }

        // Behind pace - check threshold (always 20%, not affected by test mode)
        let redThresholdMl = Int(Double(dailyGoalMl) * Self.attentionThreshold)
        if deficit >= redThresholdMl {
            currentUrgency = .overdue    // 20%+ behind = red
        } else {
            currentUrgency = .attention  // Behind but < 20% = amber
        }
    }

    // MARK: - Reminder Evaluation

    /// Start periodic evaluation (every minute)
    /// This keeps deficit current as time passes and syncs to Watch
    private func startPeriodicEvaluation() {
        evaluationTimer?.invalidate()
        evaluationTimer = Timer.scheduledTimer(withTimeInterval: 60, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.updateUrgency()           // Recalculate deficit based on current time
                self?.syncStateToWatch()        // Push updated state to Watch
                self?.evaluateAndScheduleReminder()
            }
        }
    }

    /// Evaluate current state and schedule reminder if needed
    func evaluateAndScheduleReminder() {
        guard let notificationManager = notificationManager else {
            print("[HydrationReminder] No notification manager")
            return
        }

        // 1. Check notifications enabled & authorized
        guard notificationManager.isEnabled && notificationManager.isAuthorized else {
            print("[HydrationReminder] Notifications not enabled or authorized")
            return
        }

        // 2. Check not in quiet hours (also means we're in active hydration hours)
        guard !notificationManager.isInQuietHours() else {
            print("[HydrationReminder] In quiet hours")
            return
        }

        // 3. Check < 8 reminders sent today
        guard notificationManager.canSendReminder else {
            print("[HydrationReminder] Max reminders reached")
            return
        }

        // 4. Check daily goal not achieved
        guard dailyTotalMl < dailyGoalMl else {
            print("[HydrationReminder] Goal already achieved")
            return
        }

        // 5. Calculate deficit and urgency
        updateUrgency()

        // 6. Only notify if behind pace by at least 50ml (below 50ml is considered "on track")
        guard deficitMl >= minimumDeficitForNotification else {
            print("[HydrationReminder] On pace or deficit below 50ml - no reminder needed")
            return
        }

        // 7. Only notify if urgency > lastNotifiedUrgency (escalation only)
        if let lastNotified = lastNotifiedUrgency {
            guard currentUrgency > lastNotified else {
                print("[HydrationReminder] Not escalating - current: \(currentUrgency), last: \(lastNotified)")
                return
            }
        }

        // 8. Schedule notification and update lastNotifiedUrgency (use rounded deficit for display)
        let roundedDeficit = Self.roundToNearest50(deficitMl)
        notificationManager.scheduleHydrationReminder(urgency: currentUrgency, deficitMl: roundedDeficit)
        lastNotifiedUrgency = currentUrgency
        print("[HydrationReminder] Scheduled \(currentUrgency.description) reminder - deficit=\(deficitMl)ml (rounded=\(roundedDeficit)ml)")

        // 9. Also send notification to Watch for local delivery + haptic (use rounded deficit)
        let deficitDisplay = roundedDeficit >= 1000 ? String(format: "%.1fL", Double(roundedDeficit) / 1000.0) : "\(roundedDeficit)ml"
        let title = "Hydration Reminder"
        let body = currentUrgency == .overdue
            ? "You're falling behind! Drink \(deficitDisplay) to catch up."
            : "Time to hydrate! You're \(deficitDisplay) behind pace."
        WatchConnectivityManager.shared.sendNotificationToWatch(type: "reminder", title: title, body: body)
    }

    // MARK: - Current State

    /// Build current hydration state for sharing with Watch
    func buildHydrationState() -> HydrationState {
        return HydrationState(
            dailyTotalMl: dailyTotalMl,
            dailyGoalMl: dailyGoalMl,
            lastDrinkTime: lastDrinkTime,
            urgency: currentUrgency,
            deficitMl: deficitMl,
            timestamp: Date()
        )
    }

    // MARK: - Watch Sync

    /// Sync current hydration state to Watch via SharedDataManager and WatchConnectivity
    func syncStateToWatch() {
        let state = buildHydrationState()

        // Save to shared App Group container (for complications when iPhone not reachable)
        SharedDataManager.shared.saveHydrationState(state)

        // Send to Watch via WatchConnectivity
        WatchConnectivityManager.shared.sendHydrationState(state)

        // Update complication if urgency changed significantly
        WatchConnectivityManager.shared.updateComplication(with: state)

        print("[HydrationReminder] Synced state to Watch: \(state.dailyTotalMl)ml, urgency=\(state.urgency)")
    }

    /// Sync current state from CoreData to Watch (called on app launch/active)
    /// This ensures the Watch has the latest data even without a BLE connection
    func syncCurrentStateFromCoreData(dailyGoalMl: Int) {
        // Get today's total from CoreData
        let todaysTotalMl = PersistenceController.shared.getTodaysTotalMl()

        // Get last drink time from today's records
        let todaysDrinks = PersistenceController.shared.getTodaysDrinkRecords()
        let lastDrink = todaysDrinks.first?.timestamp

        // Update state and sync to Watch
        self.dailyTotalMl = todaysTotalMl
        self.dailyGoalMl = dailyGoalMl
        self.lastDrinkTime = lastDrink
        updateUrgency()
        syncStateToWatch()

        print("[HydrationReminder] Initial sync from CoreData: \(todaysTotalMl)ml, goal=\(dailyGoalMl)ml")
    }
}
