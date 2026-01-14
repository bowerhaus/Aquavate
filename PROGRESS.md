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
- Status: Day 3 - Drink detection algorithm complete
- Branch: `daily-water-intake-tracking` (current branch)

Progress - Day 1 Display Visualization (2026-01-13):
- ✅ Human figure bitmap created (50×83px, outline + filled versions)
- ✅ Two-bitmap progressive fill rendering implemented (fills bottom-to-top)
- ✅ Tumbler grid display created (5 rows × 2 cols = 10 tumblers)
- ✅ Tumbler rendering with tapered shape (wide rim, narrow base)
- ✅ Fractional fill support (e.g., 55% = 5.5 glasses filled)
- ✅ Both visualization options working and tested in code
- ✅ Test display code added to main screen (55% fill at 1375ml/2500ml)
- ✅ Configuration option added (DAILY_INTAKE_DISPLAY_MODE in config.h)
- ✅ Mode switching implemented with preprocessor directives
- ✅ Firmware compiles successfully (RAM: 6.9%, Flash: 34.7%)
- ⏳ Awaiting hardware upload and validation

Visualization Options:
- **Human Figure Mode:** Silhouette fills bottom-to-top (continuous fill)
- **Tumbler Grid Mode:** 10 glasses fill progressively with fractional support

Implementation Details (for context reset):
- Both functions in firmware/src/main.cpp:
  - `drawHumanFigure()` at lines 502-536 (position: 185,20)
  - `drawGlassGrid()` at lines 544-606 (position: 195,20)
- Test code at lines 701-715 (mode switching with 55% fill test)
- Config constant at firmware/src/config.h line 98 (DAILY_INTAKE_DISPLAY_MODE)
- Mode switching uses #if DAILY_INTAKE_DISPLAY_MODE == 0 preprocessor directive

Progress - Day 2 Data Structures & Storage (2026-01-13):
- ✅ Created drinks.h with DrinkRecord and DailyState structures
- ✅ Created storage_drinks.h with NVS storage API declarations
- ✅ Added 7 drink detection constants to config.h (lines 100-107)
- ✅ Implemented storage_drinks.cpp with circular buffer logic:
  - storageSaveDrinkRecord() - circular buffer write with wrap-around
  - storageLoadLastDrinkRecord() - retrieve most recent drink
  - storageUpdateLastDrinkRecord() - aggregate drinks in 5-min window
  - storageLoadDailyState() / storageSaveDailyState() - persistent daily state
  - storageLoadBufferMetadata() / storageSaveBufferMetadata() - buffer management
- ✅ Firmware compiles successfully (RAM: 6.9%, Flash: 34.7%)

Files Created:
- firmware/include/drinks.h - DrinkRecord (9 bytes), DailyState (26 bytes)
- firmware/include/storage_drinks.h - CircularBufferMetadata + API
- firmware/src/storage_drinks.cpp - NVS storage implementation (~200 lines)

Configuration Added (config.h lines 100-107):
- DRINK_MIN_THRESHOLD_ML = 30ml
- DRINK_REFILL_THRESHOLD_ML = 100ml
- DRINK_AGGREGATION_WINDOW_SEC = 300s (5 minutes)
- DRINK_DAILY_RESET_HOUR = 4am
- DRINK_DISPLAY_UPDATE_THRESHOLD_ML = 50ml
- DRINK_MAX_RECORDS = 200 (circular buffer)
- DRINK_DAILY_GOAL_ML = 2500ml

Progress - Day 3 Drink Detection (2026-01-13):
- ✅ Implemented drinks.cpp with complete detection algorithm (~250 lines)
- ✅ Core functions implemented:
  - drinksInit() - Load or initialize daily state from NVS
  - drinksUpdate() - Main detection loop (drink/refill detection, aggregation)
  - shouldResetDailyCounter() - 4am boundary check with fallback logic
  - getCurrentUnixTime() - RTC + timezone helper
- ✅ Drink detection logic working:
  - ≥30ml decrease triggers drink event
  - ≥100ml increase triggers refill event
  - 5-minute aggregation window for multiple sips
  - Daily reset on first drink after 4am
  - Display update threshold (≥50ml change)
- ✅ Added 5 debug serial commands to serial_commands.cpp:
  - GET_DAILY_STATE - Display current DailyState (totals, counters, window status)
  - GET_LAST_DRINK - Show most recent DrinkRecord with formatted timestamp
  - DUMP_DRINKS - Display all drink records in chronological order
  - RESET_DAILY_DRINKS - Reset daily counter (keeps records)
  - CLEAR_DRINKS - Clear all drink records (WARNING command)
- ✅ Firmware compiles successfully (RAM: 6.9%, Flash: 35.1%)

Files Modified:
- firmware/src/drinks.cpp - Core detection algorithm (NEW, ~250 lines)
- firmware/include/drinks.h - Added debug function declarations
- firmware/src/serial_commands.cpp - Added 5 debug commands (~120 lines)

Progress - Day 4 Main Loop Integration (2026-01-13 - 2026-01-14):
- ✅ Integrated drinksInit() in setup() (only if time is valid)
- ✅ Integrated drinksUpdate() in main loop (calls when UPRIGHT_STABLE achieved)
- ✅ Updated drawMainScreen() to display real daily intake data:
  - Replaces hardcoded 55% test value with actual DailyState
  - Calculates fill percentage: daily_total_ml / DRINK_DAILY_GOAL_ML (2500ml)
  - Shows 0% fill if time not set (graceful degradation)
