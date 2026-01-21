# Aquavate - Active Development Progress

**Last Updated:** 2026-01-20
**Current Branch:** `swipe-to-delete-drinks`

---

## Current Status

**COMPLETE:** Sync Daily Total to Bottle on Drink Deletion - [Plan](~/.claude/plans/moonlit-exploring-knuth.md)

### Summary
When a drink is deleted in the iOS app, the new daily total is synced to the bottle so the e-paper display matches the app.

### Implementation Complete

**Firmware:**
- ✅ `BLE_CMD_SET_DAILY_TOTAL = 0x11` command handler
- ✅ `bleCheckSetDailyTotalRequested()` checker function
- ✅ Main loop calls `drinksSetDailyIntake()` when flag set
- ✅ `bleCheckForceDisplayRefresh()` forces display update when daily total is set via BLE

**iOS App:**
- ✅ `BLECommand.setDailyTotal(ml:)` in BLEStructs.swift
- ✅ `sendSetDailyTotalCommand(ml:)` in BLEManager.swift
- ✅ HomeView calls sync after `deleteTodaysDrinks(at:)`
- ✅ HistoryView calls sync after `deleteDrinks(at:)` (only for today's drinks)
- ✅ `syncDailyTotalIfNeeded()` compares app/bottle totals on connect and syncs if different
- ✅ Auto-sync on connect: if app's CoreData total differs from bottle's total, sends SET_DAILY_TOTAL

### Key Files Modified
- `firmware/include/ble_service.h` - command constant, bleCheckForceDisplayRefresh declaration
- `firmware/src/ble_service.cpp` - command handler, force display refresh flag
- `firmware/src/main.cpp` - flag checks for daily total and display refresh
- `ios/.../Services/BLEStructs.swift` - setDailyTotal command builder
- `ios/.../Services/BLEManager.swift` - sendSetDailyTotalCommand, syncDailyTotalIfNeeded methods
- `ios/.../Views/HomeView.swift` - sync after deletion
- `ios/.../Views/HistoryView.swift` - sync after deletion (today only)

---

## Bug Fix Also Applied This Session

**HomeView drinks list fix:**
- Changed from showing only 5 "recent" drinks to showing ALL today's drinks
- Removed problematic `List` with `scrollDisabled(true)` and fixed height
- Now uses same pattern as HistoryView - proper `List` with `Section`, swipe-to-delete works
- Renamed section from "Recent Drinks" to "Today's Drinks"
- `displayDailyTotal` now always calculates from CoreData sum (not BLE value when disconnected)

---

## Recently Completed

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

- `swipe-to-delete-drinks` - **ACTIVE**: Current work
- `master` - Stable baseline

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
