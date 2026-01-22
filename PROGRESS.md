# Aquavate - Active Development Progress

**Last Updated:** 2026-01-22
**Current Branch:** `watch-hydration-reminders`

---

## Current Task

**Hydration Reminders + Apple Watch App (Issue #27)** - [Plan 036](Plans/036-watch-hydration-reminders.md)

Smart hydration reminder notifications and Apple Watch companion app with complications.

### Key Design Decisions
- Max 8 reminders per day
- Fixed quiet hours (10pm-7am)
- 60-minute base reminder interval
- Stop reminders once daily goal achieved
- Color-coded urgency: Blue (on-track) → Amber (attention) → Red (overdue)
- Escalation model: Only notify when urgency increases (no repeated same-level notifications)
- Watch: Complication + minimal app with large colored water drop
- Haptic-only notifications (Watch only - iPhone bundled with sound)

### Progress

#### Phase 1: Hydration Reminder Service (iOS) ✅
- [x] 1.1 Create `HydrationState.swift` model (HydrationUrgency enum + HydrationState struct)
- [x] 1.2 Create `NotificationManager.swift` service (UNUserNotificationCenter wrapper)
- [x] 1.3 Create `HydrationReminderService.swift` (reminder scheduling logic)
- [x] 1.4 Wire up services in `AquavateApp.swift`
- [x] 1.5 Update `SettingsView.swift` to use NotificationManager
- [x] 1.6 Add reference in `BLEManager.swift` to trigger after drink sync

#### Phase 2: App Groups for Shared Data ✅
- [x] 2.1 Add App Groups entitlement to `Aquavate.entitlements`
- [x] 2.2 Create `SharedDataManager.swift`

#### Phase 3: WatchConnectivity ✅
- [x] 3.1 Create `WatchConnectivityManager.swift` (iPhone side)

#### Phase 4: Apple Watch App ✅
- [x] 4.1 Create Watch target in Xcode (AquavateWatch)
- [x] 4.2 Create Watch home view with large colored water drop
- [x] 4.3 Create complication provider (graphicCircular, graphicCorner, graphicRectangular)
- [x] 4.4 Create `WatchSessionManager.swift` (Watch side)
- [x] 4.5 Add App Groups entitlement to Watch target

#### Phase 5: Integration ✅
- [x] 5.1 Update iPhone Info.plist with WKCompanionAppBundleIdentifier
- [x] 5.2 Wire up state updates to SharedDataManager and WatchConnectivity

#### Testing
- [ ] Test notifications with test mode (1 min intervals)
- [ ] Test quiet hours
- [ ] Test max 8 reminders
- [ ] Test escalation model (amber → red)
- [ ] Test goal achievement stops reminders
- [ ] Test Watch complication color changes
- [ ] Test Watch app display
- [ ] Test Watch ↔ iPhone sync

### Current Status: Watch App Running on Simulator ✅

**Build successful!** Both iPhone and Watch apps now compile and run.

**Completed Xcode Setup:**
1. ✅ Add new iPhone files to Aquavate target (Models/HydrationState.swift, Services/*.swift)
2. ✅ Add Info.plist WKCompanionAppBundleIdentifier
3. ✅ Add App Groups entitlement to Aquavate.entitlements
4. ✅ Create Watch target in Xcode (AquavateWatch)
5. ✅ Add Watch source files to Watch target
6. ✅ Share HydrationState.swift and SharedDataManager.swift with Watch target
7. ✅ Configure App Groups capability for Watch target
8. ✅ Fix build errors (added Combine import, fixed @StateObject singleton usage)
9. ✅ Set watchOS deployment target to 11.0

**Build Fixes Applied:**
- Added `import Combine` to WatchSessionManager.swift (required for @Published)
- Removed `@MainActor` from WatchSessionManager class (conflicted with WCSessionDelegate)
- Changed AquavateWatchApp to use `.environmentObject(WatchSessionManager.shared)` directly instead of @StateObject

**Real Device Status:**
- Real Watch deployment has tunnel connection issues (common watchOS dev problem)
- Workaround: Using Watch simulator paired with iPhone simulator for testing

**Next:** Test notifications and Watch functionality on simulators.

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

### Files Modified
- `ios/Aquavate/Aquavate/AquavateApp.swift` ✅
- `ios/Aquavate/Aquavate/Services/BLEManager.swift` ✅
- `ios/Aquavate/Aquavate/Views/SettingsView.swift` ✅
- `ios/Aquavate/Aquavate/Aquavate.entitlements` ✅
- `ios/Aquavate/Info.plist` ✅
- `ios/Aquavate/AquavateWatch Watch App/AquavateWatch/WatchSessionManager.swift` ✅ (build fixes)
- `ios/Aquavate/AquavateWatch Watch App/AquavateWatch/AquavateWatchApp.swift` ✅ (build fixes)

---

## Context Recovery

To resume from this progress file:
```
Continue from PROGRESS.md - Issue #27 (Hydration Reminders + Apple Watch App).
Watch app now builds and runs on simulator. Next: test notifications and Watch ↔ iPhone sync.
Plan: Plans/036-watch-hydration-reminders.md
Branch: watch-hydration-reminders
```

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
- `watch-hydration-reminders` - Current work (Issue #27)

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
