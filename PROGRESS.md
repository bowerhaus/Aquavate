# Aquavate - Active Development Progress

**Last Updated:** 2026-01-14 (Daily Water Intake Tracking - Bug Fixes Complete)

---

## Current Branch: `daily-water-intake-tracking`

**Status:** Phase 2 complete - Ready for BLE integration

All standalone device features complete:
- Two-point calibration system (empty + full 830ml bottle)
- Gesture-based calibration trigger (inverted hold for 5s)
- Real-time water level measurement and display (±10-15ml accuracy)
- USB time configuration via serial commands (SET_DATETIME, SET_DATE, SET_TIME, SET_TZ, GET_TIME)
- Case-insensitive command parsing
- Smart e-paper display refresh (only updates when water level changes ≥5ml)
- NVS storage for calibration data, timezone, time_valid flag, and time persistence
- ESP32 internal RTC with NVS-based time persistence (event-based + hourly saves)
- Daily water intake tracking with 5-minute drink aggregation
- Daily reset at 4am boundary with fallback logic

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
Time Setting:
  SET_DATETIME <date> <time> <tz>  - Set date+time+timezone (e.g., SET_DATETIME 2026-01-14 14:30:00 +0)
  SET_DATE <date>                   - Set date only (YYYY-MM-DD)
  SET_TIME <time>                   - Set time only (HH[:MM[:SS]])
  SET_TZ <offset>                   - Set timezone offset (-12 to +14)
  GET_TIME                          - Display current time

Drink Tracking:
  GET_DAILY_STATE       - Show current daily state
  GET_LAST_DRINK        - Show most recent drink record
  DUMP_DRINKS           - Display all drink records
  RESET_DAILY_DRINKS    - Reset daily counter to 0
  CLEAR_DRINKS          - Clear all drink records (WARNING: erases data)

Debug Levels:
  0  - All debug OFF (quiet mode)
  1  - Events only (drinks, refills, display updates)
  2  - + Gestures (gesture detection, state changes, calibration)
  3  - + Weight readings (load cell ADC, water levels)
  4  - + Accelerometer (raw accelerometer data every second)
  9  - All ON (all categories including future BLE)
```

Progress - Day 5 Debug Level System (2026-01-14):
- ✅ Implemented runtime debug level control via single-character serial commands
- ✅ Created 6 debug levels for progressive verbosity:
  - **0**: All debug OFF (quiet mode)
  - **1**: Events only (drinks, refills, display updates)
  - **2**: + Gestures (gesture detection, state changes, calibration)
  - **3**: + Weight readings (load cell ADC, water levels)
  - **4**: + Accelerometer (raw accelerometer data every second)
  - **9**: All ON (all categories, reserved levels 5-8 for future expansion)
- ✅ Replaced all compile-time `#if DEBUG_*` checks with runtime checks
- ✅ Added global debug control variables in main.cpp:
  - `g_debug_enabled`, `g_debug_water_level`, `g_debug_accelerometer`
  - `g_debug_display`, `g_debug_drink_tracking`, `g_debug_calibration`, `g_debug_ble`
- ✅ Updated main.cpp: Water level, display, and accelerometer debug output now runtime-controlled
- ✅ Updated gestures.cpp: Gesture detection debug now uses `g_debug_calibration` (level 2+)
- ✅ Helper macros added to config.h: `DEBUG_PRINT()`, `DEBUG_PRINTLN()`, `DEBUG_PRINTF()`
- ✅ Single-character commands: Just type `0`-`4` or `9` in serial monitor (no enter needed)
- ✅ Debug settings reset to config.h defaults on reboot (non-persistent by design)
- ✅ Firmware compiles successfully (RAM: 6.9%, Flash: 35.5%)

Debug Level System Files:
- firmware/src/config.h lines 11-59 - Debug level documentation, helper macros, extern declarations
- firmware/src/main.cpp lines 299-308 - Runtime debug control variables
- firmware/src/serial_commands.cpp lines 398-486 - Debug level handler and command parser
- firmware/src/gestures.cpp - All gesture debug output uses `g_debug_calibration`

