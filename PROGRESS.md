# Aquavate - Active Development Progress

**Last Updated:** 2026-01-27 (Session 8)
**Current Branch:** `ios-calibration-flow`

---

## Current Task

**iOS Calibration Flow (Issue #30)** - [Plan 060](Plans/060-ios-calibration-flow.md) **✅ COMPLETE - READY FOR MERGE**

### Architecture: Bottle-Driven, iOS-Mirrored

```
┌─────────────────┐                    ┌─────────────────┐
│    iOS App      │                    │     Bottle      │
├─────────────────┤                    ├─────────────────┤
│                 │  START_CALIBRATION │                 │
│  "Calibrate"    │ ──────────────────>│ Enters calib    │
│   button tap    │                    │ state machine   │
│                 │                    │                 │
│                 │  STATE_NOTIFY      │ Detects stable  │
│  Updates rich   │ <─────────────────-│ upright, runs   │
│  UI to mirror   │  (state + weights) │ existing logic  │
│  bottle state   │                    │                 │
│                 │  CANCEL_CALIB      │                 │
│  Cancel button  │ ──────────────────>│ Returns to idle │
│                 │                    │                 │
│  User watches   │                    │ E-paper shows   │
│  phone screen   │                    │ simple version  │
└─────────────────┘                    └─────────────────┘
```

### Implementation Status

#### Firmware Phase ✓ COMPLETE

- [x] **config.h** - Enable standalone calibration in IOS_MODE ✓
- [x] **ble_service.h** - Added calibration UUID, struct, command codes ✓
- [x] **ble_service.cpp** - CalibrationState characteristic + command handlers ✓
- [x] **main.cpp** - BLE calibration integration with state machine ✓
- [x] **Build verified** - 38,088 bytes RAM (11.6%), 772,705 bytes Flash (59.0%) ✓

**Build size change:** +8 bytes RAM, +972 bytes Flash (minimal overhead)

#### iOS Phase ✓ COMPLETE

- [x] **iOS: Add BLE constants** - Added `calibrationStateUUID`, `startCalibration`/`cancelCalibration` commands ✓
- [x] **iOS: Add CalibrationState parsing** - Added `BLECalibrationState` struct ✓
- [x] **iOS: Add BLEManager methods** - `startCalibration()`, `cancelCalibration()`, `handleCalibrationStateUpdate()` ✓
- [x] **iOS: Subscribe to calibration notifications** - Added to characteristic subscription list ✓
- [x] **iOS: Update CalibrationManager** - Added bottle-driven mode with state mirroring ✓
- [x] **iOS: Update CalibrationView** - Simplified to 4-screen flow (Welcome → Empty → Full → Complete) ✓
- [x] **iOS: Simplified UI flow** - Removed intermediate confirmation screens, auto-transitions ✓

### Files Modified (Sessions 6-8)

**Firmware:**
- `firmware/include/ble_service.h` - Added `AQUAVATE_CALIBRATION_STATE_UUID`, `BLE_CMD_START_CALIBRATION` (0x20), `BLE_CMD_CANCEL_CALIBRATION` (0x21), `BLE_CalibrationState` struct
- `firmware/src/ble_service.cpp` - Added CalibrationState characteristic, command handlers, `bleNotifyCalibrationState()`
- `firmware/src/main.cpp` - Integrated BLE calibration start/cancel with existing state machine

**iOS:**
- `ios/Aquavate/Aquavate/Services/BLEConstants.swift` - Added `calibrationStateUUID`, `CalibrationStep` enum, updated command codes
- `ios/Aquavate/Aquavate/Services/BLEStructs.swift` - Added `BLECalibrationState` struct, `startCalibration()`/`cancelCalibration()` commands
- `ios/Aquavate/Aquavate/Services/BLEManager.swift` - Added `calibrationState` published property, `startCalibration()`, `cancelCalibration()`, `handleCalibrationStateUpdate()`
- `ios/Aquavate/Aquavate/Services/CalibrationManager.swift` - Added `CalibrationMode` enum, bottle-driven mode with state mirroring, auto-transitions
- `ios/Aquavate/Aquavate/Views/CalibrationView.swift` - Simplified to 4-screen flow: Welcome → Empty → Full → Complete

**Note:** Activity stats commands renumbered from 0x21-0x23 to 0x30-0x32 to make room for calibration commands.

### UI Simplification (Session 8)

Calibration flow simplified to match firmware's approach:
- Removed water drop icons from welcome screen
- Combined prompt + measuring into single screens for both steps
- Removed intermediate "measured" confirmation screens
- Auto-transition from empty → full and from full → complete

### BLE Protocol Summary

**Commands (iOS → Bottle):**
| Command | Code | Description |
|---------|------|-------------|
| `START_CALIBRATION` | `0x20` | Start calibration state machine |
| `CANCEL_CALIBRATION` | `0x21` | Cancel calibration, return to idle |

**Notification Characteristic (Bottle → iOS, 12 bytes):**
```cpp
struct BLE_CalibrationState {
    uint8_t  state;      // CalibrationState enum (0=idle, 2=started, 3=wait_empty, etc.)
    uint8_t  flags;      // Bit 0: error occurred
    int32_t  empty_adc;  // Captured empty ADC
    int32_t  full_adc;   // Captured full ADC
    uint16_t reserved;
};
```

**State Values:**
| State | Value | iOS Action |
|-------|-------|------------|
| `CAL_IDLE` | 0 | Show "Start Calibration" button |
| `CAL_STARTED` | 2 | Show "Calibration Starting..." |
| `CAL_WAIT_EMPTY` | 3 | Show empty bottle prompt |
| `CAL_MEASURE_EMPTY` | 4 | Show "Measuring..." progress |
| `CAL_WAIT_FULL` | 6 | Show full bottle prompt |
| `CAL_MEASURE_FULL` | 7 | Show "Measuring..." progress |
| `CAL_COMPLETE` | 9 | Show success screen |
| `CAL_ERROR` | 10 | Show error with retry option |

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md - iOS calibration flow complete, ready for merge (Plan 060)
```

**Status:** Implementation complete. Ready for PR merge after hardware testing.

---

## Recently Completed

- **LittleFS Drink Storage / NVS Fragmentation Fix (Issue #76)** - [Plan 059](Plans/059-littlefs-drink-storage.md)
- Drink Baseline Hysteresis Fix (Issue #76) - [Plan 057](Plans/057-drink-baseline-hysteresis.md)
- Unified Sessions View Fix (Issue #74) - [Plan 056](Plans/056-unified-sessions-view.md)
- Repeated Amber Notification Fix (Issue #72) - [Plan 055](Plans/055-repeated-amber-notification-fix.md)
- iOS Day Boundary Fix (Issue #70) - [Plan 054](Plans/054-ios-day-boundary-fix.md)
- Notification Threshold Adjustment (Issue #67) - [Plan 053](Plans/053-notification-threshold-adjustment.md)
- iOS Memory Exhaustion Fix (Issue #28) - [Plan 052](Plans/052-ios-memory-exhaustion-fix.md)
- Foreground Notification Fix (Issue #56) - [Plan 051](Plans/051-foreground-notification-fix.md)

---

## Branch Status

- `master` - Stable baseline
- `ios-calibration-flow` - **ACTIVE** - iOS calibration flow with bottle-driven approach

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
