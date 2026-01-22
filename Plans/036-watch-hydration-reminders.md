# Plan: Hydration Reminders + Apple Watch App (Issue #27)

## Summary

Add smart hydration reminder notifications and an Apple Watch companion app with complications.

**Key Design Decisions:**
- Max 8 reminders per day
- Fixed quiet hours (10pm-7am) in static constants
- 60-minute base reminder interval
- Stop reminders once daily goal achieved
- Color-coded urgency: Blue (on-track) → Amber (attention) → Red (overdue)
- Watch: Complication + minimal app with large colored water drop

---

## Phase 1: Hydration Reminder Service (iOS)

### 1.1 Create HydrationState Model

**New file:** `ios/Aquavate/Aquavate/Models/HydrationState.swift`

```swift
enum HydrationUrgency: Int, Codable {
    case onTrack = 0    // Blue: 45-60 min
    case attention = 1  // Amber: 60-90 min
    case overdue = 2    // Red: 90+ min
}

struct HydrationState: Codable {
    let dailyTotalMl: Int
    let dailyGoalMl: Int
    let lastDrinkTime: Date?
    let urgency: HydrationUrgency
    let timestamp: Date
}
```

### 1.2 Create NotificationManager Service

**New file:** `ios/Aquavate/Aquavate/Services/NotificationManager.swift`

- Wraps `UNUserNotificationCenter`
- Static quiet hours: 22:00-07:00
- Tracks reminders sent today (reset at 4am boundary)
- Max 8 reminders per day enforcement

Key methods:
- `requestAuthorization() async throws`
- `scheduleHydrationReminder(urgency:, minutesSinceDrink:)`
- `cancelAllPendingReminders()`
- `isInQuietHours() -> Bool`

### 1.3 Create HydrationReminderService

**New file:** `ios/Aquavate/Aquavate/Services/HydrationReminderService.swift`

The "brain" that decides when to send reminders:

```swift
@MainActor
class HydrationReminderService: ObservableObject {
    // Configuration constants
    static let baseIntervalMinutes = 60
    static let graceMinutes = 45        // No reminder within 45 min of drink
    static let attentionMinutes = 90    // Amber threshold
    static let maxRemindersPerDay = 8

    @Published private(set) var currentUrgency: HydrationUrgency = .onTrack
    @Published private(set) var remindersSentToday: Int = 0
    @Published private(set) var lastNotifiedUrgency: HydrationUrgency? = nil  // For escalation tracking

    func evaluateAndScheduleReminder()
    func drinkRecorded()   // Called after BLE sync, resets lastNotifiedUrgency
    func goalAchieved()    // Cancels all pending, resets lastNotifiedUrgency
}
```

**Reminder Logic:**
1. Check notifications enabled & authorized
2. Check not in quiet hours
3. Check < 8 reminders sent today
4. Check >= 45 min since last drink
5. Check daily goal not achieved
6. Calculate urgency
7. **Only notify if urgency > lastNotifiedUrgency** (escalation only)
8. Schedule notification and update lastNotifiedUrgency

**Escalation Model:**
- Once amber notification sent, no more amber until resolved
- Once red notification sent, no more red until resolved
- Amber → Red escalation still triggers notification
- `lastNotifiedUrgency` resets when:
  - Drink recorded (user hydrated)
  - New day starts (4am boundary)
  - Goal achieved

### 1.4 Wire Up Services

**Modify:** `ios/Aquavate/Aquavate/AquavateApp.swift`

```swift
@StateObject private var notificationManager = NotificationManager()
@StateObject private var hydrationReminderService: HydrationReminderService

// In body, after ContentView:
.environmentObject(notificationManager)
.environmentObject(hydrationReminderService)

// Wire up:
bleManager.hydrationReminderService = hydrationReminderService
```

**Modify:** `ios/Aquavate/Aquavate/Services/BLEManager.swift`

