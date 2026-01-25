//
//  NotificationManager.swift
//  Aquavate
//
//  Created by Claude Code on 22/01/2026.
//

import Foundation
import UserNotifications

@MainActor
class NotificationManager: ObservableObject {
    // MARK: - Constants

    static let quietHoursStart = 22  // 10pm
    static let quietHoursEnd = 7     // 7am
    static let maxRemindersPerDay = 12
    static let dailyResetHour = 4    // 4am boundary (matches drink tracking)

    // MARK: - Published Properties

    @Published var isAuthorized = false
    @Published var authorizationStatus: UNAuthorizationStatus = .notDetermined

    /// User preference for whether hydration reminders are enabled
    @Published var isEnabled: Bool {
        didSet {
            UserDefaults.standard.set(isEnabled, forKey: "hydrationRemindersEnabled")
            if !isEnabled {
                cancelAllPendingReminders()
            }
        }
    }

    /// User preference for back-on-track notifications
    @Published var backOnTrackEnabled: Bool {
        didSet {
            UserDefaults.standard.set(backOnTrackEnabled, forKey: "backOnTrackNotificationsEnabled")
        }
    }

    /// User preference for enforcing daily reminder limit
    @Published var dailyLimitEnabled: Bool {
        didSet {
            UserDefaults.standard.set(dailyLimitEnabled, forKey: "dailyReminderLimitEnabled")
        }
    }

    /// Number of reminders sent today (resets at 4am)
    @Published private(set) var remindersSentToday: Int = 0

    // MARK: - Private Properties

    private let notificationCenter = UNUserNotificationCenter.current()
    private var lastResetDate: Date?

    // MARK: - Initialization

    init() {
        self.isEnabled = UserDefaults.standard.bool(forKey: "hydrationRemindersEnabled")
        self.backOnTrackEnabled = UserDefaults.standard.bool(forKey: "backOnTrackNotificationsEnabled")
        self.dailyLimitEnabled = UserDefaults.standard.object(forKey: "dailyReminderLimitEnabled") as? Bool ?? true
        self.remindersSentToday = UserDefaults.standard.integer(forKey: "remindersSentToday")
        if let lastReset = UserDefaults.standard.object(forKey: "lastReminderResetDate") as? Date {
            self.lastResetDate = lastReset
        }
        checkAuthorizationStatus()
        checkDailyReset()
    }

    // MARK: - Authorization

    /// Check current notification authorization status
    func checkAuthorizationStatus() {
        Task {
            let settings = await notificationCenter.notificationSettings()
            await MainActor.run {
                self.authorizationStatus = settings.authorizationStatus
                self.isAuthorized = settings.authorizationStatus == .authorized
            }
        }
    }

    /// Request notification authorization
    func requestAuthorization() async throws {
        let granted = try await notificationCenter.requestAuthorization(options: [.alert, .sound, .badge])
        await MainActor.run {
            self.isAuthorized = granted
            self.authorizationStatus = granted ? .authorized : .denied
        }
        print("[Notifications] Authorization \(granted ? "granted" : "denied")")
    }

    // MARK: - Quiet Hours

    /// Check if current time is within quiet hours (10pm - 7am)
    func isInQuietHours() -> Bool {
        let calendar = Calendar.current
        let hour = calendar.component(.hour, from: Date())

        // Quiet hours: 22:00 (10pm) to 07:00 (7am)
        // This means: hour >= 22 OR hour < 7
        return hour >= Self.quietHoursStart || hour < Self.quietHoursEnd
    }

    // MARK: - Daily Reset

    /// Check if we need to reset the daily reminder count (at 4am boundary)
    private func checkDailyReset() {
        let now = Date()
        let calendar = Calendar.current

        // Calculate today's 4am boundary
        var components = calendar.dateComponents([.year, .month, .day], from: now)
        components.hour = Self.dailyResetHour
        components.minute = 0
        components.second = 0
        guard let todayBoundary = calendar.date(from: components) else { return }

        // If now is before 4am, use yesterday's 4am as the boundary
        let effectiveBoundary: Date
        if now < todayBoundary {
            effectiveBoundary = calendar.date(byAdding: .day, value: -1, to: todayBoundary)!
        } else {
            effectiveBoundary = todayBoundary
        }

        // Reset if last reset was before the effective boundary
        if let lastReset = lastResetDate, lastReset < effectiveBoundary {
            resetDailyCount()
        } else if lastResetDate == nil {
            resetDailyCount()
        }
    }

    private func resetDailyCount() {
        remindersSentToday = 0
        lastResetDate = Date()
        UserDefaults.standard.set(remindersSentToday, forKey: "remindersSentToday")
        UserDefaults.standard.set(lastResetDate, forKey: "lastReminderResetDate")
        print("[Notifications] Daily reminder count reset")
    }

