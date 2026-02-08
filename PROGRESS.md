# Aquavate - Active Development Progress

**Last Updated:** 2026-02-08 (Session 32)
**Current Branch:** `master`

---

## Current Task

None — ready for next task.

---

## Recently Completed

- **Fix: False wakes from table nudges (Issue #110)** - [Plan 070](Plans/070-reduce-single-tap-sensitivity.md) ✅ COMPLETE — Table nudges triggered false wakes from normal sleep. Root cause was activity interrupt threshold (0.5g), not single-tap. Fix: increased `ACTIVITY_WAKE_THRESHOLD` from 0x08 (0.5g) to 0x18 (1.5g). Tap threshold unchanged. PRD updated.
- **Fix: Auto-recovery after battery depletion (Issue #107)** - [Plan 069](Plans/069-battery-depletion-recovery.md) ✅ COMPLETE — ESP32 could get stuck in deep sleep after battery depletion because ADXL343 loses interrupt config while ESP32 RTC domain persists. Fix: periodic health-check timer wake (every 2 hours) added to both normal and extended deep sleep modes. Device boots normally on health-check wake, auto-sleeps after 30s. Battery impact ~1mAh/day. PRD updated.
- **Fix False Double-Tap Triggering Backpack Mode (Issue #103)** - [Plan 068](Plans/068-false-double-tap-backpack-fix.md) ✅ COMPLETE — Setting bottle down on hard surface created bounce pattern matching ADXL343 double-tap, falsely entering backpack mode. Fix: gate double-tap handler on `g_has_been_upright_stable` flag — bottle must have settled on surface (2s stability) before double-tap can trigger backpack mode. ~5 lines changed.
- **Fix Backpack Mode Entry (Issue #97)** - [Plan 067](Plans/067-backpack-mode-entry-fix.md) ✅ COMPLETE — Backpack mode never entered when bottle horizontal. Root cause: per-cycle flags/counters couldn't track state across 30s wake/sleep cycles. Fix: check current gesture (`gesture != GESTURE_UPRIGHT_STABLE`) at sleep time — if not on surface, stay awake and let 180s backpack timer handle it. Four approaches tried; final solution is zero new state, net -1 line.
- **Fix ADXL343 Register Addresses (Issue #98)** - [Plan 066](Plans/066-fix-adxl343-register-addresses.md) ✅ COMPLETE — Fixed THRESH_ACT register (0x1C → 0x24), separated activity threshold (0.5g) from tap threshold (3.0g), updated PRD.
- **Double-Tap to Enter Backpack Mode (Issue #99)** - [Plan 065](Plans/065-double-tap-to-sleep.md) ✅ COMPLETE — Double-tap gesture to manually enter extended deep sleep. ADXL343 hardware detection, same 3.0g threshold as wake. PRD updated.
- **Import/Export Backup (Issue #93)** - [Plan 064](Plans/064-import-export.md) ✅ COMPLETE — JSON backup export/import with Merge and Replace modes. New "Data" category in Settings. iOS-UX-PRD updated.
- **Display Redraw on Wake Fix (Issue #88)** - [Plan 063](Plans/063-display-redraw-on-wake.md) ✅ COMPLETE — Persisted daily goal in RTC memory, removed dual flag check in displayNeedsUpdate().
- **Settings Page Redesign (Issue #87)** - Plan 063 ✅ COMPLETE (PR #89) — Option 5 (Smart Contextual sub-pages) selected. Keep-alive fix, Health/Notification status flags, error message cleanup. iOS-UX-PRD updated.
- **Daily Goal Setting from iOS App (Issue #83)** - [Plan 062](Plans/062-daily-goal-setting.md) ✅ COMPLETE (PR #86)
- **Calibration Circular Dependency Fix (Issue #84)** - [Plan 062](Plans/062-calibration-circular-dependency.md) ✅ COMPLETE (PR #85)
- **Faded Blue Behind Indicator (Issue #81)** - [Plan 061](Plans/061-faded-blue-behind-indicator.md) ✅ COMPLETE
- **iOS Calibration Flow (Issue #30)** - [Plan 060](Plans/060-ios-calibration-flow.md) ✅ COMPLETE
- **LittleFS Drink Storage / NVS Fragmentation Fix (Issue #76)** - [Plan 059](Plans/059-littlefs-drink-storage.md)
- Drink Baseline Hysteresis Fix (Issue #76) - [Plan 057](Plans/057-drink-baseline-hysteresis.md)
- Unified Sessions View Fix (Issue #74) - [Plan 056](Plans/056-unified-sessions-view.md)

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md — No active task. Ready for next task. Branch: master.
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
