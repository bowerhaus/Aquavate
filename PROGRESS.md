# Aquavate - Active Development Progress

**Last Updated:** 2026-01-28 (Session 12)
**Current Branch:** `master`
**GitHub Issue:** None active

---

## Current Task

No active task - ready for next issue.

---

## Recently Completed

- **Calibration Circular Dependency Fix (Issue #84)** - [Plan 062](Plans/062-calibration-circular-dependency.md) ✅ COMPLETE
  - Fixed circular dependency preventing recalibration with corrupt calibration data
  - Added scale factor bounds validation (100-800 ADC/g) in config.h
  - Added accelerometer-only stability mode for calibration (`gesturesSetCalibrationMode()`)
  - Validated incoming BLE BottleConfig to prevent corruption
  - Validated scale factor on load in storage.cpp
  - Updated CLAUDE.md to reflect standalone calibration enabled in both modes
  - Plan includes iOS app guidance for read-modify-write pattern

- **Faded Blue Behind Indicator (Issue #81)** - [Plan 061](Plans/061-faded-blue-behind-indicator.md) ✅ COMPLETE
- **iOS Calibration Flow (Issue #30)** - [Plan 060](Plans/060-ios-calibration-flow.md) ✅ COMPLETE
- **LittleFS Drink Storage / NVS Fragmentation Fix (Issue #76)** - [Plan 059](Plans/059-littlefs-drink-storage.md)
- Drink Baseline Hysteresis Fix (Issue #76) - [Plan 057](Plans/057-drink-baseline-hysteresis.md)
- Unified Sessions View Fix (Issue #74) - [Plan 056](Plans/056-unified-sessions-view.md)

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md - no active task
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
