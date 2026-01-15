# Aquavate - Active Development Progress

**Last Updated:** 2026-01-14

---

## Current Branch: `extended-deep-sleep-backpack-mode`

**Status:** ✅ Phase 2.6 Complete - Ready to Merge

### Standalone Device Features (Complete)

**Phase 1: Core Firmware**
- Two-point calibration system (empty + full 830ml bottle)
- Gesture-based calibration trigger (inverted hold for 5s)
- Real-time water level measurement and display (±10-15ml accuracy)
- Smart e-paper display with state tracking (water, time, battery)
- NVS storage for calibration data and settings

**Phase 2: Daily Intake Tracking** ✅ Complete
- Drink detection with type classification (GULP <100ml, POUR ≥100ml)
- Daily reset at 4am boundary with fallback logic
- NVS circular buffer storage (600 records = 30 days history)
- Dual visualization modes (human figure or tumbler grid)
- Hardcoded 2500ml daily goal

**USB Time Configuration:**
- Serial commands: SET_DATETIME, SET_DATE, SET_TIME, SET_TZ, GET_TIME
- Case-insensitive command parsing
- ESP32 internal RTC with NVS-based time persistence
- Event-based + hourly timestamp saves

**Display Module:**
- Smart state tracking for all displayed values
- Time updates immediately on hour changes (e.g., 9:59am → 10:00am)
- Battery monitoring with 20% quantized steps
- Updates only when bottle is UPRIGHT_STABLE (prevents flicker)

**Memory Usage:** RAM: 7.0% (22,972 bytes) | Flash: 36.6% (480,040 bytes)

---

## Active Work