    // MARK: - Scheduling

    /// Check if we can send more reminders today
    var canSendReminder: Bool {
        checkDailyReset()
        guard dailyLimitEnabled else { return true }
        return remindersSentToday < Self.maxRemindersPerDay
    }

    /// Schedule a hydration reminder notification
    /// - Parameters:
    ///   - urgency: The urgency level for the notification
    ///   - deficitMl: How many ml behind pace (for message customization)
    func scheduleHydrationReminder(urgency: HydrationUrgency, deficitMl: Int) {
        guard isEnabled && isAuthorized else {
            print("[Notifications] Cannot schedule - enabled: \(isEnabled), authorized: \(isAuthorized)")
            return
        }

        guard !isInQuietHours() else {
            print("[Notifications] Cannot schedule - in quiet hours")
            return
        }

        guard canSendReminder else {
            print("[Notifications] Cannot schedule - max reminders (\(Self.maxRemindersPerDay)) reached")
            return
        }

        let content = UNMutableNotificationContent()
        content.title = "Hydration Reminder"
        content.sound = .default

        // Format deficit for display
        let deficitDisplay = deficitMl >= 1000
            ? String(format: "%.1fL", Double(deficitMl) / 1000.0)
            : "\(deficitMl)ml"

        switch urgency {
        case .onTrack:
            content.body = "Stay hydrated! Keep up the good pace."
        case .attention:
            content.body = "Time to hydrate! You're \(deficitDisplay) behind pace."
        case .overdue:
            content.body = "You're falling behind! Drink \(deficitDisplay) to catch up."
        }

        // Schedule immediately
        let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 1, repeats: false)
        let request = UNNotificationRequest(
            identifier: "hydration-\(UUID().uuidString)",
            content: content,
            trigger: trigger
        )

        notificationCenter.add(request) { error in
            if let error = error {
                print("[Notifications] Failed to schedule: \(error.localizedDescription)")
            } else {
                Task { @MainActor in
                    self.remindersSentToday += 1
                    UserDefaults.standard.set(self.remindersSentToday, forKey: "remindersSentToday")
                    print("[Notifications] Scheduled \(urgency.description) reminder (\(self.remindersSentToday)/\(Self.maxRemindersPerDay) today)")
                }
            }
        }
    }

    /// Schedule a goal reached celebration notification
    func scheduleGoalReachedNotification() {
        guard isEnabled && isAuthorized else {
            print("[Notifications] Cannot schedule goal notification - enabled: \(isEnabled), authorized: \(isAuthorized)")
            return
        }

        let content = UNMutableNotificationContent()
        content.title = "Goal Reached! 💧"
        content.body = "Good job! You've hit your daily hydration goal."
        content.sound = .default

        // Schedule immediately
        let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 1, repeats: false)
        let request = UNNotificationRequest(
            identifier: "hydration-goal-reached",
            content: content,
            trigger: trigger
        )

        notificationCenter.add(request) { error in
            if let error = error {
                print("[Notifications] Failed to schedule goal notification: \(error.localizedDescription)")
            } else {
                print("[Notifications] Scheduled goal reached notification")
            }
        }
    }

    /// Schedule a back-on-track celebration notification
    func scheduleBackOnTrackNotification() {
        guard isEnabled && isAuthorized && backOnTrackEnabled else {
            print("[Notifications] Cannot schedule back-on-track notification - enabled: \(isEnabled), authorized: \(isAuthorized), backOnTrack: \(backOnTrackEnabled)")
            return
        }

        let content = UNMutableNotificationContent()
        content.title = "Back On Track!"
        content.body = "Nice work catching up on your hydration."
        content.sound = .default

        // Schedule immediately
        let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 1, repeats: false)
        let request = UNNotificationRequest(
            identifier: "hydration-back-on-track-\(UUID().uuidString)",
            content: content,
            trigger: trigger
        )

        notificationCenter.add(request) { error in
            if let error = error {
                print("[Notifications] Failed to schedule back-on-track notification: \(error.localizedDescription)")
            } else {
                print("[Notifications] Scheduled back-on-track notification")
            }
        }
    }

    /// Cancel all pending hydration reminder notifications
    func cancelAllPendingReminders() {
        Task {
            let requests = await notificationCenter.pendingNotificationRequests()
            let hydrationRequests = requests.filter { $0.identifier.hasPrefix("hydration-") }
            let identifiers = hydrationRequests.map { $0.identifier }
            notificationCenter.removePendingNotificationRequests(withIdentifiers: identifiers)
            print("[Notifications] Cancelled \(identifiers.count) pending reminders")
        }
    }

    /// Reset the daily reminder count (called when goal achieved or new day)
    func resetReminderCount() {
        resetDailyCount()
    }
}
