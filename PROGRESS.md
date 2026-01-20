# Aquavate - Active Development Progress

**Last Updated:** 2026-01-20
**Current Branch:** `improved-calibration-ui`

---

## Current Status

**In Progress:** Shake-to-Empty Gesture - [Plan 025](Plans/025-shake-to-empty-gesture.md)

### Implementation Complete - Ready for Hardware Testing
- [x] Configuration constants added to config.h
- [x] GESTURE_SHAKE_WHILE_INVERTED enum added
- [x] Shake detection logic in gestures.cpp
- [x] drinksCancelLast() function in drinks.cpp
- [x] uiShowBottleEmptied() confirmation screen
- [x] Main loop integration with weight verification
- [x] Firmware builds successfully

### Hardware Testing Required
- [ ] Test shake gesture detection (shake vs hold still)
- [ ] Test cancellation with empty bottle
- [ ] Test normal drink recording when bottle not empty

---

## Recently Completed

- ✅ Improved Calibration UI - [Plan 024](Plans/024-improved-calibration-ui.md)
- ✅ ADXL343 Accelerometer Migration - [Plan 023](Plans/023-adxl343-accelerometer-migration.md)
- ✅ DS3231 RTC Integration - [Plan 022](Plans/022-ds3231-rtc-integration.md)

---

## Next Up

1. **Phase 3E: Firmware Power Optimization** - [Plan 013](Plans/013-ble-integration.md)
   - Connection parameter tuning
   - Power measurement validation

2. **Phase 4.7: iOS Calibration Wizard** (Optional) - [Plan 014](Plans/014-ios-ble-coredata-integration.md)

3. **Merge `new-hardware` branch to master**

---

## Known Issues

**Wake-on-tilt sensitivity (ADXL343):**
- Requires deliberate shake (~2 seconds) due to aggressive filtering
- Trade-off accepted for battery life benefits
- See [Plan 023](Plans/023-adxl343-accelerometer-migration.md) for details

---

## Branch Status

- `new-hardware` - **ACTIVE**: Hardware upgrades (ADXL343, DS3231, calibration UI)
- `master` - Stable baseline

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
