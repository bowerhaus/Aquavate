# Aquavate - Active Development Progress

**Last Updated:** 2026-02-08 (Session 35)
**Current Branch:** `master`

---

## Current Task

None — ready for next task.

---

## Recently Completed

- **Remove Redundant Single-Tap Wake Interrupt** - [Plan 072](Plans/072-remove-redundant-single-tap-wake.md) ✅ COMPLETE — Single-tap wake interrupt was redundant with activity wake in normal deep sleep (activity 1.5g < tap 3.0g). Removed single-tap from INT_ENABLE (0x70→0x30), kept activity + double-tap. Changed backpack wake screen text from "waking" to "waking up". PRD updated.
- **Simplify Boot/Wake Serial Log Output (Issue #108)** - [Plan 071](Plans/071-simplify-boot-wake-serial-log.md) ✅ COMPLETE — Reduced boot/wake serial output from ~90 lines to ~20 lines. Wrapped verbose messages in DEBUG_PRINTF across 7 files (main.cpp, config.h, storage.cpp, display.cpp, drinks.cpp, activity_stats.cpp, storage_drinks.cpp). Separated gesture+countdown status line (unconditional, every 3s) from accel debug (d4+). Enabled serial commands in IOS_MODE (both BLE + serial fit in IRAM with ~9.7KB headroom). Fixed display.cpp DEBUG_PRINTF(1,...) bug. All debug category defaults set to 0; `d0`-`d9` runtime control available.
- **Fix: False wakes from table nudges (Issue #110)** - [Plan 070](Plans/070-reduce-single-tap-sensitivity.md) ✅ COMPLETE — Table nudges triggered false wakes from normal sleep. Root cause was activity interrupt threshold (0.5g), not single-tap. Fix: increased `ACTIVITY_WAKE_THRESHOLD` from 0x08 (0.5g) to 0x18 (1.5g). Tap threshold unchanged. PRD updated.
- **Fix: Auto-recovery after battery depletion (Issue #107)** - [Plan 069](Plans/069-battery-depletion-recovery.md) ✅ COMPLETE
- **Fix False Double-Tap Triggering Backpack Mode (Issue #103)** - [Plan 068](Plans/068-false-double-tap-backpack-fix.md) ✅ COMPLETE
- **Fix Backpack Mode Entry (Issue #97)** - [Plan 067](Plans/067-backpack-mode-entry-fix.md) ✅ COMPLETE

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md — no active task. Plan 072 complete on branch remove-redundant-single-tap-wake (PR pending merge).
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
