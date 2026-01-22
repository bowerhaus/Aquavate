# Plan 035: Re-introduce Extended Awake Duration for Unsynced Records

**Status:** ✅ Complete
**Branch:** `extended-awake-unsynced`
**Created:** 2026-01-22
**Completed:** 2026-01-22
**Related:** [Plan 034 - Timer Rationalization](034-timer-rationalization.md), GitHub Issue #24

## Goal

Improve background sync reliability by keeping the bottle awake longer when unsynced drink records exist, giving iOS more time to opportunistically connect.

## Context

**Current state (after Plan 034 Timer Rationalization):**
- Bottle wakes on motion, advertises while awake
- `ACTIVITY_TIMEOUT_MS = 30000` - sleeps after 30 seconds idle
- **No special handling for unsynced records** - the extended awake logic was removed

**Before Plan 034:**
- Bottle stayed awake for 2 minutes when unsynced records existed
- This gave iOS time to opportunistically connect in background

**User pain point:**
- Takes a drink (bottle wakes, records it)
- Puts bottle down → sleeps after 30 seconds
- iOS background BLE is slow (10-60+ seconds) → misses the 30-second window
- Opens app 5+ minutes later → bottle asleep, can't sync
- Has to pick up bottle again to wake it

**Constraint:**
- When bottle is asleep, no iOS code can reach it - must wake bottle first
- Option B (iOS state restoration) doesn't help this scenario

## Solution

Re-introduce extended activity timeout when unsynced records exist:
- **Normal (no unsynced)**: 30 seconds → sleep
- **Unsynced records exist**: 4 minutes → sleep

This fits within the Plan 034 two-timer model - it's still a single activity timeout, just with a conditional duration.

## Changes Required

### Firmware (2 files)

**1. [config.h](firmware/src/config.h:128)** - Add extended timeout constant
```cpp
// After line 128 (ACTIVITY_TIMEOUT_MS):
#define ACTIVITY_TIMEOUT_EXTENDED_MS   240000  // 4 minutes when unsynced records exist
```

**2. [main.cpp](firmware/src/main.cpp:1515-1517)** - Use extended timeout when unsynced
Replace the current comment and sleep check:
```cpp
// Plan 034: Simplified - single activity timeout, no extended timeout for unsynced records
// BLE data activity resets the timeout via bleCheckDataActivity() above
if (g_sleep_timeout_ms > 0 && millis() - wakeTime >= g_sleep_timeout_ms) {
```

With:
```cpp
// Check if activity timeout expired (only if sleep enabled)
// Use extended timeout when unsynced records exist to give iOS time to connect
uint32_t timeout_ms = g_sleep_timeout_ms;
if (storageGetUnsyncedCount() > 0) {
    timeout_ms = ACTIVITY_TIMEOUT_EXTENDED_MS;
}
if (g_sleep_timeout_ms > 0 && millis() - wakeTime >= timeout_ms) {
```

Also add a debug line in the sleep path to show which timeout was used.

### iOS (1 file)

**[BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)** - Request background reconnect when already disconnected

The app already handles background reconnection, but only if it was connected when backgrounded. Added handling for the case where the bottle disconnected while the app was in foreground:

```swift
// In appDidEnterBackground(), after the existing "if connectionState.isConnected" block:
} else if autoReconnectEnabled {
    // Already disconnected but have a known device - still request background reconnect
    if let identifierString = UserDefaults.standard.string(forKey: BLEConstants.lastConnectedPeripheralKey),
       let identifier = UUID(uuidString: identifierString) {
        let peripherals = centralManager.retrievePeripherals(withIdentifiers: [identifier])
        if let peripheral = peripherals.first {
            requestBackgroundReconnection(to: peripheral)
        }
    }
}
```

## Why 4 Minutes?

- **30 seconds**: Current - too short for iOS background BLE
- **2 minutes**: Previous - marginal improvement
- **4 minutes**: Good balance - high chance iOS connects, moderate battery impact
- **5+ minutes**: Diminishing returns, excessive battery drain

## Battery Impact Assessment

| Metric | Now (30s) | Proposed (4 min) | Impact |
|--------|-----------|------------------|--------|
| Awake time per drink (unsynced) | 30 sec | 4 min | +3.5 min |
| Current draw while advertising | ~15mA | ~15mA | Same |
| Extra energy per wake | - | ~3.2J | Moderate |
| Impact on battery life | Baseline | ~10-15% reduction | Acceptable |

**Key point**: The extended timeout only applies when unsynced records exist. After iOS syncs, subsequent wakes use the normal 30-second timeout.

## Verification

1. **Build firmware**: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. **Upload to device**: User handles manually
3. **Test scenario**:
   - Connect app to bottle, background the app
   - Wait for disconnect (5 seconds)
   - Wake bottle (pick it up), pour a drink, set it down
   - Wait 2-3 minutes (leave app backgrounded)
   - Open app
   - Expected: Drink should already be synced (background connection happened)

4. **Verify normal timeout still works**:
   - Sync all records so unsynced count = 0
   - Wake bottle, set it down
   - Expected: Sleeps after 30 seconds (not 4 minutes)

## Remaining Limitation

If the user opens the app **after** the 4-minute window has elapsed and the bottle has gone back to sleep:
- The bottle cannot be reached
- User must pick up the bottle to wake it
- Foreground scan burst will then connect within 5 seconds

This is unavoidable without significant battery trade-offs (periodic wake advertising).
