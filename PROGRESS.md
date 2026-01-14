# Aquavate - Active Development Progress

**Last Updated:** 2026-01-14

---

## Current Branch: `smart-display-state-tracking`

**Status:** Phase 2 complete - Ready for BLE integration

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

**Memory Usage:** RAM: 7.0% (22,804 bytes) | Flash: 35.7% (468,132 bytes)

---

## Active Work

**Preparing for Phase 3: BLE Integration**
- All standalone features complete and tested
- Documentation up to date
- Current branch ready to merge to master


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

None currently.

---

## Branch Status

- `smart-display-state-tracking` - **ACTIVE**: Ready to merge
- `comprehensive-fsm-refactoring` - Banked: Full FSM refactor (for future BLE/OTA work)
- `master` - Stable: Hardware design + iOS skeleton

---

## Reference Documents

- [PRD.md](docs/PRD.md) - Full product requirements
- [Sensor Puck Design](Plans/004-sensor-puck-design.md) - Mechanical design v3.0
- [Standalone Calibration Mode](Plans/005-standalone-calibration-mode.md) - Two-point calibration implementation plan
- [USB Time Setting](Plans/006-usb-time-setting.md) - Serial time configuration for standalone operation
- [Daily Intake Tracking](Plans/007-daily-intake-tracking.md) - Phase 2 implementation plan (COMPLETED)
- [Drink Type Classification](Plans/008-drink-type-classification.md) - Gulp vs Pour implementation (COMPLETED)
- [Smart Display State Tracking](Plans/009-smart-display-state-tracking.md) - Display module with time/battery updates (COMPLETED)
- [Hardware Research](Plans/001-hardware-research.md) - Component selection analysis
- [Adafruit BOM](Plans/002-bom-adafruit-feather.md) - UK parts list for Feather config
- [SparkFun BOM](Plans/003-bom-sparkfun-qwiic.md) - UK parts list for Qwiic config