Add property and trigger after drink sync:
```swift
weak var hydrationReminderService: HydrationReminderService?

// In completeSyncTransfer(), after saving:
hydrationReminderService?.drinkRecorded()
```

### 1.5 Update SettingsView

**Modify:** `ios/Aquavate/Aquavate/Views/SettingsView.swift`

Replace line 13 (`@State private var notificationsEnabled = true`) with proper binding:

```swift
@EnvironmentObject var notificationManager: NotificationManager

// Line 372-380: Replace toggle with:
Toggle(isOn: $notificationManager.isEnabled) {
    HStack {
        Image(systemName: "bell.fill")
            .foregroundStyle(.purple)
        Text("Hydration Reminders")
    }
}
.onChange(of: notificationManager.isEnabled) { _, enabled in
    if enabled {
        Task { try? await notificationManager.requestAuthorization() }
    }
}
```

---

## Phase 2: App Groups for Shared Data

### 2.1 Add App Groups Entitlement

**Modify:** `ios/Aquavate/Aquavate/Aquavate.entitlements`

Add:
```xml
<key>com.apple.security.application-groups</key>
<array>
    <string>group.com.bowerhaus.Aquavate</string>
</array>
```

### 2.2 Create SharedDataManager

**New file:** `ios/Aquavate/Aquavate/Services/SharedDataManager.swift`

```swift
class SharedDataManager {
    static let shared = SharedDataManager()
    private let appGroupId = "group.com.bowerhaus.Aquavate"

    func saveHydrationState(_ state: HydrationState)
    func loadHydrationState() -> HydrationState?
}
```

---

## Phase 3: WatchConnectivity

### 3.1 Create WatchConnectivityManager (iPhone)

**New file:** `ios/Aquavate/Aquavate/Services/WatchConnectivityManager.swift`

```swift
@MainActor
class WatchConnectivityManager: NSObject, ObservableObject, WCSessionDelegate {
    static let shared = WatchConnectivityManager()

    @Published private(set) var isWatchAppInstalled = false

    func sendHydrationState(_ state: HydrationState)
    func updateComplication()
}
```

Uses:
- `updateApplicationContext(_:)` for latest state
- `transferCurrentComplicationUserInfo(_:)` for complication updates

---

## Phase 4: Apple Watch App

### 4.1 Create Watch Target

In Xcode: File > New > Target > watchOS > App
- Product Name: `AquavateWatch`
- Bundle ID: `com.bowerhaus.Aquavate.watchkitapp`
- watchOS 10.0 minimum
- Include Complications: Yes

**New directory structure:**
```
ios/Aquavate/
  AquavateWatch/
    AquavateWatchApp.swift
    ContentView.swift
    WatchSessionManager.swift
    ComplicationProvider.swift
    Assets.xcassets/
    AquavateWatch.entitlements  (with same App Group)
```

### 4.2 Watch Home View

**New file:** `AquavateWatch/ContentView.swift`

```swift
struct WatchHomeView: View {
    @ObservedObject var session = WatchSessionManager.shared

    var body: some View {
        VStack(spacing: 12) {
            // Large colored water drop
            Image(systemName: "drop.fill")
                .font(.system(size: 60))
                .foregroundColor(urgencyColor)

            // Daily progress
            Text("\(liters, specifier: "%.1f")L / \(goalLiters, specifier: "%.1f")L")
                .font(.title2)

            // Time since last drink
            if let lastDrink = session.hydrationState?.lastDrinkTime {
                Text(lastDrink, style: .relative)
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
        }
    }
}
```

### 4.3 Complication

**New file:** `AquavateWatch/ComplicationProvider.swift`

Support families:
- `graphicCircular` - Small water drop with progress ring
- `graphicCorner` - Larger drop with percentage

```swift
struct GraphicCircularView: View {
    let state: HydrationState

    var body: some View {
        Gauge(value: progress) {
            Image(systemName: "drop.fill")
                .foregroundColor(state.urgency.color)
        }
        .gaugeStyle(.accessoryCircularCapacity)
    }
}
```

