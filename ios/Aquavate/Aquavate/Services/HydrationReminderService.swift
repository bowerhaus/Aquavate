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
    static let maxRemindersPerDay = 8

    #if DEBUG
    static let testModeAttentionThreshold = 0.05  // 5% behind triggers amber/red in test mode
    #endif

    // MARK: - Published Properties

    @Published private(set) var currentUrgency: HydrationUrgency = .onTrack
    @Published private(set) var lastNotifiedUrgency: HydrationUrgency? = nil
    @Published private(set) var lastDrinkTime: Date? = nil
    @Published private(set) var dailyTotalMl: Int = 0
    @Published private(set) var dailyGoalMl: Int = 2500
    @Published private(set) var deficitMl: Int = 0

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
        startPeriodicEvaluation()
    }

    deinit {
        evaluationTimer?.invalidate()
    }

    // MARK: - Pace-Based Configuration

    private var attentionThreshold: Double {
        #if DEBUG
        return testModeEnabled ? Self.testModeAttentionThreshold : Self.attentionThreshold
        #else
        return Self.attentionThreshold
        #endif
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

    /// Called after a drink is recorded (via BLE sync)
    func drinkRecorded() {
        lastDrinkTime = Date()

        // Recalculate urgency - may still be behind pace even after drinking
        let previousUrgency = currentUrgency
        updateUrgency()

        // Reset escalation tracking if urgency improved
        if currentUrgency < previousUrgency {
            lastNotifiedUrgency = nil
        }

        print("[HydrationReminder] Drink recorded - deficit=\(deficitMl)ml, urgency=\(currentUrgency)")
    }

    /// Called when daily goal is achieved
    func goalAchieved() {
        lastNotifiedUrgency = nil
        notificationManager?.cancelAllPendingReminders()
        print("[HydrationReminder] Goal achieved - reminders cancelled")
    }

    /// Called at 4am daily reset
    func dailyReset() {
        lastNotifiedUrgency = nil
        dailyTotalMl = 0
        currentUrgency = .onTrack
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

        // Behind pace - check threshold
        let redThresholdMl = Int(Double(dailyGoalMl) * attentionThreshold)
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

        // 6. Only notify if behind pace (deficit > 0) and not already notified at this level
        guard deficitMl > 0 else {
            print("[HydrationReminder] On pace - no reminder needed")
            return
        }

        // 7. Only notify if urgency > lastNotifiedUrgency (escalation only)
        if let lastNotified = lastNotifiedUrgency {
            guard currentUrgency > lastNotified else {
                print("[HydrationReminder] Not escalating - current: \(currentUrgency), last: \(lastNotified)")
                return
            }
        }

        // 8. Schedule notification and update lastNotifiedUrgency
        notificationManager.scheduleHydrationReminder(urgency: currentUrgency, deficitMl: deficitMl)
        lastNotifiedUrgency = currentUrgency
        print("[HydrationReminder] Scheduled \(currentUrgency.description) reminder - deficit=\(deficitMl)ml")
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
