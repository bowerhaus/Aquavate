# Aquavate - Active Development Progress

**Last Updated:** 2026-01-21
**Current Branch:** `ble-auto-reconnection`

---

## Current Task

**BLE Auto-Reconnection** - [Plan 028](Plans/028-ble-auto-reconnection.md)

Implementing hybrid auto-reconnection so the bottle automatically connects to the phone when it wakes up.

### Status: Implementation Complete - Testing

All code changes are complete. Background sync is not yet confirmed working - iOS background BLE is unreliable and may take minutes to connect or may not connect at all depending on system state.

### What's Implemented

**iOS App:**
- [x] Foreground auto-reconnect: 5-second scan burst when app comes to foreground
- [x] Background disconnect: App disconnects after 5 seconds when backgrounded
- [x] Background reconnection request: `centralManager.connect()` queued for iOS to auto-connect
- [x] Idle disconnect timer: 60-second timer after connection complete
- [x] `pendingBackgroundReconnectPeripheral` tracking
- [x] `didDisconnectPeripheral` calls `requestBackgroundReconnection()` when appropriate
- [x] `didConnect` handles background reconnection and clears pending state

**Firmware:**
- [x] Extended advertising timeout (2 min) when unsynced records exist
- [x] Extended awake duration (2 min) when unsynced records exist
- [x] Only applies in iOS mode (`ENABLE_BLE=1`)

### Testing Status
- [ ] Background sync not yet confirmed working
- Firmware logs show sync activity when it happens - look for `[BLE] Sync: START`, `[BLE] Sync: COMPLETE`
- iOS logs visible in Console.app - filter for "Aquavate" or "BLE"

### Key Files Modified
- `ios/Aquavate/Aquavate/Services/BLEManager.swift` - Background reconnection logic
- `ios/Aquavate/Aquavate/AquavateApp.swift` - App lifecycle calls
- `firmware/include/ble_service.h` - `BLE_ADV_TIMEOUT_EXTENDED_SEC` (120s)
- `firmware/src/ble_service.cpp` - Extended advertising when unsynced
- `firmware/src/config.h` - `AWAKE_DURATION_EXTENDED_MS` (120s)
- `firmware/src/main.cpp` - Extended sleep timeout when unsynced

### Build Status
Firmware: **SUCCEEDED** - ready to upload (`platformio run -t upload`)

---

## Recently Completed

- Daily intake resets at 4am (current branch)
- Bidirectional Drink Record Sync - [Plan 027](Plans/027-bidirectional-drink-sync.md)
- Swipe-to-Delete Drinks - [Plan 026](Plans/026-swipe-to-delete-drinks.md)

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
