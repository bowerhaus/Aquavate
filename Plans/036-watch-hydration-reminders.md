# Plan: Hydration Reminders + Apple Watch App (Issue #27)

## Summary

Add smart hydration reminder notifications and an Apple Watch companion app with complications.

**Key Design Decisions:**
- Max 8 reminders per day
- Fixed quiet hours (10pm-7am) - also defines active hydration hours (7am-10pm)
- **Pace-based urgency model** - tracks whether you're on schedule to meet daily goal
- Stop reminders once daily goal achieved
- Color-coded urgency: Blue (on-track) → Amber (attention) → Red (overdue)
- Watch: Complication + minimal app with large colored water drop + "Xml to catch up"

---

## Pace-Based Hydration Model

### Overview

Instead of tracking "time since last drink" (which just requires any drink to reset), we track whether you're **on pace to meet your daily goal**. This provides more meaningful feedback about hydration status.

### Active Hours
- **Start:** 7am (matches quiet hours end)
- **End:** 10pm (matches quiet hours start)
- **Duration:** 15 hours of active hydration time

### Calculations

```
Expected intake = (elapsed active hours / 15) × dailyGoalMl
Deficit = expected - dailyTotalMl
```

**Example (2500ml daily goal):**
- At 2pm (7 hours into active period): Expected = (7/15) × 2500 = 1167ml
- If you've drunk 800ml: Deficit = 1167 - 800 = 367ml behind

### Urgency Mapping

| Urgency | Condition | Example (2500ml goal) |
|---------|-----------|----------------------|
| Blue (onTrack) | deficit ≤ 0 | On pace or ahead |
| Amber (attention) | 0 < deficit < 20% of goal | 1-499ml behind |
| Red (overdue) | deficit ≥ 20% of goal | 500ml+ behind |

### Edge Cases

| Scenario | Behavior |
|----------|----------|
| Before 7am | Always on track (deficit = 0) |
| After 10pm | Compare against full daily goal |
| Goal achieved | Always on track, deficit = 0 |
| Over goal | Negative deficit (surplus) |

### Watch Display

When deficit > 0:
```
     💧 (colored drop)
   1.2L / 2.5L
   367ml to catch up
```

When on track or ahead:
```
     💧 (blue drop)
   1.2L / 2.5L
   On track ✓
```

When goal reached:
```
     💧 (blue drop)
   2.5L / 2.5L
   Goal reached! 🎉
```

---

## Phase 1: Hydration Reminder Service (iOS)

### 1.1 Create HydrationState Model

**New file:** `ios/Aquavate/Aquavate/Models/HydrationState.swift`

```swift
enum HydrationUrgency: Int, Codable {
    case onTrack = 0    // Blue: on pace or ahead
    case attention = 1  // Amber: 1-19% behind pace
    case overdue = 2    // Red: 20%+ behind pace
}

struct HydrationState: Codable {
    let dailyTotalMl: Int
    let dailyGoalMl: Int
    let lastDrinkTime: Date?
    let urgency: HydrationUrgency
    let deficitMl: Int          // How much behind pace (0 if on track, negative if ahead)
    let timestamp: Date

    var progress: Double { ... }
    var isGoalAchieved: Bool { ... }
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

The "brain" that decides when to send reminders using pace-based urgency:

```swift
@MainActor
class HydrationReminderService: ObservableObject {
    // Pace-based configuration constants
    static let activeHoursStart = 7      // 7am
    static let activeHoursEnd = 22       // 10pm
    static let activeHoursDuration = 15  // hours
    static let attentionThreshold = 0.20 // 20% behind = amber/red boundary
    static let maxRemindersPerDay = 8

    @Published private(set) var currentUrgency: HydrationUrgency = .onTrack
    @Published private(set) var deficitMl: Int = 0
    @Published private(set) var lastNotifiedUrgency: HydrationUrgency? = nil

