//
//  HydrationReminderService.swift
//  Aquavate
//
//  Created by Claude Code on 22/01/2026.
//

import Foundation

@MainActor
class HydrationReminderService: ObservableObject {
    // MARK: - Configuration Constants

    static let baseIntervalMinutes = 60       // Base reminder interval
    static let graceMinutes = 45              // No reminder within 45 min of drink
    static let attentionMinutes = 90          // Amber threshold (90 min)
    static let maxRemindersPerDay = 8

    #if DEBUG
    static let testModeIntervalMinutes = 1    // 1 min instead of 60
    static let testModeGraceMinutes = 0       // No grace period
    static let testModeAttentionMinutes = 2   // 2 min for amber
    #endif

    // MARK: - Published Properties

    @Published private(set) var currentUrgency: HydrationUrgency = .onTrack
    @Published private(set) var lastNotifiedUrgency: HydrationUrgency? = nil
    @Published private(set) var lastDrinkTime: Date? = nil
    @Published private(set) var dailyTotalMl: Int = 0
    @Published private(set) var dailyGoalMl: Int = 2500

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

    // MARK: - Configuration

    private var graceMinutes: Int {
        #if DEBUG
        return testModeEnabled ? Self.testModeGraceMinutes : Self.graceMinutes
        #else
        return Self.graceMinutes
        #endif
    }

    private var attentionMinutes: Int {
        #if DEBUG
        return testModeEnabled ? Self.testModeAttentionMinutes : Self.attentionMinutes
        #else
        return Self.attentionMinutes
        #endif
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
        lastNotifiedUrgency = nil  // Reset escalation tracking
        currentUrgency = .onTrack
        print("[HydrationReminder] Drink recorded - urgency reset to onTrack")
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
        guard let lastDrink = lastDrinkTime else {
            currentUrgency = .onTrack
            return
        }

        let minutesSinceDrink = Int(-lastDrink.timeIntervalSinceNow / 60)

        if minutesSinceDrink >= attentionMinutes {
            currentUrgency = .overdue
        } else if minutesSinceDrink >= graceMinutes {
            currentUrgency = .attention
        } else {
            currentUrgency = .onTrack
        }
    }

    // MARK: - Reminder Evaluation

    /// Start periodic evaluation (every minute)
    private func startPeriodicEvaluation() {
        evaluationTimer?.invalidate()
        evaluationTimer = Timer.scheduledTimer(withTimeInterval: 60, repeats: true) { [weak self] _ in
            Task { @MainActor in
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

        // 2. Check not in quiet hours
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

        // 5. Calculate time since last drink
        let minutesSinceDrink: Int
        if let lastDrink = lastDrinkTime {
            minutesSinceDrink = Int(-lastDrink.timeIntervalSinceNow / 60)
        } else {
            // No drink recorded today - treat as attention
            minutesSinceDrink = graceMinutes + 1
        }

        // 6. Check >= grace period since last drink
        guard minutesSinceDrink >= graceMinutes else {
            print("[HydrationReminder] Still in grace period (\(minutesSinceDrink)/\(graceMinutes) min)")
            return
        }

        // 7. Calculate urgency
        updateUrgency()

        // 8. Only notify if urgency > lastNotifiedUrgency (escalation only)
        if let lastNotified = lastNotifiedUrgency {
            guard currentUrgency > lastNotified else {
                print("[HydrationReminder] Not escalating - current: \(currentUrgency), last: \(lastNotified)")
                return
            }
        }

        // 9. Schedule notification and update lastNotifiedUrgency
        notificationManager.scheduleHydrationReminder(urgency: currentUrgency, minutesSinceDrink: minutesSinceDrink)
        lastNotifiedUrgency = currentUrgency
        print("[HydrationReminder] Scheduled \(currentUrgency.description) reminder")
    }

    // MARK: - Current State

    /// Build current hydration state for sharing with Watch
    func buildHydrationState() -> HydrationState {
        return HydrationState(
            dailyTotalMl: dailyTotalMl,
            dailyGoalMl: dailyGoalMl,
            lastDrinkTime: lastDrinkTime,
            urgency: currentUrgency,
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
