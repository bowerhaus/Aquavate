# Aquavate - Active Development Progress

**Last Updated:** 2026-04-17 (Session 40)
**Current Branch:** `master`

---

## Current Task

No active task. Ready for next issue.

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md
```

---

## Recently Completed

- **Fix Timezone / Daily Reset & BLE Sync Corruption (Issue #120)** - [Plan 077](Plans/077-fix-timezone-daily-reset.md) ✅ COMPLETE — Daily reset was firing at UTC midnight (1am BST instead of midnight BST), and a race condition could corrupt BLE sync data during the UTC/BST gap. Fix: iOS now sends timezone offset (hours) alongside UTC timestamp in SET_TIME (5→6 bytes); firmware stores it and uses it only for reset boundary math; all record timestamps remain true UTC. `getCurrentUnixTime()` returns pure UTC; `getTodayResetTimestamp()` and `getSecondsUntilRollover()` compute local midnight as UTC via signed arithmetic. Added `volatile g_ble_sync_in_progress` guard (set at sync START, cleared at COMPLETE/error/disconnect) to prevent daily reset mid-sync. E-paper sleep display and rollover wake check now use local time. 6 files changed (4 firmware, 2 iOS). Deploy: flash firmware first, then iOS — 6-byte SET_TIME is not backward compatible.
- **Fix App Crash in Sleep Mode Analysis (Issue #105)** - [Plan 076](Plans/076-fix-sleep-mode-analysis-crash.md) ✅ COMPLETE — CoreData `timerWakeCount` (Int16) overflowed when receiving UInt16 from firmware (max 65,535 vs 32,767). Widened `CDBackpackSession.timerWakeCount` and `CDMotionWakeEvent.durationSec` from Integer 16 to Integer 32 in CoreData model v2 (lightweight migration). Updated Int16→Int32 conversions in PersistenceController, ActivityStatsView enum, and BackupModels. Also fixed activity stats sync to merge instead of clear-and-replace, preserving historical data across firmware updates. 6 iOS files changed.
- **Low Battery Lockout (Issue #68)** - [Plan 075](Plans/075-low-battery-lockout.md) ✅ COMPLETE — Two-tier battery warning: iOS early warning at 25% (BLE flag + push notification + red badge), firmware lockout at 20% (full-screen "charge me", timer-only deep sleep with 15-min health checks). Recovery at 25% with hysteresis. Threshold runtime-configurable via `SET BATTERY LOCKOUT THRESHOLD` serial command, persisted in NVS. 9 firmware files + 4 iOS files changed. PRD and IOS-UX-PRD updated.
- **Fix: Drink not detected when bottle is emptied (Issue #116)** - [Plan 074](Plans/074-ble-set-time-baseline-fix.md) ✅ COMPLETE — BLE SET_TIME handler was calling `drinksInit()` on every connection, zeroing the drink detection baseline. If this happened while holding the bottle, the drink was invisible. Fix: added `drinksIsInitialized()` guard, removed forced baseline zero in `drinksInit()`, moved RTC restore before wakeup guard (survives EN-pin resets), added NVS save in `drinksSaveToRTC()` for better power-cycle fallback. Also excluded SET_TIME from activity timeout reset (was adding 30s unnecessary awake time). 4 firmware files changed.
- **Foreground Auto-Reconnection to Bottle (Issue #114)** - [Plan 073](Plans/073-foreground-auto-reconnection.md) ✅ COMPLETE — Used `CBCentralManager.connect()` in foreground (same as background) so app auto-connects when bottle advertises without manual pull-to-refresh. Renamed all "background reconnect" APIs to "auto-reconnect". Single file change: BLEManager.swift. PRD and IOS-UX-PRD updated.
- **Remove Redundant Single-Tap Wake Interrupt** - [Plan 072](Plans/072-remove-redundant-single-tap-wake.md) ✅ COMPLETE — Single-tap wake interrupt was redundant with activity wake in normal deep sleep (activity 1.5g < tap 3.0g). Removed single-tap from INT_ENABLE (0x70→0x30), kept activity + double-tap. Changed backpack wake screen text from "waking" to "waking up". PRD updated.
- **Simplify Boot/Wake Serial Log Output (Issue #108)** - [Plan 071](Plans/071-simplify-boot-wake-serial-log.md) ✅ COMPLETE — Reduced boot/wake serial output from ~90 lines to ~20 lines. Wrapped verbose messages in DEBUG_PRINTF across 7 files (main.cpp, config.h, storage.cpp, display.cpp, drinks.cpp, activity_stats.cpp, storage_drinks.cpp). Separated gesture+countdown status line (unconditional, every 3s) from accel debug (d4+). Enabled serial commands in IOS_MODE (both BLE + serial fit in IRAM with ~9.7KB headroom). Fixed display.cpp DEBUG_PRINTF(1,...) bug. All debug category defaults set to 0; `d0`-`d9` runtime control available.
- **Fix: False wakes from table nudges (Issue #110)** - [Plan 070](Plans/070-reduce-single-tap-sensitivity.md) ✅ COMPLETE — Table nudges triggered false wakes from normal sleep. Root cause was activity interrupt threshold (0.5g), not single-tap. Fix: increased `ACTIVITY_WAKE_THRESHOLD` from 0x08 (0.5g) to 0x18 (1.5g). Tap threshold unchanged. PRD updated.
- **Fix: Auto-recovery after battery depletion (Issue #107)** - [Plan 069](Plans/069-battery-depletion-recovery.md) ✅ COMPLETE
- **Fix False Double-Tap Triggering Backpack Mode (Issue #103)** - [Plan 068](Plans/068-false-double-tap-backpack-fix.md) ✅ COMPLETE
- **Fix Backpack Mode Entry (Issue #97)** - [Plan 067](Plans/067-backpack-mode-entry-fix.md) ✅ COMPLETE

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
