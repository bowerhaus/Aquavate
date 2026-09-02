# Aquavate - Active Development Progress

**Last Updated:** 2026-09-02 (Session 44)
**Current Branch:** `epaper-fading-fix` (branched from `master`)

---

## Current Task

**None — branch `epaper-fading-fix` is complete and ready for PR.**

Changes are unstaged (user stages manually). PR description must include
`Closes #124`.

### What was done

**Fix E-Paper Display Fading (Issue #124)** — [Plan 079](Plans/079-epaper-fading-fix.md)

Corrected how the SSD1680 panel is driven, in a new `AquavateEPD` subclass
([firmware/include/aquavate_epd.h](firmware/include/aquavate_epd.h)) plus one
declaration change in main.cpp — the whole thing reverts in a single step:

- **`0xF4` → `0xF7`** — the panel is no longer left DC-biased between refreshes.
  Adafruit's value omits the disable-analog/disable-clock bits, so ±15/20 V
  stayed on the panel for hours overnight and days in backpack mode.
- **VSH2 `0x00` → `0xA8`** — Adafruit sends an out-of-range source voltage.
- **`0x18 0x80`** — select the internal temperature sensor (POR default is an
  external one that isn't fitted).
- **Blind refresh wait 1500 → 2500 ms**, runtime-tunable.
- **Refresh instrumentation** — `rtc_refresh_count` + `displayRefreshPanel()`.
  All 16 refresh call sites route through the helper; keep it that way or the
  tally silently undercounts.
- **`EPD *` serial diagnostics** — `PATTERN`, `TEST`, `WAIT`, `TEMP`, `VCOM`,
  `LUT`. ~3.5 KB flash, no IRAM, none persisted.

Build: Flash 61.2%, RAM 11.7%, no IRAM pressure. Native unit tests not run —
no changes to calibration.cpp / weight.cpp / drinks.cpp.

### Outcome — display recovered, panel NOT replaced

Replacement was recommended after every firmware lever was exhausted, then the
display recovered and the user chose to stop. **Which change fixed it is not
established** — three live candidates: `0xF7` stopping the idle decay, the many
`EPD TEST` flush cycles acting as conditioning, or ambient temperature (the
fridge test showed large real temperature sensitivity). Phase 5's controlled A/B
never ran.

**If it degrades again, follow the playbook in Plan 079 → "If the display
degrades again".** It has the ordered steps and, importantly, the table of
levers already swept with no effect (VCOM at every value, forced temperature
bins, timing, battery, SRAM) so the same ground isn't covered twice. The one
lever that did change behaviour was the waveform LUT.

### Recommended next (not done, user's call)

1. **Phase 3 — reduce refresh count.** Highest value remaining. A 5 ml threshold
   against ±10–15 ml sensor accuracy with no hysteresis, checked every 5 s, means
   noise alone triggers full refreshes on a stationary bottle. Fewer refreshes =
   less panel wear.
2. **Scheduled conditioning cycle** — directly indicated if the flush cycles are
   what recovered the panel.
3. **Overnight idle-stability test** — the clean pass/fail on whether `0xF7`
   fixed the decay mechanism.
4. **Upstream issue** against `adafruit/Adafruit_EPD` (Findings 1–3 affect every
   user of this panel).

### Files changed

- `firmware/include/aquavate_epd.h` — **new.** `AquavateEPD`: corrected init
  (two VCOM variants), `0xF7`, tunable blind wait, forced temperature bin, two
  experimental waveform LUTs (both annotated with their tested results)
- `firmware/include/display.h` / `firmware/src/display.cpp` — `rtc_refresh_count`,
  `displayRefreshPanel()`, 5 call sites
- `firmware/src/ui_calibration.cpp` — 11 call sites
- `firmware/src/main.cpp` — 2 call sites, `AquavateEPD` declaration, boot-log
  refresh count, `epdContrastTest()`, `epdDiagnosticPattern()`, EPD accessors
- `firmware/src/serial_commands.cpp` — six `EPD *` handlers, dispatch, help

### Notes

- User stages changes manually — do not `git add` automatically.

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md
```

---

## Recently Completed

- **Fix E-Paper Display Fading (Issue #124)** - [Plan 079](Plans/079-epaper-fading-fix.md) ✅ COMPLETE (pending PR) — Display had degraded to salt-and-pepper speckle reading as grey "fading". Root cause identified as the Adafruit EPD driver leaving the panel DC-biased between refreshes: `display()` never powers down and `_display_update_val = 0xF4` omits the disable-analog/disable-clock bits, so ±15/20 V stayed on the panel for hours overnight and days in backpack mode. New `AquavateEPD` subclass (`firmware/include/aquavate_epd.h`) + one declaration change: `0xF4`→`0xF7` (controller powers itself down as the final waveform step), VSH2 `0x00`→`0xA8` (Adafruit sends an out-of-range source voltage), `0x18 0x80` (internal temp sensor — POR default is an external one that isn't fitted), blind refresh wait 1500→2500 ms. Added `rtc_refresh_count` instrumentation via a `displayRefreshPanel()` helper that all 16 refresh call sites route through, and six `EPD *` serial diagnostics (`PATTERN`/`TEST`/`WAIT`/`TEMP`/`VCOM`/`LUT`). Display recovered; panel NOT replaced. **Attribution unresolved** — `0xF7`, the many `EPD TEST` flush cycles (conditioning), or ambient temperature could each explain it. Swept with no effect: VCOM at every value in both waveform modes, forced temperature bins, timing, battery, SRAM. Not done: Phase 3 refresh-count reduction, conditioning cycle, overnight idle test. 6 files changed (1 new).
- **Introduce Unit Tests** - [Plan 078](Plans/078-introduce-unit-tests.md) ✅ COMPLETE — Added minimum viable unit tests to firmware and iOS app. Firmware: PlatformIO `[env:native]` with Unity framework; host stubs for Arduino.h/NVS/RTC_DATA_ATTR; injectable `getCurrentUnixTime()`; 3 test suites (`test_calibration`, `test_weight`, `test_drinks`) — 30/30 green. iOS: XCTest target `AquavateTests`; injectable `now: () -> Date` on `HydrationReminderService`; `HydrationReminderServiceTests.swift` covering pace, deficit, urgency, throttle reset — 16/16 green. CLAUDE.md and AGENTS.md updated with test run instructions and pre-PR checklist.
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