    func calculateExpectedIntake() -> Int    // Based on time of day
    func calculateDeficit() -> Int           // expected - actual
    func updateUrgency()                     // Maps deficit to urgency level
    func evaluateAndScheduleReminder()
    func drinkRecorded()   // Recalculates deficit, may still be behind
    func goalAchieved()    // Cancels all pending
    func syncCurrentStateFromCoreData(dailyGoalMl:)  // Initial sync on app launch
}
```

**Pace Calculation:**
```swift
func calculateExpectedIntake() -> Int {
    let now = Date()
    let hour = Calendar.current.component(.hour, from: now)

    // Before active hours: expect 0
    if hour < activeHoursStart { return 0 }

    // After active hours: expect full goal
    if hour >= activeHoursEnd { return dailyGoalMl }

    // During active hours: proportional
    let elapsedHours = hour - activeHoursStart
    return (elapsedHours * dailyGoalMl) / activeHoursDuration
}
```

**Urgency Mapping:**
```swift
func updateUrgency() {
    let deficit = calculateDeficit()
    self.deficitMl = max(0, deficit)  // Don't show negative

    if dailyTotalMl >= dailyGoalMl {
        currentUrgency = .onTrack  // Goal achieved
    } else if deficit <= 0 {
        currentUrgency = .onTrack  // On pace or ahead
    } else if deficit < Int(Double(dailyGoalMl) * attentionThreshold) {
        currentUrgency = .attention  // Behind but < 20%
    } else {
        currentUrgency = .overdue    // 20%+ behind
    }
}
```

**Reminder Logic:**
1. Check notifications enabled & authorized
2. Check not in quiet hours (also means we're in active hours)
3. Check < 8 reminders sent today
4. Check daily goal not achieved
5. Calculate deficit and urgency
6. **Only notify if urgency > lastNotifiedUrgency** (escalation only)
7. Schedule notification and update lastNotifiedUrgency

**Escalation Model:**
- Once amber notification sent, no more amber until urgency improves
- Once red notification sent, no more red until urgency improves
- Amber → Red escalation still triggers notification
- `lastNotifiedUrgency` resets when:
  - Urgency improves (user catches up)
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
                .font(.title3)

            // Deficit or status message
            statusText
                .font(.caption)
                .foregroundColor(.secondary)

            // Time since last drink
            if let lastDrink = session.hydrationState?.lastDrinkTime {
                Text(lastDrink, style: .relative)
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
        }
    }

    @ViewBuilder
    private var statusText: some View {
        if let state = session.hydrationState {
            if state.isGoalAchieved {
                Text("Goal reached! 🎉")
            } else if state.deficitMl > 0 {
                Text("\(state.deficitMl)ml to catch up")
            } else {
                Text("On track ✓")
            }
        } else {
            Text("--")
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

## Phase 6: Pace-Based Urgency Model

Migrate from time-based urgency (minutes since last drink) to pace-based urgency (deficit from expected intake).

### 6.1 Update HydrationState Model

**Modify:** `ios/Aquavate/Aquavate/Models/HydrationState.swift`

Add deficit field:
```swift
struct HydrationState: Codable {
    let dailyTotalMl: Int
    let dailyGoalMl: Int
    let lastDrinkTime: Date?
    let urgency: HydrationUrgency
    let deficitMl: Int          // NEW: How much behind pace (0 if on track)
    let timestamp: Date
}
```

### 6.2 Update HydrationReminderService

**Modify:** `ios/Aquavate/Aquavate/Services/HydrationReminderService.swift`

Replace time-based constants with pace-based:
```swift
// Remove these:
// static let graceMinutes = 45
// static let attentionMinutes = 90

