# Plan: Fix Hardcoded 2000 mL Goal When Disconnected (Issue #31)

## Problem Summary

The iOS app displays a hardcoded 2000 mL hydration goal when disconnected from the bottle, instead of using the user's previously synced goal. This happens because:

1. `BLEManager.dailyGoalMl` is initialized to `2000` (hardcoded default)
2. When connected, this value is updated from the device's BLE Bottle Config
3. If the app is **restarted while disconnected**, the goal resets to 2000 mL because there's no persistence

## Root Cause

In [BLEManager.swift:112](ios/Aquavate/Aquavate/Services/BLEManager.swift#L112):
```swift
@Published private(set) var dailyGoalMl: Int = 2000
```

The goal is only updated when the device sends a Bottle Config update. There's no mechanism to persist and restore the goal across app launches.

## Solution

Persist `dailyGoalMl` to `UserDefaults` using a `didSet` observer (matching the existing `lastSyncTime` pattern at line 134), and restore it in `init()`.

### Files to Modify

| File | Changes |
|------|---------|
| [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) | Add UserDefaults persistence for goal |

### Implementation Steps

1. **Add `didSet` observer to `dailyGoalMl`** (line 112):
   ```swift
   /// Daily goal in ml (from config)
   @Published private(set) var dailyGoalMl: Int = 2000 {
       didSet {
           // Persist to UserDefaults for offline display
           UserDefaults.standard.set(dailyGoalMl, forKey: "lastKnownDailyGoalMl")
       }
   }
   ```

2. **Restore goal in `init()`** (after line 206, following `lastSyncTime` pattern):
   ```swift
   // Load persisted daily goal (default 2000 if never synced)
   if UserDefaults.standard.object(forKey: "lastKnownDailyGoalMl") != nil {
       dailyGoalMl = UserDefaults.standard.integer(forKey: "lastKnownDailyGoalMl")
   }
   ```

That's it - no other changes needed. The `didSet` automatically persists whenever `dailyGoalMl` is updated (from `handleBottleConfigUpdate()` or `writeBottleConfig()`).

## Verification

1. **Build the iOS app** in Xcode
2. **Connect to the bottle** and verify goal syncs from device
3. **Note the goal value** displayed in the app
4. **Force-quit the app** (swipe up from app switcher)
5. **Turn off the bottle** or move out of range
6. **Relaunch the app** while disconnected
7. **Verify** the goal shows the last synced value, not 2000 mL

## Notes

- Follows existing `lastSyncTime` pattern (lines 134-141, 206)
- UserDefaults is appropriate for this single value
- The 2000 mL default remains as fallback for first-time users
- No changes needed to `handleBottleConfigUpdate()` or `writeBottleConfig()` - the `didSet` handles all cases
