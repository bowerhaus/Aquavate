# Aquavate - Active Development Progress

**Last Updated:** 2026-01-13 (USB Time Setting Complete & Tested)

---

## Current Branch: `usb-time-setting`

**Status:** Ready to merge to master

All standalone device features complete:
- Two-point calibration system (empty + full 830ml bottle)
- Gesture-based calibration trigger (inverted hold for 5s)
- Real-time water level measurement and display (±10-15ml accuracy)
- USB time configuration via serial commands (SET_TIME, GET_TIME, SET_TIMEZONE)
- Smart e-paper display refresh (only updates when water level changes ≥5ml)
- NVS storage for calibration data, timezone, and time_valid flag
- ESP32 internal RTC (no external DS3231 needed)

Hardware testing complete. Documentation updated. Code refactored.

---

## Active Work

**Phase 2: Daily Water Intake Tracking** (Started 2026-01-13)
- Plan: [Plans/007-daily-intake-tracking.md](Plans/007-daily-intake-tracking.md)
- Status: Day 1 - Display visualization prototyping complete
- Branch: `daily-water-intake-tracking` (current branch)

Progress - Day 1 Display Visualization (2026-01-13):
- ✅ Human figure bitmap created (50×83px, outline + filled versions)
- ✅ Two-bitmap progressive fill rendering implemented (fills bottom-to-top)
- ✅ Tumbler grid display created (5 rows × 2 cols = 10 tumblers)
- ✅ Tumbler rendering with tapered shape (wide rim, narrow base)
- ✅ Fractional fill support (e.g., 55% = 5.5 glasses filled)
- ✅ Both visualization options working and tested in code
- ✅ Test display code added to main screen (55% fill at 1375ml/2500ml)
- ✅ Configuration option planned (DAILY_INTAKE_DISPLAY_MODE in config.h)
- ⏳ Ready to add config.h option and implement mode switching
- ⏳ Awaiting hardware upload and validation

Visualization Options:
- **Human Figure Mode:** Silhouette fills bottom-to-top (continuous fill)
- **Tumbler Grid Mode:** 10 glasses fill progressively with fractional support

Implementation Details (for context reset):
- Both functions in firmware/src/main.cpp:
  - `drawHumanFigure()` at lines 502-536 (position: 185,20)
  - `drawGlassGrid()` at lines 544-606 (position: 195,20)
- Test code at lines 676-684 (currently shows tumbler grid at 55%)
- Config constant needed in firmware/src/config.h after line 95
- Mode switching: Replace test code with #if DAILY_INTAKE_DISPLAY_MODE preprocessor directive

Next Steps:
1. Add to firmware/src/config.h (after line 95):
   ```
   // Display mode: 0=human figure, 1=tumbler grid
   #define DAILY_INTAKE_DISPLAY_MODE 1
   ```
2. Update main.cpp test code (lines 676-684) to use mode switching
3. Upload to hardware for visual validation (both modes)
4. Proceed to drink detection implementation (Day 2)

Key features (planned):
- Drink event detection with 5-minute aggregation
- Daily reset at 4am boundary
- NVS circular buffer storage (200 records)
- Dual visualization modes (human figure or tumbler grid)
- Hardcoded 2500ml daily goal

---

## Next Up

### Firmware - Phase 2: Storage & Display (After Time Setting)
- [ ] Implement drink record storage in NVS (7-day circular buffer)
- [ ] Create DrinkRecord structure with timestamp, amount, level
- [ ] Build e-paper UI abstraction layer
- [ ] Design and render status display (daily total, battery, BLE status)
- [ ] Implement empty gesture detection (invert + shake)

### Firmware - Phase 3: BLE Communication
- [ ] Set up NimBLE server with Device Info service
- [ ] Implement custom Aquavate GATT service
- [ ] Create drink history sync protocol
- [ ] Add bottle configuration characteristics
- [ ] Test BLE advertising and connection

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

- `master` - Stable: Basic iOS Hello World + Hardware design
- `standalone-calibration-mode` - Active: Calibration system implemented, hardware testing in progress
- `Calibration` - Archived: Superseded by standalone-calibration-mode
- `Hello-IOS` - Merged: Initial iOS project
- `Hardware-design` - Merged: Sensor puck v3.0 design
- `Initial-PRD` - Merged: Product requirements document

---

## Reference Documents

- [PRD.md](docs/PRD.md) - Full product requirements
- [Sensor Puck Design](Plans/004-sensor-puck-design.md) - Mechanical design v3.0
- [Standalone Calibration Mode](Plans/005-standalone-calibration-mode.md) - Two-point calibration implementation plan
- [USB Time Setting](Plans/006-usb-time-setting.md) - Serial time configuration for standalone operation
- [Daily Intake Tracking](Plans/007-daily-intake-tracking.md) - Phase 2 implementation plan (IN PROGRESS)
- [Hardware Research](Plans/001-hardware-research.md) - Component selection analysis
- [Adafruit BOM](Plans/002-bom-adafruit-feather.md) - UK parts list for Feather config
- [SparkFun BOM](Plans/003-bom-sparkfun-qwiic.md) - UK parts list for Qwiic config