// Add these:
static let activeHoursStart = 7
static let activeHoursEnd = 22
static let activeHoursDuration = 15
static let attentionThreshold = 0.20  // 20% of goal
```

Add pace calculation methods:
```swift
func calculateExpectedIntake() -> Int
func calculateDeficit() -> Int
```

Update `updateUrgency()` to use deficit-based logic instead of time-based.

Update `buildHydrationState()` to include `deficitMl`.

### 6.3 Update Watch ContentView

**Modify:** `ios/Aquavate/AquavateWatch/ContentView.swift`

Replace percentage display with status text:
- "Xml to catch up" when deficit > 0
- "On track ✓" when on pace or ahead
- "Goal reached! 🎉" when goal achieved

### 6.4 Initial Sync on App Launch

**Modify:** `ios/Aquavate/Aquavate/AquavateApp.swift`

Add call to sync current CoreData state to Watch on app launch and when app becomes active:
```swift
hydrationReminderService.syncCurrentStateFromCoreData(dailyGoalMl: bleManager.dailyGoalMl)
```

### 6.5 Periodic Watch Sync

**Modify:** `ios/Aquavate/Aquavate/Services/HydrationReminderService.swift`

The existing 60-second periodic evaluation timer should also sync to Watch, so the deficit updates in near-real-time as time passes:

```swift
private func startPeriodicEvaluation() {
    evaluationTimer = Timer.scheduledTimer(withTimeInterval: 60, repeats: true) { [weak self] _ in
        Task { @MainActor in
            self?.evaluateAndScheduleReminder()
            self?.updateUrgency()       // Recalculate deficit based on current time
            self?.syncStateToWatch()    // Push updated state to Watch
        }
    }
}
```

**Why this matters:** With pace-based urgency, the deficit increases as time passes even without any drinks. Without periodic sync, the Watch would show stale deficit values until the next app activation or BLE sync.

**Watch Update Triggers (Complete List):**
| Trigger | Frequency | Method |
|---------|-----------|--------|
| App launch | Once | `syncCurrentStateFromCoreData()` |
| App becomes active | Each time | `syncCurrentStateFromCoreData()` |
| BLE drink sync | After sync | `updateState()` → `syncStateToWatch()` |
| Periodic timer | Every 60s | `updateUrgency()` → `syncStateToWatch()` |

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

### Modified Files (5)
| File | Changes |
|------|---------|
| `AquavateApp.swift` | Add services, BGAppRefreshTask registration, background task handler |
| `BLEManager.swift` | Add reminder service ref, trigger after sync |
| `SettingsView.swift` | Wire up notification toggle (line 373) |
| `Aquavate.entitlements` | Add App Groups |
| `Info.plist` | Add `fetch` background mode, BGTaskSchedulerPermittedIdentifiers |

---

## Configuration Constants

```swift
// HydrationReminderService - Pace-based model
static let activeHoursStart = 7           // 7am
static let activeHoursEnd = 22            // 10pm
static let activeHoursDuration = 15       // hours
static let attentionThreshold = 0.20      // 20% behind = red threshold
static let maxRemindersPerDay = 8

// NotificationManager
static let quietHoursStart = 22  // 10pm (matches active hours end)
static let quietHoursEnd = 7     // 7am (matches active hours start)

