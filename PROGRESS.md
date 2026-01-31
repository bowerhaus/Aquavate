# Aquavate - Active Development Progress

**Last Updated:** 2026-01-31 (Session 25)
**Current Branch:** `backpack-mode-entry-fix`

---

## Current Task

**Fix Backpack Mode Entry (Issue #97)** - [Plan 067](Plans/067-backpack-mode-entry-fix.md)

Backpack mode never entered when bottle is horizontal. Root cause: `g_time_since_stable_start` reset on every motion wake (main.cpp:662), making the 180s threshold unreachable across short 30s wake/sleep cycles.

**Solution:** Track consecutive non-stable wake cycles. After 4 wakes without UPRIGHT_STABLE / drink / BLE activity, enter extended sleep instead of normal sleep.

### Progress
- [x] Plan created (067-backpack-mode-entry-fix.md)
- [x] Branch created (`backpack-mode-entry-fix` from `master`)
- [x] GitHub issue created (#97)
- [x] Add RTC variable (`rtc_spurious_wake_count`) and config constant (`SPURIOUS_WAKE_THRESHOLD`)
- [x] Modify wake handling in setup() — init `g_wake_was_useful`, preserve/reset counter
- [x] Add useful-wake markers at UPRIGHT_STABLE, drink detection, BLE activity
- [x] Add spurious wake check before sleep entry — redirect to extended sleep after threshold
- [x] Build firmware (SUCCESS — IRAM 94.0%, RAM 11.6%, Flash 59.2%)
- [ ] User testing

### Files to Modify
- `firmware/src/main.cpp` — RTC var, wake handling, useful markers, sleep entry decision
- `firmware/src/config.h` — `SPURIOUS_WAKE_THRESHOLD` constant

---

## Recently Completed

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
Resume from PROGRESS.md — Working on: Fix backpack mode entry (Issue #97, Plan 067). Branch: backpack-mode-entry-fix. Solution: spurious wake counter — track consecutive non-stable wakes, enter extended sleep after 4. Check progress checklist above for current step.
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
