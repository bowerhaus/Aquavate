# Aquavate - Active Development Progress

**Last Updated:** 2026-01-23
**Current Branch:** `watch-hydration-reminders`

---

## Current Task

**Hydration Reminders + Apple Watch App (Issue #27)** - [Plan 036](Plans/036-watch-hydration-reminders.md)

Smart hydration reminder notifications and Apple Watch companion app with complications.

### Key Design Decisions
- Max 8 reminders per day
- Fixed quiet hours (10pm-7am) - also defines active hydration hours (7am-10pm)
- **Pace-based urgency model** - tracks whether you're on schedule to meet daily goal
- Stop reminders once daily goal achieved
- Color-coded urgency: Blue (on-track) → Amber (attention) → Red (overdue)
- Escalation model: Only notify when urgency increases (no repeated same-level notifications)
- Watch: Complication + minimal app with large colored water drop + "Xml to catch up"
- Haptic-only notifications (Watch only - iPhone bundled with sound)

### Progress

#### Phase 1-5: Complete ✅
- All phases implemented and working
- Watch app builds and runs on real device
- Initial sync bug fixed (Watch was showing 0L)

#### Phase 6: Pace-Based Urgency Model - IMPLEMENTED ✅
Migrated from time-based urgency to pace-based urgency model:

- [x] 6.1 Update `HydrationState.swift` - added `deficitMl: Int` field
- [x] 6.2 Update `HydrationReminderService.swift`:
  - Replaced time-based constants with pace-based (`activeHoursStart`, `activeHoursEnd`, `attentionThreshold`)
  - Added `calculateExpectedIntake()` and `calculateDeficit()` methods
  - Updated `updateUrgency()` for deficit-based logic
  - Updated `buildHydrationState()` to include deficit
- [x] 6.3 Update Watch `ContentView.swift` - shows "Xml to catch up" / "On track ✓" / "Goal reached!"
- [x] 6.4 Initial sync on app launch (previously implemented)
- [x] 6.5 Periodic Watch sync (every 60s) - timer now calls `updateUrgency()` + `syncStateToWatch()`

#### Testing Status
- [x] Test pace tracking - amber urgency showing correctly
- [x] Test "Xml to catch up" display on Watch - working
- [ ] Test "On track ✓" when caught up
- [ ] Test "Goal reached!" when goal achieved
- [ ] Test periodic sync (deficit increases over time without iPhone interaction)
- [ ] Test notifications with test mode
- [ ] Test quiet hours
- [ ] Test max 8 reminders
- [ ] Test Watch complication color changes

### Current Status: Phase 6 Implemented and Partially Tested

**Pace-Based Model Working:**
- Urgency based on deficit from expected pace (not time since last drink)
- Active hydration hours: 7am-10pm (15 hours)
- Expected intake calculated proportionally through the day
- Thresholds: On track (deficit ≤ 0) → Amber (1-19% behind) → Red (20%+ behind)

**Watch Display (Verified Working):**
- Shows "Xml to catch up" when behind schedule (deficit > 0) ✅
- Shows "On track ✓" when on pace or ahead
- Shows "Goal reached! 🎉" when daily goal achieved
- Removed "time since last drink" (no longer relevant with pace model)

**Bug Fixes Applied:**
- Added backwards-compatible decoder for `deficitMl` field (old saved data was missing it)
- Watch now checks `receivedApplicationContext` on session activation
- Fixed duplicate Watch files issue - actual code is in `AquavateWatch Watch App/AquavateWatch/`

### Files Modified This Session
- `ios/Aquavate/Aquavate/Models/HydrationState.swift`:
  - Added `deficitMl: Int` field
  - Added custom decoder for backwards compatibility
  - Added memberwise initializer
- `ios/Aquavate/Aquavate/Services/HydrationReminderService.swift` - complete pace-based model:
  - Replaced time-based constants with pace-based (`activeHoursStart/End`, `attentionThreshold`)
  - Added `calculateExpectedIntake()` and `calculateDeficit()` methods
  - Updated `updateUrgency()` for deficit-based logic
  - Updated `buildHydrationState()` to include deficit
  - Updated periodic timer to also sync to Watch
- `ios/Aquavate/Aquavate/Services/NotificationManager.swift` - updated `scheduleHydrationReminder()` to use deficit instead of minutes
- `ios/Aquavate/AquavateWatch Watch App/AquavateWatch/ContentView.swift` - status text display, removed time since last drink
- `ios/Aquavate/AquavateWatch Watch App/AquavateWatch/WatchSessionManager.swift` - check existing context on activation

**Note:** There are duplicate Watch files in two locations. The **actual running code** is in:
- `ios/Aquavate/AquavateWatch Watch App/AquavateWatch/` (this is what runs on device)

### Files Created (All on disk)
**iPhone:**
- `ios/Aquavate/Aquavate/Models/HydrationState.swift` ✅
- `ios/Aquavate/Aquavate/Services/NotificationManager.swift` ✅
- `ios/Aquavate/Aquavate/Services/HydrationReminderService.swift` ✅
- `ios/Aquavate/Aquavate/Services/SharedDataManager.swift` ✅
- `ios/Aquavate/Aquavate/Services/WatchConnectivityManager.swift` ✅

**Watch (in `ios/Aquavate/AquavateWatch/`):**
- `AquavateWatchApp.swift` ✅
- `ContentView.swift` ✅
- `ComplicationProvider.swift` ✅
- `WatchSessionManager.swift` ✅
- `AquavateWatch.entitlements` ✅

---

## Context Recovery

To resume from this progress file:
```
Continue from PROGRESS.md - Issue #27 (Hydration Reminders + Apple Watch App).
Phase 6 (pace-based urgency model) implemented and partially tested.
Watch shows deficit correctly. Still need to test: on-track, goal reached, periodic sync, notifications.
IMPORTANT: Watch files are in TWO locations - actual code is in "AquavateWatch Watch App/AquavateWatch/"
Plan: Plans/036-watch-hydration-reminders.md
Branch: watch-hydration-reminders
```

---

## Recently Completed

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
- `watch-hydration-reminders` - Current work (Issue #27)

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
