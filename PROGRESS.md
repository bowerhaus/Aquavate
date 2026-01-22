# Aquavate - Active Development Progress

**Last Updated:** 2026-01-22
**Current Branch:** `ble-auto-reconnection`

---

## Current Task

**Timer & Sleep Rationalization** - [Plan 034](Plans/034-timer-rationalization.md)

Simplifying the firmware timer architecture from 6 confusing timers to 2 clear concepts. This arose from debugging [Plan 028 (BLE Auto-Reconnection)](Plans/028-ble-auto-reconnection.md).

### Status: Implementation Complete - Ready for Testing

### Problem (Solved)

The firmware had too many overlapping timers with different reset conditions. This has been simplified to a two-timer model.

### Solution: Two-Timer Model (Implemented)

**Timer 1: Activity Timeout (`wakeTime` - 30s)**
- Resets on: gesture change, BLE data activity (sync, commands)
- Does NOT reset on: BLE connect/disconnect alone
- When expires: stop advertising, enter normal sleep (motion wake)

**Timer 2: Time Since Stable (`g_time_since_stable_start` - 3 min)**
- Resets when: bottle becomes UPRIGHT_STABLE (also on shake/drink as user interaction)
- Does NOT reset on: BLE connect/disconnect alone
- When expires: extended sleep (timer wake 60s) - "backpack mode"

### Implementation Complete

- [x] **Step 1**: Remove BLE advertising timeout - advertising stops at sleep
- [x] **Step 2**: Implement activity timeout with BLE activity reset
- [x] **Step 3**: Rename `g_continuous_awake_start` → `g_time_since_stable_start`, change threshold 5min → 3min
- [x] **Step 4**: Remove `AWAKE_DURATION_EXTENDED_MS` and unsynced-record conditional
- [x] **Step 5**: Update config.h with clear comments
- [x] **Step 6**: Remove "Sleep blocked - BLE connected" logic for activity timeout
- [x] **Step 7**: Audit all timer reset points
- [x] **Step 8**: Remove "Extended sleep blocked - BLE connected" logic (same issue as Step 6)

### Timer Reset Audit (2026-01-22)

**`wakeTime` resets - all appropriate:**
- Gesture change (main.cpp:1304) ✅
- BLE data activity via `bleCheckDataActivity()` (main.cpp:964) ✅
- BLE commands: RESET_DAILY, CLEAR_HISTORY, SET_DAILY_TOTAL, TARE_NOW ✅
- Calibration activity (lines 1153, 1245, 1532) ✅
- Setup initialization (main.cpp:541) ✅

**`g_time_since_stable_start` resets - fixed:**
- UPRIGHT_STABLE gesture (main.cpp:1490) ✅
- Shake gesture - user interaction (main.cpp:1042) ✅
- Drink recorded - user interaction (main.cpp:1351) ✅
- Wake/power-on initialization (lines 579, 582, 841) ✅
- ~~BLE connected (main.cpp:1504)~~ ❌ **REMOVED** - same bug as activity timeout

**Fix applied:** Removed `bleIsConnected()` check that was blocking extended sleep and resetting `g_time_since_stable_start`. Extended sleep now triggers regardless of BLE connection status, consistent with activity timeout behavior.

### Files Modified
- `firmware/src/config.h` - Two-timer model documentation, renamed constants
- `firmware/src/main.cpp` - Simplified sleep logic, removed BLE-connected sleep block
- `firmware/src/ble_service.cpp` - Removed advertising timeout, added `bleCheckDataActivity()`
- `firmware/include/ble_service.h` - Removed timeout constants, added activity check function
- `firmware/src/serial_commands.cpp` - Updated variable references for STATUS command

### Build Status
Firmware: **SUCCESS** (2026-01-22)
- RAM: 11.6% (37920/327680 bytes)
- Flash: 54.3% (712361/1310720 bytes)

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
