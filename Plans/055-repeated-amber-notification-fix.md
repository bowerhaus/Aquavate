# Fix: Repeated Amber Level Notifications Not Sent

**Status:** ✅ COMPLETE (2026-01-26)

## Problem Summary

When a user falls behind their hydration target to the amber level, gets a notification, drinks enough to get back on track (deficit < 0), then falls behind again to amber level - they do NOT get another notification.

## Root Cause

The bug is in [HydrationReminderService.swift:331-346](ios/Aquavate/Aquavate/Services/HydrationReminderService.swift#L331-L346) - the `syncCurrentStateFromCoreData()` function.

This function is called:
- On app launch ([AquavateApp.swift:54](ios/Aquavate/Aquavate/AquavateApp.swift#L54))
- **When app becomes active/foreground** ([AquavateApp.swift:76](ios/Aquavate/Aquavate/AquavateApp.swift#L76))

```swift
func syncCurrentStateFromCoreData(dailyGoalMl: Int) {
    let todaysTotalMl = PersistenceController.shared.getTodaysTotalMl()
    // ...
    self.dailyTotalMl = todaysTotalMl
    updateUrgency()      // Updates urgency to .onTrack
    syncStateToWatch()
    // BUG: checkBackOnTrack() is NOT called here!
}
```

**The bug scenario:**
1. User is at amber level, gets notification → `lastNotifiedUrgency = .attention`
2. App goes to background (user locks phone or switches apps)
3. User drinks water (synced to bottle, saved to CoreData)
4. User opens app / app comes to foreground
5. `syncCurrentStateFromCoreData()` is called
6. This updates urgency to `.onTrack` via `updateUrgency()`
7. **BUT `checkBackOnTrack()` is NOT called** → `lastNotifiedUrgency` stays `.attention`!
8. Later, user falls behind again to amber
9. No notification because `currentUrgency (.attention) > lastNotifiedUrgency (.attention)` is FALSE

## Proposed Fix

Modify `syncCurrentStateFromCoreData()` to capture the previous urgency and call `checkBackOnTrack()` after updating state.

### Changes Required

**File: [HydrationReminderService.swift](ios/Aquavate/Aquavate/Services/HydrationReminderService.swift)**

Modify `syncCurrentStateFromCoreData()` (lines 331-347):

```swift
func syncCurrentStateFromCoreData(dailyGoalMl: Int) {
    // Capture previous urgency before updating state
    let previousUrgency = currentUrgency

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

    // Check for back-on-track transition (resets lastNotifiedUrgency if improved)
    checkBackOnTrack(previousUrgency: previousUrgency)

    print("[HydrationReminder] Initial sync from CoreData: \(todaysTotalMl)ml, goal=\(dailyGoalMl)ml")
}
```

---

## Additional Change: Update Urgency Colors

Update the urgency level colors to be more distinct with 80% opacity.

**File: [HydrationState.swift](ios/Aquavate/Aquavate/Models/HydrationState.swift)**

Modify the `color` property (lines 18-24):

```swift
var color: Color {
    switch self {
    case .onTrack: return .blue
    case .attention: return Color(red: 0.8, green: 0.6, blue: 0.0).opacity(0.8)  // Mustard yellow
    case .overdue: return Color(red: 0.7, green: 0.0, blue: 0.0).opacity(0.8)    // Deep red
    }
}
```

---

## Changes Made

### 1. Fixed Repeated Notification Bug (HydrationReminderService.swift)
- **Issue:** `lastNotifiedUrgency` was not persisted to UserDefaults, so after app restart the same notification would be sent again
- **Fix:** Added `didSet` observer to persist `lastNotifiedUrgency` and its date to UserDefaults
- **Fix:** On init, restore `lastNotifiedUrgency` if it was set today (handles day boundaries correctly)
- **Also:** The `checkBackOnTrack()` call was already present (added in earlier work)

### 2. Updated Urgency Colors (HydrationState.swift)
- Attention color: RGB(247, 239, 151) - pale yellow
- Overdue color: `.red`
- All colors at 100% opacity (no longer using 60% opacity)

### 3. Improved Human Figure Progress View (HumanFigureProgressView.swift)
- Added white background layer so colors display correctly (not blending with grey)
- Replaced discrete red/amber zones with smooth LinearGradient from attention (bottom) to overdue (top)
- Removed extra `.opacity(0.6)` modifiers that were causing color blending issues

## Verification

1. Build the iOS app: `cd ios/Aquavate && xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`
2. Test notification fix:
   - Wait until deficit reaches ~200ml (amber notification should fire)
   - Drink enough to get deficit below 0 (on track)
   - Background the app, then bring it back to foreground
   - Wait/let time pass until deficit rises above 150ml again
   - Verify a new amber notification is received
3. Verify color changes:
   - Check that attention level shows pale yellow gradient to red
   - Gradient transition is smooth as deficit increases
4. Run existing tests to ensure no regressions

## Testing Performed

- iOS app builds successfully (`xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`)
- Visual verification of gradient colors in human figure view
- Verified opacity settings produce expected colors (tested 0%, 50%, 90%, 100%)