// Test mode (DEBUG only) - lower thresholds for testing
#if DEBUG
static let testModeAttentionThreshold = 0.05  // 5% behind triggers amber/red
#endif
```

---

## Test Mode

For development/testing, a `testModeEnabled` flag lowers the threshold so you can see urgency changes more easily:

| Setting | Normal | Test Mode |
|---------|--------|-----------|
| Attention/Red threshold | 20% behind | 5% behind |

**Example (2500ml goal):**
- Normal: Need to be 500ml behind to trigger red
- Test mode: Only 125ml behind triggers red

**Toggle in Settings (DEBUG only):**
```swift
#if DEBUG
Section("Developer") {
    Toggle("Test Mode (Sensitive Urgency)", isOn: $hydrationReminderService.testModeEnabled)
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

## Phase 7: Background Notifications

Enable notification delivery when the iPhone app is backgrounded using BGAppRefreshTask.

### 7.1 Register Background Task

**Modify:** `ios/Aquavate/Aquavate/AquavateApp.swift`

```swift
import BackgroundTasks

struct AquavateApp: App {
    static let hydrationCheckTaskIdentifier = "com.bowerhaus.Aquavate.hydrationCheck"

    init() {
        registerBackgroundTasks()
    }

    private func registerBackgroundTasks() {
        BGTaskScheduler.shared.register(
            forTaskWithIdentifier: Self.hydrationCheckTaskIdentifier,
            using: nil
        ) { task in
            self.handleHydrationCheck(task: task as! BGAppRefreshTask)
        }
    }
}
```

### 7.2 Schedule on App Background

**Modify:** `AquavateApp.handleScenePhaseChange()`

```swift
case .background:
    bleManager.appDidEnterBackground()
    scheduleHydrationCheck()  // NEW

private func scheduleHydrationCheck() {
    let request = BGAppRefreshTaskRequest(identifier: Self.hydrationCheckTaskIdentifier)
    request.earliestBeginDate = Date(timeIntervalSinceNow: 15 * 60)  // ~15 min
    try? BGTaskScheduler.shared.submit(request)
}
```

### 7.3 Handle Background Task

```swift
private func handleHydrationCheck(task: BGAppRefreshTask) {
    // Schedule next check
    scheduleHydrationCheck()

    Task { @MainActor in
        // Get state from CoreData
        let todaysTotalMl = PersistenceController.shared.getTodaysTotalMl()
        let dailyGoalMl = UserDefaults.standard.integer(forKey: "dailyGoalMl")
        let effectiveGoal = dailyGoalMl > 0 ? dailyGoalMl : 2500

        // Calculate deficit
        let deficit = calculateDeficit(totalMl: todaysTotalMl, goalMl: effectiveGoal)

        // Check goal reached first
        if todaysTotalMl >= effectiveGoal && !goalNotificationSent {
            notificationManager.scheduleGoalReachedNotification()
            // ... mark sent
        }
        // Check if behind pace
        else if shouldSendBackgroundNotification(...) {
            notificationManager.scheduleHydrationReminder(urgency: urgency, deficitMl: deficit)
        }

        task.setTaskCompleted(success: true)
    }
}
```

### 7.4 Update Info.plist

**Modify:** `ios/Aquavate/Info.plist`

```xml
<key>UIBackgroundModes</key>
<array>
    <string>bluetooth-central</string>
    <string>fetch</string>  <!-- NEW -->
</array>
<key>BGTaskSchedulerPermittedIdentifiers</key>
<array>
    <string>com.bowerhaus.Aquavate.hydrationCheck</string>
</array>
```

### Background Notification Behavior

| Aspect | Behavior |
|--------|----------|
| Timing | iOS controls actual execution (could be 15 min to hours) |
| Frequency | Based on app usage patterns |
| Reschedule | Task reschedules itself before completing |
| Data source | Reads from CoreData (may be stale if no BLE sync) |

---

## Phase 8: Goal Reached Notification

Add a celebration notification when the daily goal is achieved.

### 8.1 Add Notification Method

**Modify:** `ios/Aquavate/Aquavate/Services/NotificationManager.swift`

```swift
func scheduleGoalReachedNotification() {
    guard isEnabled && isAuthorized else { return }

    let content = UNMutableNotificationContent()
    content.title = "Goal Reached! 💧"
    content.body = "Good job! You've hit your daily hydration goal."
    content.sound = .default

    let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 1, repeats: false)
    let request = UNNotificationRequest(
        identifier: "hydration-goal-reached",
        content: content,
        trigger: trigger
    )
    notificationCenter.add(request)
}
```

### 8.2 Track Goal Notification State

**Modify:** `ios/Aquavate/Aquavate/Services/HydrationReminderService.swift`

```swift
@Published private(set) var goalNotificationSentToday: Bool = false {
    didSet {
        UserDefaults.standard.set(goalNotificationSentToday, forKey: "goalNotificationSentToday")
    }
}

init() {
    self.goalNotificationSentToday = UserDefaults.standard.bool(forKey: "goalNotificationSentToday")
    // ...
}

func goalAchieved() {
    lastNotifiedUrgency = nil
    notificationManager?.cancelAllPendingReminders()

    // Only send once per day
    if !goalNotificationSentToday {
        notificationManager?.scheduleGoalReachedNotification()
        goalNotificationSentToday = true
    }
}

func dailyReset() {
    goalNotificationSentToday = false  // Reset for new day
    // ...
}
```

### 8.3 Background Task Goal Check

**Modify:** `ios/Aquavate/Aquavate/AquavateApp.swift`

In `handleHydrationCheck()`, check for goal reached before checking deficit:

```swift
let goalNotificationSent = UserDefaults.standard.bool(forKey: "goalNotificationSentToday")
if todaysTotalMl >= effectiveGoal && !goalNotificationSent {
    notificationManager.scheduleGoalReachedNotification()
    UserDefaults.standard.set(true, forKey: "goalNotificationSentToday")
}
```

---

## Phase 9: Watch Local Notifications (iPhone-Triggered)

Add Watch-local notifications so the Watch always receives alerts regardless of iPhone screen state. Currently, iOS notification mirroring only works when the iPhone is locked.

### Design Decision: iPhone-Triggered Approach

Instead of the Watch running its own background tasks and calculating deficit (which would duplicate logic), the iPhone sends a "notify" flag via WatchConnectivity when a notification should be shown. The Watch then schedules a local notification.

**Why this approach:**
- Single source of truth (iPhone calculates everything)
- No duplicated business logic on Watch
- Simpler implementation (~50 lines)
- Works whenever WatchConnectivity is active

**Limitation:** If WatchConnectivity session is inactive (Watch out of range, Bluetooth off), the notification will be queued and delivered when session resumes.

### 9.1 Extend HydrationState Model

**Modify:** `ios/Aquavate/Aquavate/Models/HydrationState.swift`

Add notification request fields:

```swift
struct HydrationState: Codable {
    // Existing fields...
    let dailyTotalMl: Int
    let dailyGoalMl: Int
    let lastDrinkTime: Date?
    let urgency: HydrationUrgency
    let deficitMl: Int
    let timestamp: Date

    // NEW: Notification request for Watch
    let shouldNotify: Bool           // True if Watch should show notification
    let notificationType: String?    // "reminder" or "goalReached"
    let notificationTitle: String?   // e.g., "Hydration Reminder"
    let notificationBody: String?    // e.g., "Time to hydrate! You're 350ml behind pace."
}
```

Add backwards-compatible decoder and convenience initializers.

### 9.2 Update iPhone WatchConnectivityManager

**Modify:** `ios/Aquavate/Aquavate/Services/WatchConnectivityManager.swift`

When sending state, include notification request if needed:

```swift
func sendHydrationStateWithNotification(
    _ state: HydrationState,
    notify: Bool,
    type: String?,
    title: String?,
    body: String?
) {
    var context: [String: Any] = [
        "dailyTotalMl": state.dailyTotalMl,
        "dailyGoalMl": state.dailyGoalMl,
        "urgency": state.urgency.rawValue,
        "deficitMl": state.deficitMl,
        "timestamp": state.timestamp.timeIntervalSince1970,
        // Notification fields
        "shouldNotify": notify,
        "notificationType": type ?? "",
        "notificationTitle": title ?? "",
        "notificationBody": body ?? ""
    ]
    // ... send via updateApplicationContext or transferUserInfo
}
```

### 9.3 Update HydrationReminderService

**Modify:** `ios/Aquavate/Aquavate/Services/HydrationReminderService.swift`

When scheduling a notification, also tell the Watch:

```swift
func evaluateAndScheduleReminder() {
    // ... existing checks ...

    if shouldNotify {
        // Schedule iPhone notification
        notificationManager.scheduleHydrationReminder(urgency: urgency, deficitMl: deficitMl)

        // Also tell Watch to show notification
        let title = "Hydration Reminder"
        let body = urgency == .overdue
            ? "You're falling behind! Drink \(deficitDisplay) to catch up."
            : "Time to hydrate! You're \(deficitDisplay) behind pace."

        WatchConnectivityManager.shared.sendNotificationToWatch(
            type: "reminder",
            title: title,
            body: body
        )
    }
}

func goalAchieved() {
    // ... existing code ...

    if !goalNotificationSentToday {
        notificationManager?.scheduleGoalReachedNotification()
        goalNotificationSentToday = true

        // Also tell Watch
        WatchConnectivityManager.shared.sendNotificationToWatch(
            type: "goalReached",
            title: "Goal Reached! 💧",
            body: "Good job! You've hit your daily hydration goal."
        )
    }
}
```

### 9.4 Watch Notification Manager

**New file:** `ios/Aquavate/AquavateWatch Watch App/AquavateWatch/WatchNotificationManager.swift`

```swift
import UserNotifications
import WatchKit

class WatchNotificationManager {
    static let shared = WatchNotificationManager()

    private let notificationCenter = UNUserNotificationCenter.current()

    func requestAuthorization() async throws {
        let granted = try await notificationCenter.requestAuthorization(options: [.alert, .sound])
        print("[WatchNotification] Authorization \(granted ? "granted" : "denied")")
    }

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
                print("[WatchNotification] Failed: \(error.localizedDescription)")
            } else {
                print("[WatchNotification] Scheduled: \(title)")
                // Also play haptic
                WKInterfaceDevice.current().play(.notification)
            }
        }
    }
}
```

### 9.5 Update Watch Session Manager

**Modify:** `ios/Aquavate/AquavateWatch Watch App/AquavateWatch/WatchSessionManager.swift`

Handle notification requests from iPhone:

```swift
func session(_ session: WCSession, didReceiveApplicationContext applicationContext: [String : Any]) {
    DispatchQueue.main.async {
        // Update hydration state (existing code)
        self.updateHydrationState(from: applicationContext)

        // Check for notification request (NEW)
        if let shouldNotify = applicationContext["shouldNotify"] as? Bool, shouldNotify,
           let type = applicationContext["notificationType"] as? String,
           let title = applicationContext["notificationTitle"] as? String,
           let body = applicationContext["notificationBody"] as? String {

            WatchNotificationManager.shared.scheduleNotification(
                type: type,
                title: title,
                body: body
            )
        }
    }
}
```

### 9.6 Request Notification Permission on Watch

**Modify:** `ios/Aquavate/AquavateWatch Watch App/AquavateWatch/AquavateWatchApp.swift`

```swift
@main
struct AquavateWatchApp: App {
    init() {
        Task {
            try? await WatchNotificationManager.shared.requestAuthorization()
        }
    }
    // ...
}
```

### Watch Notification Behavior

| Scenario | iPhone Notification | Watch Notification |
|----------|--------------------|--------------------|
| iPhone unlocked, Watch on wrist | ✅ Shows on iPhone | ✅ Shows on Watch (local) |
| iPhone locked, Watch on wrist | ✅ Mirrors to Watch | ✅ Shows on Watch (local) |
| iPhone in pocket, Watch on wrist | May not see | ✅ Shows on Watch (local) |

**Key benefit:** Watch always shows notification with haptic regardless of iPhone state.

### Files Summary for Phase 9

| File | Changes |
|------|---------|
| `Models/HydrationState.swift` | Add notification request fields |
| `Services/WatchConnectivityManager.swift` | Add `sendNotificationToWatch()` method |
| `Services/HydrationReminderService.swift` | Call Watch notification on reminder/goal |
| `AquavateWatch/WatchNotificationManager.swift` | **NEW** - Schedule local notifications |
| `AquavateWatch/WatchSessionManager.swift` | Handle notification request from iPhone |
| `AquavateWatch/AquavateWatchApp.swift` | Request notification permission |

---

## Phase 10: Target Intake Visualization on HomeView

Add visual pace tracking to the main iPhone app screen showing expected vs actual progress.

### 10.1 Update HumanFigureProgressView

**Modify:** `ios/Aquavate/Aquavate/Components/HumanFigureProgressView.swift`

- Add `expectedCurrent: Int?` parameter for expected intake by now
- Add `urgency: HydrationUrgency` parameter for color coding
- Add second fill layer (behind actual) showing expected progress in urgency color
- Add "X ml behind target" text below figure (colored by urgency)

### 10.2 Update HomeView

**Modify:** `ios/Aquavate/Aquavate/Views/HomeView.swift`

- Add `@EnvironmentObject var hydrationReminderService`
- Pass expected intake, deficit, and urgency to HumanFigureProgressView

---

## Phase 11: 50ml Rounding for Deficit Display

Round deficit values to nearest 50ml for cleaner display and suppress notifications/display when below 50ml threshold.

### Design Decision

Small deficits (e.g., 23ml behind) are not actionable and create noise. Rounding to 50ml provides cleaner values and a natural threshold for suppression.

### Rounding Formula

```swift
func roundToNearest50(_ value: Int) -> Int {
    return ((value + 25) / 50) * 50
}
```

**Examples:**
- 23ml → 0ml (suppressed)
- 49ml → 50ml
- 73ml → 50ml
- 127ml → 150ml
- 1234ml → 1.2L (formatted for display)

### 11.1 Update HydrationReminderService

**Modify:** `ios/Aquavate/Aquavate/Services/HydrationReminderService.swift`

```swift
// Add constant
static let minimumDeficitForNotification = 50

// Add helper
static func roundToNearest50(_ value: Int) -> Int {
    return ((value + 25) / 50) * 50
}

// In evaluateAndScheduleReminder():
// Change guard from: deficitMl > 0
// To: deficitMl >= Self.minimumDeficitForNotification

// Round deficit before passing to notifications:
let roundedDeficit = Self.roundToNearest50(deficitMl)
notificationManager.scheduleHydrationReminder(urgency: currentUrgency, deficitMl: roundedDeficit)
```

### 11.2 Update HumanFigureProgressView

**Modify:** `ios/Aquavate/Aquavate/Components/HumanFigureProgressView.swift`

```swift
private func roundToNearest50(_ value: Int) -> Int {
    return ((value + 25) / 50) * 50
}

private var roundedDeficitMl: Int {
    roundToNearest50(deficitMl)
}

private var isBehindTarget: Bool {
    roundedDeficitMl >= 50  // Only show if 50ml+ behind
}

// In body, use roundedDeficitMl for display
Text("\(roundedDeficitMl) ml behind target")
```

### 11.3 Update Watch ContentView

**Modify:** `ios/Aquavate/AquavateWatch Watch App/AquavateWatch/ContentView.swift`

```swift
private func roundToNearest50(_ value: Int) -> Int {
    return ((value + 25) / 50) * 50
}

@ViewBuilder
private var statusText: some View {
    if let state = state {
        if state.isGoalAchieved {
            Text("Goal reached! 🎉")
        } else {
            let roundedDeficit = roundToNearest50(state.deficitMl)
            if roundedDeficit >= 50 {
                // Show rounded deficit
                Text("\(roundedDeficit)ml to catch up")
            } else {
                Text("On track ✓")
            }
        }
    }
}
```

### Behavior Summary

| Raw Deficit | Rounded | Display |
|-------------|---------|---------|
| 0-24ml | 0ml | "On track ✓" |
| 25-74ml | 50ml | "50ml to catch up" |
| 75-124ml | 100ml | "100ml to catch up" |
| 125-174ml | 150ml | "150ml to catch up" |
| 1000ml+ | Rounded | "1.0L to catch up" |

### Files Modified

| File | Changes |
|------|---------|
| `Services/HydrationReminderService.swift` | Add `roundToNearest50()`, `minimumDeficitForNotification`, round values in notifications |
| `Components/HumanFigureProgressView.swift` | Add rounding, suppress "behind target" if < 50ml |
| `AquavateWatch/ContentView.swift` | Add rounding, suppress "to catch up" if < 50ml |

---

## Verification

1. **Pace tracking**: At 2pm with 0ml drunk, verify amber/red urgency (you're behind pace)
2. **Catch up display**: Watch shows "Xml to catch up" when behind (rounded to 50ml)
3. **On track display**: After catching up, Watch shows "On track ✓"
4. **Quiet hours**: Set time to 11pm, verify no reminders scheduled
5. **Max reminders**: Manually trigger 8, verify 9th is blocked
6. **Goal achievement**: Reach daily goal, verify "Goal reached! 🎉" and reminders cancelled
7. **Watch complication**: Add to watch face, verify color changes with urgency
8. **Watch app**: Open on watch, verify large drop, progress, and status display
9. **Watch sync**: Take drink on bottle, verify watch updates within seconds
10. **Initial sync**: Launch iPhone app, verify Watch receives current state immediately
11. **Periodic sync**: Leave Watch app open for 2+ minutes, verify deficit increases as time passes (without any iPhone interaction)
12. **Background notifications**: Background app, wait for iOS to schedule task, verify notification arrives
13. **Goal notification**: Reach goal, verify "Goal Reached! 💧" notification appears
14. **Goal notification once**: Drink more after goal, verify no duplicate notification
15. **Watch local notification**: With iPhone unlocked, trigger reminder, verify Watch shows notification with haptic
16. **Watch haptic**: Verify Watch taps wrist when notification arrives
17. **50ml rounding**: Verify deficit displays are rounded to nearest 50ml (e.g., 73ml shows as 50ml)
18. **50ml threshold**: When deficit < 50ml, verify "On track ✓" is shown instead of "Xml behind"
19. **No notification under 50ml**: When deficit < 50ml, verify no reminder notification is triggered
