# Plan 028: Hybrid BLE Auto-Reconnection

**Status:** In Progress (Testing)
**Branch:** `ble-auto-reconnection`
**Created:** 2026-01-21
**Updated:** 2026-01-21

---

## Goal

Enable automatic BLE reconnection when the bottle wakes up and the phone is nearby, providing seamless sync and real-time display without manual user intervention.

## Approach: Hybrid Reconnection (Option C)

1. **Foreground scan burst** - Fast active scanning when app is active (1-5 sec)
2. **Background reconnection** - iOS auto-connects when bottle advertises (app backgrounded)
3. **Extended advertising** - Firmware advertises longer when unsynced records exist (2 min vs 30 sec)
4. **Scan-on-disconnect** - Recovery from unexpected disconnections (~8 sec)

### Why This Approach

- **Option A (Passive only):** Slower reconnection when app is open (5-15 sec wait)
- **Option C (Hybrid):** Fast foreground + passive background = best UX with minimal complexity
- **No phone battery impact:** No continuous background scanning
- **Minimal bottle battery impact:** Extended advertising only when needed (unsynced records)

---

## Implementation Status

### Phase 1: iOS BLEManager.swift - COMPLETE

- [x] Add reconnection properties
  - `foregroundScanBurstDuration: TimeInterval = 5.0`
  - `scanAfterDisconnectDelay: TimeInterval = 3.0`
  - `autoReconnectEnabled: Bool = true`
  - `foregroundScanTimer: Timer?`
  - `isAutoReconnectScan: Bool = false`
  - `pendingBackgroundReconnectPeripheral: CBPeripheral?`

- [x] Add `performForegroundScanBurst()` method
  - 5-second scan for known device
  - Auto-connect when found
  - Silent failure (no user alerts)

- [x] Add `handleScanBurstTimeout()` method
  - Cleanup timer and state
  - Reset to disconnected

- [x] Add `appDidBecomeActive()` method
  - Trigger scan burst when app foregrounds
  - Skip if already connected/connecting

- [x] Add `appDidEnterBackground()` method
  - Cancel pending auto-reconnect attempts
  - Stop scanning if in progress
  - Disconnect after 5 seconds with background reconnect request

- [x] Add `disconnect(requestBackgroundReconnect: Bool)` method
  - Stores peripheral in `pendingBackgroundReconnectPeripheral` for background reconnection

- [x] Add `requestBackgroundReconnection(to:)` method
  - Calls `centralManager.connect()` with notification options
  - iOS queues this and auto-connects when peripheral advertises

- [x] Add `cancelBackgroundReconnection()` method
  - Cancels pending background reconnection request

- [x] Modify `didConnect`
  - Clear `pendingBackgroundReconnectPeripheral` when connection happens
  - Set `connectedPeripheral` and `connectedDeviceName`
  - Log whether this is a background reconnection

- [x] Modify `didDisconnectPeripheral`
  - Check for `pendingBackgroundReconnectPeripheral` and call `requestBackgroundReconnection()`
  - Add auto-reconnect attempt on unexpected disconnect (3-second delay)

- [x] Modify `didDiscover`
  - Prioritize known device during scan burst
  - Auto-connect if `isAutoReconnectScan && isKnownDevice`

- [x] Enhance `willRestoreState`
  - Properly restore connected peripherals
  - Re-discover characteristics if needed

### Phase 2: iOS AquavateApp.swift - COMPLETE

- [x] Update `handleScenePhaseChange()`
  - Call `bleManager.appDidBecomeActive()` when active
  - Call `bleManager.appDidEnterBackground()` when background

### Phase 3: Firmware Extended Timeouts - COMPLETE

**Problem discovered:** iOS background BLE doesn't actively scan - it passively notices devices when the system periodically checks. Two issues:
1. The 30-second advertising window was too short for iOS to notice in background
2. The 30-second awake duration meant the bottle went to sleep before iOS could connect

**Solution:** Extend both advertising AND awake duration when unsynced records exist.

- [x] Add `BLE_ADV_TIMEOUT_EXTENDED_SEC = 120` constant in `ble_service.h`
- [x] Modify `bleUpdate()` in `ble_service.cpp`:
  - Check `storageGetUnsyncedCount()`
  - Use 2-minute advertising timeout when unsynced records exist
  - Use normal 30-second timeout otherwise
- [x] Add `AWAKE_DURATION_EXTENDED_MS = 120000` constant in `config.h`
- [x] Modify sleep logic in `main.cpp`:
  - Check `storageGetUnsyncedCount()` before sleep decision
  - Use 2-minute awake duration when unsynced records exist
  - Use normal 30-second timeout otherwise

