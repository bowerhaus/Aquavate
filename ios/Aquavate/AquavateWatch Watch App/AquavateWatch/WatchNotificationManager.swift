//
//  WatchNotificationManager.swift
//  AquavateWatch
//
//  Created by Claude Code on 23/01/2026.
//

import UserNotifications
import WatchKit

/// Manages local notifications on the Apple Watch
/// iPhone sends notification requests via WatchConnectivity, and this manager schedules them locally
class WatchNotificationManager {
    static let shared = WatchNotificationManager()

    private let notificationCenter = UNUserNotificationCenter.current()

    // MARK: - Authorization

    /// Request notification authorization from the user
    func requestAuthorization() async throws {
        let granted = try await notificationCenter.requestAuthorization(options: [.alert, .sound])
        print("[WatchNotification] Authorization \(granted ? "granted" : "denied")")
    }

    // MARK: - Scheduling

    /// Schedule a local notification on the Watch
    /// - Parameters:
    ///   - type: Notification type ("reminder" or "goalReached")
    ///   - title: Notification title
    ///   - body: Notification body text
    func scheduleNotification(type: String, title: String, body: String) {
        let content = UNMutableNotificationContent()
        content.title = title
        content.body = body
        content.sound = .default

        // Schedule immediately (1 second delay)
        let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 1, repeats: false)
        let identifier = "hydration-watch-\(type)-\(UUID().uuidString)"
        let request = UNNotificationRequest(identifier: identifier, content: content, trigger: trigger)

        notificationCenter.add(request) { error in
            if let error = error {
                print("[WatchNotification] Failed to schedule: \(error.localizedDescription)")
            } else {
                print("[WatchNotification] Scheduled: \(title)")
                // Play haptic feedback
                WKInterfaceDevice.current().play(.notification)
            }
        }
    }
}