- ✅ Changed main screen text from bottle capacity to daily intake:
  - Large text now shows daily total (e.g., "1350ml") instead of bottle level
  - "today" label below in size 2 text
  - Text dynamically centered between bottle graphic and visualization
  - Bottle graphic remains (shows current fill level 0-830ml)
- ✅ Implemented two-gesture system for better stability:
  - GESTURE_UPRIGHT: Bottle on table within 12° of vertical (for display updates)
  - GESTURE_UPRIGHT_STABLE: Upright + weight stable for 2s (for drink tracking)
  - Weight stability tracking moved inside gestures.cpp for cleaner encapsulation
  - Baseline weight updates continuously when stable (tracks gradual changes)
  - Timer resets when weight changes >6ml (prevents false positives during tilting)
- ✅ Relaxed accelerometer thresholds for real-world stability:
  - Z-axis threshold: 0.97g (was 0.985g) - tolerates sensor noise
  - Variance threshold: 0.02 g² (was 0.01 g²) - tolerates surface vibrations
- ✅ Fixed drink detection baseline initialization:
  - last_recorded_adc now initialized on first UPRIGHT_STABLE
  - Prevents false detections from ADC=0 baseline
- ✅ Enabled debug output for testing:
  - DEBUG_ENABLED = 1 (verbose debug output)
  - DEBUG_WATER_LEVEL = 1 (water level messages)
  - DEBUG_ACCELEROMETER = 1 (accelerometer debug)
  - DEBUG_DISPLAY_UPDATES = 1 (display update messages)
- ✅ Firmware compiles successfully (RAM: 6.9%, Flash: 35.4%)
- 🔧 **CURRENTLY DEBUGGING**: Drink detection working but only registering every other drink
  - Need to investigate baseline update logic after drink events
  - Issue: After drink detected, baseline may not be updating correctly for next drink

Main Screen Layout (2.13" landscape):
- Top center: Time display (e.g., "Wed 2pm")
- Top right: Battery icon with percentage
- Left side: Bottle graphic showing current fill level (0-830ml)
- Center: Daily intake total in large text ("1350ml")
- Right side: Human figure or tumbler grid visualization (fills progressively)

Integration Details:
- drinksUpdate() called at line 1044 (when UPRIGHT_STABLE & calibrated)
- Display updates at lines 1020-1062 (when UPRIGHT or UPRIGHT_STABLE)
- Daily intake visualization in lines 701-729 (real-time data from DailyState)
- Daily intake text display in lines 675-704 (fetches current total)
- Gesture weight stability tracking in gestures.cpp lines 218-230
- Time validity checks ensure safe operation without RTC

Gesture Architecture:
- gestures.h: GESTURE_UPRIGHT and GESTURE_UPRIGHT_STABLE enum values
- gestures.cpp: Weight tracking with 6ml delta threshold and 2s duration
- main.cpp: Display updates on UPRIGHT, drink tracking on UPRIGHT_STABLE

Serial Commands Available:
```
GET_DAILY_STATE       - Show current daily state
GET_LAST_DRINK        - Show most recent drink record
DUMP_DRINKS           - Display all drink records
RESET_DAILY_DRINKS    - Reset daily counter to 0
CLEAR_DRINKS          - Clear all drink records (WARNING: erases data)
```

Active Debugging (2026-01-14):

**Issue #1: Every-Other-Drink Detection**
**Symptoms:**
- First drink: Detected correctly, baseline updated
- Second drink: Not detected (delta_ml calculation issue?)
- Third drink: Detected correctly
- Pattern repeats

**Suspected Root Cause:**
After a drink is detected, `last_recorded_adc` is updated (line 172 in drinks.cpp).
On the next UPRIGHT_STABLE, the baseline might already reflect the new weight,
resulting in delta_ml ≈ 0, so the next drink isn't detected until baseline drifts back.

**Investigation Needed:**
- Check if baseline update (line 213: drift compensation) is running too frequently
- Verify baseline only updates when NOT in aggregation window AND no drink detected
- Consider adding cooldown period after drink detection before baseline updates

---

**Issue #2: Time/Date Display Incorrect**
**Symptoms:**
- Display showing: "Thursday 1pm"
- Actual time: Wednesday 5am (2026-01-15 05:00)
- Off by ~20 hours and wrong day of week

**Context:**
- Time was previously set via SET_TIME command
- User about to reset time again using SET_TIME command
- May be timezone offset issue or RTC drift
- Need to verify formatTimeForDisplay() logic and timezone handling

**Investigation Needed:**
- Check if timezone offset is being applied correctly
- Verify ESP32 internal RTC is maintaining accurate time
- Review formatTimeForDisplay() implementation
- Confirm SET_TIME command sets both UTC and timezone offset correctly

Next Steps:
1. Fix every-other-drink detection issue
2. Test full drink/refill cycle with serial commands
3. Validate daily intake visualization updates on display
4. Test daily reset at 4am boundary
5. Test full day simulation scenarios

Key Features Implemented:
- ✅ Drink event detection with 5-minute aggregation window
- ✅ Daily reset at 4am boundary with fallback logic
- ✅ NVS circular buffer storage (200 records)
- ✅ Dual visualization modes (human figure or tumbler grid)
- ✅ Hardcoded 2500ml daily goal
- 🔧 Baseline drift compensation (currently causing false negatives)

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