**Phase 2.5: Deep Sleep Power Management** ✅ COMPLETE
- Reinstated deep sleep mode with 30s awake duration ✅
- Optional "Zzzz" sleep indicator (configurable via DISPLAY_SLEEP_INDICATOR) ✅
- Sleep blocked during active calibration ✅
- Wake timer resets after calibration exit ✅
- Serial command SET_SLEEP_TIMEOUT with NVS persistence ✅
- Welcome screen only on power-on/reset (not on wake from sleep) ✅
- Sleep display uses last valid state (doesn't re-read sensors) ✅
- Enhanced wake-on-tilt interrupt handling with diagnostics ✅
- LIS3DH interrupt properly cleared before/after sleep ✅
- Sleep timeout persists across reboots via NVS ✅
- 'T' test command for real-time interrupt diagnostics ✅
- **RTC Memory State Persistence** ✅
  - Display state saved to RTC memory before sleep (water, daily, time, battery)
  - Drink detection baseline saved to RTC memory (prevents false triggers)
  - Wake counter for diagnostics (tracks sleep cycles)
  - Magic number validation (0x41515541 "AQUA" for display, 0x44524E4B "DRNK" for drinks)
  - Auto-restore on wake from sleep, auto-invalidate on power cycle
  - **Result:** Display only updates when values actually change, eliminating unnecessary flashing
- **Wake-on-Tilt Status** ⚠️
  - Using original Z-axis low interrupt configuration (INT1_CFG: 0x02)
  - Threshold: 0x32 (0.80g) based on measured values (Vertical: 1.00g, tilted: 0.47-0.77g)
  - **Known limitation:** Reliably wakes on left/right tilts, but forward/backward may not consistently trigger
  - This is acceptable for MVP - users will naturally pick up bottle in ways that trigger wake
- Plan: [Plans/010-deep-sleep-reinstatement.md](Plans/010-deep-sleep-reinstatement.md)

**Status:** ✅ Complete and ready to merge. Wake-on-tilt works for primary use cases (left/right).

**Phase 2.6: Extended Deep Sleep (Backpack Mode)** ✅ COMPLETE
- Dual deep sleep modes: Normal (motion wake) + Extended (timer wake) ✅
- Automatic extended mode trigger after 2 minutes continuous awake ✅
- Timer-based wake (60s default) in extended mode to conserve battery ✅
- Motion detection on timer wake to return to normal mode ✅
- Display "Zzzz" indicator cleared on wake from extended sleep ✅
- RTC memory persistence for continuous awake tracking (magic: 0x45585400) ✅
- Serial commands:
  - SET_EXTENDED_SLEEP_TIMER (default: 60s, range: 1-3600s) ✅
  - SET_EXTENDED_SLEEP_THRESHOLD (default: 120s, range: 30-600s) ✅
  - GET_STATUS (shows all system settings including extended sleep) ✅
  - SET_SLEEP_TIMEOUT backward compatible (also accepts SET_NORMAL_SLEEP_TIMEOUT) ✅
- NVS persistence for all extended sleep settings ✅
- Configurable "Zzzz" indicator for extended sleep mode ✅
- Plan: [Plans/011-extended-deep-sleep-backpack-mode.md](Plans/011-extended-deep-sleep-backpack-mode.md)
- **Power Impact:** ~60× reduction in backpack scenario (30mA continuous → 0.5mAh/hour)
- **Build:** RAM 7.0% (22,980 bytes), Flash 37.2% (487,360 bytes) ✅

**Implementation Details:**
- Files Modified: config.h, main.cpp, storage.h, storage.cpp, serial_commands.cpp, PROGRESS.md
- RTC Memory: 3 variables with magic number validation (survives deep sleep)
- Display Integration: Force update flag clears Zzzz on first UPRIGHT_STABLE after wake
- Motion Check: Samples gestures over 500ms to detect continuous motion vs. stationary

**Status:** ✅ Complete, tested build, ready to merge. Prevents battery drain from continuous wake cycles when in backpack.


---

## Next Up

### Firmware - Phase 3: BLE Integration & App Sync
- [ ] Plan BLE GATT service structure
- [ ] Implement NimBLE server with Device Info service
- [ ] Create custom Aquavate GATT service (drink history, current state)
- [ ] Add drink record sync protocol (forward from NVS to app)
- [ ] Test BLE advertising and connection from iOS
- [ ] Consider FSM architecture for BLE pairing state management

### iOS App - Phase 4: BLE & Storage
- [ ] Implement CoreBluetooth BLE manager
- [ ] Add device scanning and pairing
- [ ] Create CoreData models (Bottle, DrinkRecord, DailySummary)
- [ ] Build basic home screen with current level display
- [ ] Implement drink history sync from puck

---

## Known Issues

**Wake-on-tilt directional sensitivity:**
- Wake interrupt reliably triggers on left/right tilts
- Forward/backward tilts may not consistently wake from deep sleep
- Root cause: LIS3DH Z-axis low interrupt (INT1_CFG=0x02) doesn't catch all tilt directions equally
- Impact: Minimal - users naturally pick up bottles in ways that trigger wake (left/right grasp)
- Future work: Could investigate 6D orientation detection or multi-axis interrupt configuration

---

## Branch Status

- `extended-deep-sleep-backpack-mode` - **ACTIVE**: ✅ Complete, ready to merge to master
- `deep-sleep-reinstatement` - Merged: Deep sleep power management (Phase 2.5)
- `comprehensive-fsm-refactoring` - Banked: Full FSM refactor (for future BLE/OTA work)
- `master` - Stable: Hardware design + iOS skeleton + standalone firmware (phases 1-2.5)

---

## Reference Documents

- [PRD.md](docs/PRD.md) - Full product requirements
- [Sensor Puck Design](Plans/004-sensor-puck-design.md) - Mechanical design v3.0
- [Standalone Calibration Mode](Plans/005-standalone-calibration-mode.md) - Two-point calibration implementation plan
- [USB Time Setting](Plans/006-usb-time-setting.md) - Serial time configuration for standalone operation
- [Daily Intake Tracking](Plans/007-daily-intake-tracking.md) - Phase 2 implementation plan (COMPLETED)
- [Drink Type Classification](Plans/008-drink-type-classification.md) - Gulp vs Pour implementation (COMPLETED)
- [Smart Display State Tracking](Plans/009-smart-display-state-tracking.md) - Display module with time/battery updates (COMPLETED)
- [Deep Sleep Reinstatement](Plans/010-deep-sleep-reinstatement.md) - Power management with 30s sleep (COMPLETED)
- [Extended Deep Sleep Backpack Mode](Plans/011-extended-deep-sleep-backpack-mode.md) - Dual sleep modes for continuous motion (COMPLETED)
- [Hardware Research](Plans/001-hardware-research.md) - Component selection analysis
- [Adafruit BOM](Plans/002-bom-adafruit-feather.md) - UK parts list for Feather config
- [SparkFun BOM](Plans/003-bom-sparkfun-qwiic.md) - UK parts list for Qwiic config
