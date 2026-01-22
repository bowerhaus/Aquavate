# Aquavate - Active Development Progress

**Last Updated:** 2026-01-22
**Current Branch:** `master`

---

## Current Task

None - ready for next task.

---

## Recently Completed

- ✅ Extended Awake Duration for Unsynced Records (Issue #24) - [Plan 035](Plans/035-extended-awake-unsynced.md)
  - Re-introduced 4-minute extended timeout when unsynced records exist
  - Enables iOS opportunistic background sync while bottle advertises
  - iOS: Request background reconnection even when already disconnected
  - Tested: Background sync works when app backgrounded, drink taken, app foregrounded

- ✅ Timer & Sleep Rationalization - [Plan 034](Plans/034-timer-rationalization.md)
  - Simplified firmware from 6 timers to 2-timer model
  - Timer 1: Activity timeout (30s) - resets on gesture change or BLE data activity
  - Timer 2: Time since stable (3 min) - resets on UPRIGHT_STABLE, triggers extended sleep
  - Removed BLE connection blocking sleep (connection alone is not activity)
  - Added PING command for keep-alive during app refresh
  - Added pull-down refresh to HistoryView
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
