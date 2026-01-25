# Plan: Fix Hydration Reminder Notifications (Issue #56)

## Problem

Hydration reminder notifications are scheduled correctly (reminder count increments, logs show success), but **no banner appears on the phone** when the app is in foreground.

## Root Cause

iOS does not display notification banners when the app is in foreground by default. The app must explicitly implement `UNUserNotificationCenterDelegate` and return presentation options.

**Current state of `NotificationManager`:**
- Does NOT inherit from `NSObject`
- Does NOT conform to `UNUserNotificationCenterDelegate`
- Does NOT set the delegate on `UNUserNotificationCenter.current()`
- Does NOT implement `willPresent` delegate method

## Solution

Modify `NotificationManager` to handle foreground notification presentation.

## Changes

### File: [NotificationManager.swift](ios/Aquavate/Aquavate/Services/NotificationManager.swift)

**1. Update class declaration (line 11-12)**

```swift
// From:
@MainActor
class NotificationManager: ObservableObject {

// To:
@MainActor
class NotificationManager: NSObject, ObservableObject, UNUserNotificationCenterDelegate {
```

**2. Refactor `init()` (lines 59-69)**

NSObject subclasses require `super.init()`. Published properties must be set after super.init(), so load UserDefaults values first:

```swift
override init() {
    // Load from UserDefaults before super.init()
    let isEnabledValue = UserDefaults.standard.bool(forKey: "hydrationRemindersEnabled")
    let backOnTrackValue = UserDefaults.standard.bool(forKey: "backOnTrackNotificationsEnabled")
    let dailyLimitValue = UserDefaults.standard.object(forKey: "dailyReminderLimitEnabled") as? Bool ?? true
    let remindersValue = UserDefaults.standard.integer(forKey: "remindersSentToday")
    let lastResetValue = UserDefaults.standard.object(forKey: "lastReminderResetDate") as? Date

    super.init()

    // Assign to properties after super.init()
    self.isEnabled = isEnabledValue
    self.backOnTrackEnabled = backOnTrackValue
    self.dailyLimitEnabled = dailyLimitValue
    self.remindersSentToday = remindersValue
    self.lastResetDate = lastResetValue

    // Set delegate for foreground presentation
    notificationCenter.delegate = self

    checkAuthorizationStatus()
    checkDailyReset()
}
```

**3. Add delegate methods (after line 285)**

```swift
// MARK: - UNUserNotificationCenterDelegate

extension NotificationManager {

    /// Present notifications when app is in foreground
    nonisolated func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        willPresent notification: UNNotification,
        withCompletionHandler completionHandler: @escaping (UNNotificationPresentationOptions) -> Void
    ) {
        completionHandler([.banner, .sound, .badge])

        Task { @MainActor in
            print("[Notifications] Foreground notification presented: \(notification.request.identifier)")
        }
    }

    /// Handle notification tap
    nonisolated func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        didReceive response: UNNotificationResponse,
        withCompletionHandler completionHandler: @escaping () -> Void
    ) {
        let identifier = response.notification.request.identifier

        Task { @MainActor in
            print("[Notifications] User tapped notification: \(identifier)")
            // Future: Navigate based on notification type
        }

        completionHandler()
    }
}
```

## Key Design Points

- **`NSObject` inheritance**: Required for `UNUserNotificationCenterDelegate` (Objective-C protocol)
- **`nonisolated` methods**: Delegate callbacks come from arbitrary threads
- **`Task { @MainActor in }`**: Safe pattern for main-thread access (matches BLEManager)
- **Delegate lifetime**: NotificationManager is a `@StateObject` in AquavateApp, so it persists for app lifetime

## Verification

1. **Build the app** - Ensure no compilation errors
2. **Foreground test**: Keep app in foreground, wait for a reminder to trigger, verify banner appears
3. **Background test**: Background the app, verify notifications still work
4. **Tap test**: Tap a notification, verify log message appears
