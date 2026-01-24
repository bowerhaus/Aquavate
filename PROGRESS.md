# Aquavate - Active Development Progress

**Last Updated:** 2026-01-24
**Current Branch:** `master`

---

## Current Task

None - awaiting next issue.

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md.
```

---

## Recently Completed

- ✅ Retry Button on "Bottle is Asleep" Alert (Issue #52) - [Plan 045](Plans/045-retry-bottle-asleep-alert.md)
  - Added Retry/Cancel buttons to alert in HomeView and HistoryView
  - Users can tap Retry after waking bottle instead of pulling down again
  - Updated iOS-UX-PRD.md Section 2.4

- ✅ Three-Color Stacked Human Figure Fill (Issue #50) - [Plan 044](Plans/044-three-color-stacked-fill.md)
  - Human figure now shows stacked blue/orange/red based on deficit thresholds
  - Orange: deficit up to 20% threshold
  - Red: deficit beyond 20% (only when significantly behind)
  - Updated iOS-UX-PRD.md Section 2.9

- ✅ Orange Line Rounding Fix (Issue #48) - [Plan 043](Plans/043-orange-line-rounding-fix.md)
  - Fixed orange overlay appearing when user is "on track" (small deficit < 25ml)
  - Changed overlay condition to use `isBehindTarget` (50ml rounded threshold)
  - Ensures visual overlay and "behind target" text appear/disappear together

- ✅ Daily Reminder Limit Toggle (Issue #45) - [Plan 042](Plans/042-daily-reminder-limit-toggle.md)
  - Added "Limit Daily Reminders" toggle to Settings → Hydration Reminders
  - When disabled, allows unlimited reminders (respects quiet hours and escalation)
  - Fixed back-on-track notification timing bug (was using stale urgency data)

- ✅ Fix HomeView Refresh Alert (Issue #44) - [Plan 041](Plans/041-fix-homeview-refresh-alert.md)
  - Fixed race condition in BLEManager where `stopScanning()` guard caused corrupted state
  - Refactored `attemptConnection()` to use elapsed time for `.bottleAsleep` detection
  - Added defensive state recovery and cleanup on Bluetooth transitions
  - Tested: All 5 verification scenarios pass consistently

- ✅ Local Activity Stats Storage (Issue #36 Comment) - [Plan 040](Plans/040-local-activity-stats-storage.md)
  - Activity stats (motion wake events, backpack sessions) now persist in CoreData
  - Users can view data even when disconnected with "Last synced X ago" timestamp
  - Fixed: Diagnostics section accessible when disconnected in SettingsView

- ✅ Fix Duplicate HealthKit Entries (Issue #37) - [Plan 039](Plans/039-fix-duplicate-healthkit-entries.md)
  - Race condition in `syncDrinksToHealthKit()` caused multiple HealthKit entries
  - Added boolean lock to prevent concurrent sync execution
  - Added timestamp-based dedup check as defense in depth
  - Tested: Single drink creates exactly one HealthKit entry

- ✅ Hydration Reminders + Apple Watch App (Issue #27) - [Plan 036](Plans/036-watch-hydration-reminders.md)
  - Smart hydration reminders with pace-based urgency model
  - Apple Watch companion app with complications
  - Deficit tracking: "Xml to catch up" / "On track ✓" / "Goal reached!"
  - Background notifications via BGAppRefreshTask
  - Watch local notifications triggered by iPhone for reliable delivery
  - Target intake visualization on HomeView
  - 50ml rounding for deficit display
  - Back On Track notification (optional)
  - All 12 phases implemented and tested

- ✅ Activity & Sleep Mode Tracking (Issue #36) - [Plan 038](Plans/038-activity-sleep-tracking.md)
  - Track individual wake events and backpack sessions for battery analysis
  - Firmware: RTC buffer for 100 motion wakes + 20 backpack sessions
  - iOS: Activity Stats view in Settings → Diagnostics section
  - Enhancement: "Drink taken" flag shows water drop icon for wakes where drink was consumed

- ✅ Persist Daily Goal When Disconnected (Issue #31) - [Plan 037](Plans/037-persist-daily-goal.md)
  - Fixed iOS app displaying hardcoded 2000 mL goal when disconnected
  - Added `didSet` observer to persist `dailyGoalMl` to UserDefaults
  - Restored goal from UserDefaults on app launch
  - Follows existing `lastSyncTime` persistence pattern

- ✅ Shake-to-Empty Toggle Setting (Issue #32) - [Plan 036](Plans/036-shake-to-empty-toggle.md)
  - Added toggle in iOS Settings to enable/disable shake-to-empty gesture
  - Setting syncs to firmware via BLE and persists in NVS

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