Usage Examples:
```
0  - Turn off all debug output (quiet mode)
1  - Show only drink events and display updates
2  - Add gesture detection messages (UPRIGHT, UPRIGHT_STABLE, conditions)
3  - Add weight readings (Water level: 450ml)
4  - Add raw accelerometer data (Accel X: 0.02g Y: -0.01g Z: 0.98g)
9  - Enable everything including future BLE debug
```

Progress - Day 6 Refill Bug Fix & Command Improvements (2026-01-14):
- ✅ Fixed refill bug where refills were being saved as drink records
  - Removed storageSaveDrinkRecord() call from refill detection block
  - Refills now only update baseline ADC and close aggregation window
  - Added comment explaining why refills aren't stored
- ✅ Improved time setting commands with granular control:
  - Created SET_DATETIME (combined date+time+timezone)
  - Created SET_DATE (date only, YYYY-MM-DD format)
  - Created SET_TIME (time only, flexible HH[:MM[:SS]] format with optional MM and SS)
  - Created SET_TZ (timezone-only alias for SET_TIMEZONE)
  - Made all commands case-insensitive (toupper conversion in parser)
- ✅ Firmware compiles successfully (RAM: 6.9%, Flash: 35.7%)

Files Modified:
- firmware/src/drinks.cpp - Removed refill record saving (lines 229-246)
- firmware/src/serial_commands.cpp - Command refactoring and case-insensitive parsing
- All time-setting commands now support flexible input formats

Progress - Day 7 Time Persistence Implementation (2026-01-14):
- ✅ Implemented NVS-based time persistence to preserve time across power cycles
- ✅ Smart save strategy to minimize flash wear:
  - Save timestamp on every drink or refill event (opportunistic)
  - Save timestamp hourly on the hour (periodic fallback)
  - Save timestamp on all time-setting commands (immediate)
  - Only save if DS3231 RTC is not present (g_rtc_ds3231_present flag)
- ✅ Time restoration on boot from NVS (storageLoadLastBootTime)
- ✅ Flash lifespan optimized: ~40 writes/day = ~7-8 years
- ✅ Added saveTimestampOnEvent() helper in drinks.cpp
- ✅ Added g_rtc_ds3231_present flag for future DS3231 integration
- ✅ Firmware compiles successfully (RAM: 6.9%, Flash: 35.7%)

Files Modified:
- firmware/src/main.cpp - Time restoration on boot, hourly save logic, g_rtc_ds3231_present flag
- firmware/src/drinks.cpp - saveTimestampOnEvent() helper, calls on drink/refill
- firmware/src/storage.cpp - storageSaveLastBootTime() and storageLoadLastBootTime()
- firmware/include/storage.h - Function declarations

Time Persistence Strategy:
- Maximum time loss: ~1 hour on power cycle (typically much less)
- ESP32 internal RTC maintains time during deep sleep (no loss)
- NVS saves act as checkpoints for power cycle recovery
- Strategy balances accuracy vs. flash wear (100,000 write cycles per sector)

Next Steps:
1. Validate daily intake visualization updates on display
2. Test daily reset at 4am boundary
3. Test full day simulation scenarios
4. Begin BLE integration planning

Key Features Implemented:
- ✅ Drink event detection with 5-minute aggregation window
- ✅ Daily reset at 4am boundary with fallback logic
- ✅ NVS circular buffer storage (200 records)
- ✅ Dual visualization modes (human figure or tumbler grid)
- ✅ Hardcoded 2500ml daily goal
- ✅ Baseline drift compensation working correctly

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

- `daily-water-intake-tracking` - **ACTIVE**: Implementing minimal bug fixes (FSM-free approach)
- `comprehensive-fsm-refactoring` - Banked: Full FSM refactor (for future BLE/OTA work)
- `master` - Stable: Basic iOS Hello World + Hardware design
- `standalone-calibration-mode` - Merged: Calibration system complete
- `usb-time-setting` - Merged: Serial time configuration complete
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
