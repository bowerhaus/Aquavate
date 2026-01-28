# Aquavate - Active Development Progress

**Last Updated:** 2026-01-28 (Session 13)
**Current Branch:** `daily-goal-setting`
**GitHub Issue:** #83

---

## Current Task

**No active task** - Ready for next issue or PR creation for Issue #83

---

## Recently Completed (This Session)

**Daily Goal Setting from iOS App (Issue #83)** - [Plan 062](Plans/062-daily-goal-setting.md) ✅ COMPLETE

Allow users to configure their daily hydration goal from the iOS app Settings screen. Goal transfers to bottle via BLE and persists in NVS.

### Progress

- [x] Plan created and approved
- [x] Branch created: `daily-goal-setting`
- [x] **Phase 1: Firmware - NVS Persistence**
  - [x] 1.1 Add storage functions in storage.cpp
  - [x] 1.2 Add declarations in storage.h
  - [x] 1.3 Update BLE service save/load
  - [x] 1.4 Add constants in config.h
- [x] **Phase 2: iOS - Settings UI**
  - [x] 2.1 Update SettingsView with Stepper
  - [x] 2.2 Add setDailyGoal() to BLEManager
- [x] Build and verify firmware
- [x] Build and verify iOS
- [x] Merged master (includes Issue #84 calibration fix)
- [x] **Phase 3: iOS - Fix Read-Modify-Write** (prevents calibration corruption)
  - [x] 3.1 Add calibration storage properties (lastKnownScaleFactor, lastKnownTareWeightGrams)
  - [x] 3.2 Update handleBottleConfigUpdate() to store calibration values
  - [x] 3.3 Fix writeBottleConfig() to use stored calibration values
  - [x] 3.4 Reset calibration values on disconnect
- [x] Build and verify iOS (passed)
- [x] **Phase 4: Display Update & UI Improvements**
  - [x] 4.1 Fix e-paper not updating on goal change (was gated by UPRIGHT_STABLE gesture)
  - [x] 4.2 Replace Stepper with dialog/picker (single BLE write on confirm)
- [x] Build and verify firmware (passed)
- [x] Build and verify iOS (passed)
- [x] **Testing**: Upload firmware, test on device - ALL TESTS PASS

### Key Decisions
- UI: Dialog with Picker (select value, tap Save - single BLE write)
- Range: 1000-4000ml in 250ml steps
- Offline: Require connection (row disabled when disconnected)

### Files Modified
- `firmware/src/storage.cpp` - Added `storageSaveDailyGoal()`, `storageLoadDailyGoal()`
- `firmware/include/storage.h` - Added function declarations
- `firmware/src/ble_service.cpp` - Updated `bleLoadBottleConfig()`, `bleSaveBottleConfig()`, calls `displaySetDailyGoal()` on config write
- `firmware/src/config.h` - Added `DRINK_DAILY_GOAL_MIN_ML`, `DRINK_DAILY_GOAL_MAX_ML`, `DRINK_DAILY_GOAL_DEFAULT_ML`
- `firmware/src/display.cpp` - Added `g_daily_goal_changed` flag, `displaySetDailyGoal()`, **`displayCheckGoalChanged()`**
- `firmware/include/display.h` - Added `displayCheckGoalChanged()` declaration
- `firmware/src/main.cpp` - **Added goal change check outside gesture block to force display update**
- `firmware/src/serial_commands.cpp` - Updated default constant reference
- `firmware/include/ble_service.h` - Updated comment
- `ios/.../SettingsView.swift` - **Changed from Stepper to dialog with Picker** (single BLE write on confirm)
- `ios/.../BLEManager.swift` - Added `setDailyGoal()` method with debounce, updates HydrationReminderService, **fixed read-modify-write pattern**

### Issue Resolved: Calibration Corruption on Goal Setting
When testing, setting the daily goal corrupted calibration data because `writeBottleConfig()` was sending hardcoded `scaleFactor: 1.0`. The firmware (with Issue #84 fix) rejected this invalid value, so the goal was never saved.

**Fix (Phase 3)**: iOS app now stores calibration values (`lastKnownScaleFactor`, `lastKnownTareWeightGrams`) when reading bottle config on connection, and preserves them when writing config updates. This implements the read-modify-write pattern recommended in Plan 062.

---

## Recently Completed

- **Daily Goal Setting from iOS App (Issue #83)** - [Plan 062](Plans/062-daily-goal-setting.md) ✅ COMPLETE (ready for PR)
- **Calibration Circular Dependency Fix (Issue #84)** - [Plan 062](Plans/062-calibration-circular-dependency.md) ✅ COMPLETE (PR #85)
  - Fixed circular dependency preventing recalibration with corrupt calibration data
  - Added scale factor bounds validation (100-800 ADC/g) in config.h
  - Added accelerometer-only stability mode for calibration (`gesturesSetCalibrationMode()`)
  - Validated incoming BLE BottleConfig to prevent corruption
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
Resume from PROGRESS.md - daily goal setting (Issue #83) COMPLETE.

Session 13 completed:
- All phases complete, device testing passed
- Ready to create PR for Issue #83 and merge to master
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