---

## Files Modified

| File | Changes |
|------|---------|
| `ios/Aquavate/Aquavate/Services/BLEManager.swift` | Background reconnection logic (~50 lines added) |
| `ios/Aquavate/Aquavate/AquavateApp.swift` | Lifecycle handling calls BLEManager methods |
| `firmware/include/ble_service.h` | Added `BLE_ADV_TIMEOUT_EXTENDED_SEC` constant |
| `firmware/src/ble_service.cpp` | Extended advertising when unsynced records exist |
| `firmware/src/config.h` | Added `AWAKE_DURATION_EXTENDED_MS` constant |
| `firmware/src/main.cpp` | Extended sleep timeout when unsynced records exist |

---

## How Background Sync Works

### Flow When App is Backgrounded

1. **App backgrounds** → `appDidEnterBackground()` called
2. **5-second delay** → Allows any in-progress sync to complete
3. **Disconnect** → `disconnect(requestBackgroundReconnect: true)` called
4. **didDisconnect fires** → Sees `pendingBackgroundReconnectPeripheral` is set
5. **Request background reconnect** → `centralManager.connect(peripheral, options:)` with notification flags
6. **iOS queues connect request** → App can now be suspended

### Flow When Bottle Wakes with Unsynced Records

1. **Bottle wakes** (motion detected)
2. **Start advertising** → With extended timeout (2 minutes) because unsynced records exist
3. **iOS notices advertisement** → May take 10-60 seconds in background
4. **iOS auto-connects** → Because we called `connect()` earlier
5. **didConnect fires** → App wakes momentarily
6. **Discover services** → Subscribe to notifications
7. **Current State received** → Shows unsynced count > 0
8. **Auto-sync triggers** → `startDrinkSync()` called
9. **Drink records synced** → Saved to CoreData
10. **Idle timer starts** → 60 seconds
11. **Disconnect** → Bottle can sleep again

### Advertising Behavior

| Condition | Advertising Duration | Rationale |
|-----------|---------------------|-----------|
| No unsynced records | 30 seconds | Save battery, nothing to sync |
| Unsynced records exist | 120 seconds (2 min) | Give iOS time to connect in background |

---

## Expected Behavior

| Scenario | What Happens | Timing |
|----------|--------------|--------|
| App open, bottle wakes | Scan burst finds device | 1-5 seconds |
| App backgrounded, bottle wakes (with drinks) | iOS auto-connects, syncs | 10-60 seconds |
| App backgrounded, bottle wakes (no drinks) | No connection (30s timeout) | - |
| Unexpected disconnect | Auto-reconnect attempt | ~8 seconds |
| App terminated, bottle wakes | iOS relaunches app | 30-120 seconds |

---

## Testing Checklist

- [ ] **Test 1: Foreground Reconnection**
  1. Connect to bottle, background app, wait for disconnect
  2. Wake bottle, bring app to foreground
  3. Expected: Auto-connects within 5 seconds

- [ ] **Test 2: Disconnect Recovery**
  1. Connect to bottle, walk out of range
  2. Return to range, wake bottle
  3. Expected: Auto-reconnects within ~8 seconds

- [ ] **Test 3: Background Sync (Main Use Case)**
  1. Connect to bottle, background app
  2. Wait for disconnect (5 seconds)
  3. Wake bottle and pour a drink
  4. Wait 1-2 minutes (iOS needs time to connect)
  5. Bring app to foreground
  6. Expected: Drink already synced and visible

- [ ] **Test 4: App Termination**
  1. Connect to bottle, force quit app
  2. Wake bottle, launch app
  3. Expected: Auto-reconnects during launch

---

## Notes

- Idle disconnect timer (60s) ensures connection doesn't stay open forever
- Extended advertising (2 min) only when needed to minimize battery impact
- iOS background BLE is passive - relies on system scheduling, not active scanning
- Build firmware: `platformio run` (builds successfully)
- Upload firmware: `platformio run -t upload`

---

## Follow-up: Timer Rationalization

While debugging this plan, we discovered the firmware timer architecture was too complex with 6 overlapping timers. See [Plan 034 - Timer Rationalization](034-timer-rationalization.md) for the simplification effort that:

- Reduces 5 firmware timers to 2 clear concepts
- Removes the "extended awake if unsynced" conditional added in Phase 3
- Ties advertising directly to awake state (no separate timeout)
- Renames confusing "continuous awake" to "time since stable"
