# Aquavate - Active Development Progress

**Last Updated:** 2026-01-22
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

- ✅ HealthKit Water Intake Integration - [Plan 029](Plans/029-healthkit-integration.md)
- ✅ Settings Page Cleanup (Issue #22) - [Plan 033](Plans/033-settings-page-cleanup.md)
  - Replaced hardcoded `Bottle.sample` with live BLEManager properties
  - Removed unused `useOunces` preference
  - Removed version string from About section
  - Device name now persists after disconnect (shows last connected device)
- ✅ Fix Hydration Graphic Update on Deletion (Issue #20) - [Plan 032](Plans/032-hydration-graphic-update-fix.md)
  - iOS: Added `@MainActor` to Task in BLE deletion completion handler (HomeView.swift)
  - Firmware: Synced daily goal between display and BLE config (now uses DRINK_DAILY_GOAL_ML=2500)
  - Added `displaySetDailyGoal()` function for runtime configuration
  - Fixed bottleConfig defaults to prevent divide-by-zero
- ✅ Fix Daily Intake Reset at 4am (Issue #17) - [Plan 031](Plans/031-daily-reset-from-records.md)
  - Removed redundant cached fields from DailyState struct
  - Daily totals now computed dynamically from drink records using 4am boundary
  - Matches iOS app calculation logic for consistency
  - Removed deprecated SET_DAILY_INTAKE command
- ✅ Shake-to-Empty Improvements - [Plan 030](Plans/030-shake-to-empty-improvements.md)
  - Fixed extended sleep lockout when shaking to empty
  - Extended sleep timer now resets on user interactions (shake, drink, stable)
  - Water level debug output now visible in IOS_MODE=1
  - Added drink tracking delta debug output
- ✅ Bidirectional Drink Record Sync - [Plan 027](Plans/027-bidirectional-drink-sync.md)
- ✅ Swipe-to-Delete Drinks & Enhanced Reset Daily - [Plan 026](Plans/026-swipe-to-delete-drinks.md)
- ✅ Shake-to-Empty Gesture - [Plan 025](Plans/025-shake-to-empty-gesture.md)
- ✅ Improved Calibration UI - [Plan 024](Plans/024-improved-calibration-ui.md)
- ✅ ADXL343 Accelerometer Migration - [Plan 023](Plans/023-adxl343-accelerometer-migration.md)
- ✅ DS3231 RTC Integration - [Plan 022](Plans/022-ds3231-rtc-integration.md)

---

## Known Issues

**Wake-on-tilt sensitivity (ADXL343):**
- Requires deliberate shake (~2 seconds) due to aggressive filtering
- Trade-off accepted for battery life benefits

---

## Branch Status

- `master` - Stable baseline

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
