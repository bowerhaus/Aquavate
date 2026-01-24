# Plan: Add Daily Reminder Limit Toggle (Issue #45)

## Summary
Add a toggle in Settings to enable/disable the daily hydration reminder limit (currently hardcoded at 12/day). Also fix back-on-track notification timing bug.

## Files to Modify

1. **[NotificationManager.swift](ios/Aquavate/Aquavate/Services/NotificationManager.swift)**
2. **[SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift)**
3. **[BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)**
4. **[HydrationReminderService.swift](ios/Aquavate/Aquavate/Services/HydrationReminderService.swift)**

---

## Implementation Steps

### Step 1: Add `dailyLimitEnabled` Property to NotificationManager

**File:** [NotificationManager.swift](ios/Aquavate/Aquavate/Services/NotificationManager.swift)

Add a new published property (after `backOnTrackEnabled` around line 40):

```swift
@Published var dailyLimitEnabled: Bool {
    didSet {
        UserDefaults.standard.set(dailyLimitEnabled, forKey: "dailyReminderLimitEnabled")
    }
}
```

Initialize it in `init()` (around line 55):

```swift
self.dailyLimitEnabled = UserDefaults.standard.object(forKey: "dailyReminderLimitEnabled") as? Bool ?? true
```

Note: Default to `true` to preserve existing behavior.

### Step 2: Modify `canSendReminder` to Respect Toggle

**File:** [NotificationManager.swift:138-142](ios/Aquavate/Aquavate/Services/NotificationManager.swift#L138-L142)

Update the computed property:

```swift
var canSendReminder: Bool {
    checkDailyReset()
    guard dailyLimitEnabled else { return true }
    return remindersSentToday < Self.maxRemindersPerDay
}
```

### Step 3: Add Toggle UI in SettingsView

**File:** [SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift)

Add new toggle after "Reminders Today" display (after line 459, before "Back On Track Alerts"):

```swift
Toggle(isOn: $notificationManager.dailyLimitEnabled) {
    HStack {
        Image(systemName: "number.circle")
            .foregroundStyle(.blue)
        Text("Limit Daily Reminders")
    }
}
```

### Step 4: Make "Reminders Today" Display Conditional

**File:** [SettingsView.swift:451-459](ios/Aquavate/Aquavate/Views/SettingsView.swift#L451-L459)

Update to show limit only when enabled, otherwise show just the count:

```swift
HStack {
    Image(systemName: "bell.badge")
        .foregroundStyle(.secondary)
    Text("Reminders Today")
    Spacer()
    if notificationManager.dailyLimitEnabled {
        Text("\(notificationManager.remindersSentToday)/\(NotificationManager.maxRemindersPerDay)")
            .foregroundStyle(.secondary)
            .font(.subheadline)
    } else {
        Text("\(notificationManager.remindersSentToday)")
            .foregroundStyle(.secondary)
            .font(.subheadline)
    }
}
```

### Step 5: Fix Back-on-Track Notification Timing

**Bug:** The back-on-track notification was not sent immediately because `drinkRecorded()` was called before `updateState()`, so it was using stale urgency values.

**File:** [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)

In `processSyncedRecords()`, capture previous urgency before state update:

```swift
// Capture previous urgency before updating state for back-on-track detection
let previousUrgency = hydrationReminderService?.currentUrgency

// Update hydration state (this recalculates urgency)
hydrationReminderService?.updateState(
    totalMl: dailyTotalMl,
    goalMl: dailyGoalMl,
    lastDrink: Date()
)

// Check for back-on-track transition
hydrationReminderService?.checkBackOnTrack(previousUrgency: previousUrgency)
```

**File:** [HydrationReminderService.swift](ios/Aquavate/Aquavate/Services/HydrationReminderService.swift)

Rename `drinkRecorded()` to `checkBackOnTrack(previousUrgency:)`:

```swift
func checkBackOnTrack(previousUrgency: HydrationUrgency?) {
    guard let previousUrgency = previousUrgency else { return }

    if previousUrgency > .onTrack && currentUrgency == .onTrack {
        // Send back-on-track notification
        notificationManager?.scheduleBackOnTrackNotification()
        // ... watch notification code
    }
}
```

---

## UI Result

```
Hydration Reminders
├─ [Toggle] Hydration Reminders
├─ Status: Authorized
├─ Current Status: On Track
├─ Reminders Today: 3/12      (or just "3" when limit disabled)
├─ [Toggle] Limit Daily Reminders    <-- NEW
├─ [Toggle] Back On Track Alerts
```

---

## Verification

- [x] Build the app: No compilation errors
- [x] Test toggle persistence: Toggle state persists across app restarts
- [x] Test display update: Shows "3/12" with limit enabled, "3" when disabled
- [x] Test default behavior: Fresh install has limit enabled by default
- [ ] Test back-on-track notification: Get behind pace, drink to catch up, verify `[HydrationReminder] Back on track!` log appears immediately
