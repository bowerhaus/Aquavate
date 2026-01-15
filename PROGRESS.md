# Aquavate - Active Development Progress

**Last Updated:** 2026-01-15

---

## Current Branch: `extended-deep-sleep-backpack-mode`

**Status:** ✅ Complete - Ready to Merge

This branch implements dual deep sleep modes (normal motion-wake + extended timer-wake) to prevent battery drain during continuous motion scenarios like backpacks. See [Plans/011-extended-deep-sleep-backpack-mode.md](Plans/011-extended-deep-sleep-backpack-mode.md) for details.

**Power Impact:** ~60× reduction in backpack scenario (30mA continuous → 0.5mAh/hour)

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
- `master` - Stable: Hardware design + iOS skeleton + standalone firmware (phases 1-2.6)

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