### 4.4 Watch Session Manager

**New file:** `AquavateWatch/WatchSessionManager.swift`

```swift
@MainActor
class WatchSessionManager: NSObject, ObservableObject, WCSessionDelegate {
    static let shared = WatchSessionManager()
    @Published var hydrationState: HydrationState?

    // Receive applicationContext updates from iPhone
    // Trigger complication reload when state changes
}
```

---

## Phase 5: Integration

### 5.1 Update Info.plist Files

**iPhone Info.plist:**
```xml
<key>WKCompanionAppBundleIdentifier</key>
<string>com.bowerhaus.Aquavate.watchkitapp</string>
```

### 5.2 Trigger Updates

In `HydrationReminderService`, after state changes:
```swift
SharedDataManager.shared.saveHydrationState(state)
WatchConnectivityManager.shared.sendHydrationState(state)
```

---

## Files Summary

### New Files (8)
| File | Purpose |
|------|---------|
| `Models/HydrationState.swift` | Shared urgency/state model |
| `Services/NotificationManager.swift` | UNUserNotificationCenter wrapper |
| `Services/HydrationReminderService.swift` | Reminder scheduling logic |
| `Services/SharedDataManager.swift` | App Groups data access |
| `Services/WatchConnectivityManager.swift` | WCSession (iPhone side) |
| `AquavateWatch/` | Entire Watch app target |

### Modified Files (4)
| File | Changes |
|------|---------|
| `AquavateApp.swift` | Add new services, wire up |
| `BLEManager.swift` | Add reminder service ref, trigger after sync |
| `SettingsView.swift` | Wire up notification toggle (line 373) |
| `Aquavate.entitlements` | Add App Groups |

---

## Configuration Constants

```swift
// HydrationReminderService
static let baseIntervalMinutes = 60
static let graceMinutes = 45
static let attentionMinutes = 90
static let maxRemindersPerDay = 8

// NotificationManager
static let quietHoursStart = 22  // 10pm
static let quietHoursEnd = 7     // 7am

// Test mode (DEBUG only) - shorter intervals for testing
#if DEBUG
static let testModeIntervalMinutes = 1      // 1 min instead of 60
static let testModeGraceMinutes = 0         // No grace period
static let testModeAttentionMinutes = 2     // 2 min for amber
#endif
```

---

## Test Mode

For development/testing, a `testModeEnabled` flag shortens all intervals:

| Setting | Normal | Test Mode |
|---------|--------|-----------|
| Base interval | 60 min | 1 min |
| Grace period | 45 min | 0 min |
| Attention threshold | 90 min | 2 min |
| Overdue threshold | 90+ min | 2+ min |

**Toggle in Settings (DEBUG only):**
```swift
#if DEBUG
Section("Developer") {
    Toggle("Test Mode (Fast Reminders)", isOn: $hydrationReminderService.testModeEnabled)
}
#endif
```

---

## Haptic-Only Notifications

**Important:** True haptic-only (no sound) only works reliably on Watch.

| Platform | Haptic-Only Support |
|----------|---------------------|
| **Apple Watch** | Yes - `WKInterfaceDevice.current().play(.notification)` delivers haptic tap without sound |
| **iPhone** | No - haptic is bundled with sound; silent notification = no haptic |

**Implementation:**
- Watch notifications use haptic-only delivery
- iPhone notifications are secondary (user can mute in iOS settings)
- Primary feedback is the Watch haptic + complication color change

---

## Verification

1. **Notifications**: Enable toggle in Settings, wait 60+ min, verify haptic reminder
2. **Quiet hours**: Set time to 11pm, verify no reminders scheduled
3. **Max reminders**: Manually trigger 8, verify 9th is blocked
4. **Goal achievement**: Reach 2L, verify pending reminders cancelled
5. **Watch complication**: Add to watch face, verify color changes with urgency
6. **Watch app**: Open on watch, verify large drop and progress display
7. **Watch sync**: Take drink on bottle, verify watch updates within seconds
