# Aquavate - Active Development Progress

**Last Updated:** 2026-01-28 (Session 12)
**Current Branch:** `daily-goal-setting`
**GitHub Issue:** #83

---

## Current Task

**Daily Goal Setting from iOS App (Issue #83)** - [Plan 063](Plans/063-daily-goal-setting.md)

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
- [ ] **Testing**: Upload firmware, test on device

### Key Decisions
- UI: Stepper with ±250ml steps
- Range: 1000-4000ml
- Offline: Require connection (stepper disabled when disconnected)

### Files Modified
- `firmware/src/storage.cpp` - Added `storageSaveDailyGoal()`, `storageLoadDailyGoal()`
- `firmware/include/storage.h` - Added function declarations
- `firmware/src/ble_service.cpp` - Updated `bleLoadBottleConfig()`, `bleSaveBottleConfig()`, calls `displaySetDailyGoal()` on config write
- `firmware/src/config.h` - Added `DRINK_DAILY_GOAL_MIN_ML`, `DRINK_DAILY_GOAL_MAX_ML`, `DRINK_DAILY_GOAL_DEFAULT_ML`
- `firmware/src/display.cpp` - Added `g_daily_goal_changed` flag, updated `displaySetDailyGoal()` to set flag, updated `displayNeedsUpdate()` to check flag
- `firmware/src/serial_commands.cpp` - Updated default constant reference
- `firmware/include/ble_service.h` - Updated comment
- `ios/.../SettingsView.swift` - Added Stepper UI for goal setting
- `ios/.../BLEManager.swift` - Added `setDailyGoal()` method with debounce, updates HydrationReminderService

### Important Note
Issue #84 (calibration circular dependency fix) is now merged into this branch. This means:
- Firmware now validates incoming BLE scale_factor (100-800 ADC/g range)
- iOS app must use read-modify-write pattern when updating BottleConfig
- See [Plan 062](Plans/062-calibration-circular-dependency.md) for iOS app guidance

---

## Recently Completed

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
Resume from PROGRESS.md - working on daily goal setting (Issue #83), code complete, ready for device testing. Master merged (includes Issue #84 fix).
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
