# Aquavate - Active Development Progress

**Last Updated:** 2026-01-12 (23:45)

---

## Current Branch: `standalone-calibration-mode`

Implementing standalone calibration mode with gesture-based two-point calibration.

---

## Recently Completed

### Hardware Design
- ✅ Hardware component research and BOM creation (Adafruit & SparkFun options)
- ✅ Sensor puck mechanical design (v3.0 - 85mm diameter, landscape display in side bump)
- ✅ Component dimension documentation

### Firmware - Phase 1 Foundation
- ✅ PlatformIO project setup with dual hardware configs (Adafruit Feather / SparkFun Qwiic)
- ✅ E-paper display initialization (Adafruit 2.13" FeatherWing)
- ✅ NAU7802 load cell ADC initialization
- ✅ LIS3DH accelerometer initialization with I2C communication
- ✅ Wake-on-tilt interrupt configuration (80° tilt threshold)
- ✅ Deep sleep with EXT0 wake-up from LIS3DH INT1 pin
- ✅ Battery voltage monitoring and percentage calculation
- ✅ Basic display rendering (Hello message, battery icon, bottle graphic)
- ✅ Serial debug output for sensor readings

### iOS App - Foundation
- ✅ Xcode project created with SwiftUI
- ✅ Basic "Hello World" UI with app branding

---

## Active Work

### Firmware - Standalone Calibration Mode (Plan: 005-standalone-calibration-mode.md)
**Current Sprint:** Implementing two-point calibration system for standalone operation

**Phase 1 - Core Calibration (MVP):**
- ✅ Create header files with data structures and function declarations
  - ✅ gestures.h - Gesture detection types and functions
  - ✅ weight.h - Weight measurement utilities
  - ✅ storage.h - NVS calibration storage
  - ✅ calibration.h - State machine and calculations
  - ✅ ui_calibration.h - E-paper display screens
- ✅ Implement gesture detection module (gestures.cpp)
  - ✅ Inverted hold detection (Z < -0.8g for 5s)
  - ✅ Upright stable detection (Z > 0.9g, low variance)
  - ✅ Sideways tilt confirmation (|X| or |Y| > 0.5g)
- ✅ Implement weight measurement utilities (weight.cpp)
  - ✅ Stable weight reading with variance checking
  - ✅ Outlier removal (±2 standard deviations)
  - ✅ Mean and variance calculation
- ✅ Implement NVS storage layer (storage.cpp)
  - ✅ CalibrationData structure save/load
  - ✅ Preferences library integration
- ✅ Implement calibration state machine (calibration.cpp)
  - ✅ Two-point calibration flow (empty + full 830ml)
  - ✅ Scale factor calculation
  - ✅ Water weight measurement conversion
- ✅ Implement calibration UI screens (ui_calibration.cpp)
  - ✅ 7 static screens for calibration wizard
  - ✅ E-paper display integration (SSD1680 - full refresh only)
- ✅ Update config.h with calibration constants
  - ✅ Gesture thresholds
  - ✅ Variance limits
  - ✅ Timing constants
- ✅ Integrate calibration into main.cpp
  - ✅ Calibration mode vs tracking mode switching
  - ✅ Setup: load calibration from NVS
  - ✅ Loop: monitor for calibration trigger gesture


**Fixes Applied (2026-01-12 23:45):**
- ✅ Increased awake duration from 15s to 30s for better calibration UX
- ✅ Fixed inverted gesture detection with latch mechanism
- ✅ Added debug output for inverted hold progress
- ✅ Reduced main loop delay from 500ms to 200ms for more responsive gesture detection
- ✅ Relaxed gesture thresholds (inverted: -0.7g, upright: 0.8g) for easier detection
- ✅ Fixed weight variance threshold (100 → 5000 → 6000 ADC² units) for realistic stability detection
- ✅ Deep sleep disabled for calibration testing
- ✅ Attempted double tap gesture detection (hardware + software) - removed due to unreliability
- ✅ Firmware builds successfully (RAM: 6.8%, Flash: 31.7%)

**Calibration Flow Improvements (2026-01-12 latest):**
- ✅ Removed empty confirmation gesture - goes directly to fill screen after empty measured
- ✅ Removed full confirmation gesture - goes directly to complete after full measurement
- ✅ Updated CAL_WAIT_FULL to require:
  - Weight delta > 300,000 ADC units (based on observed ~360k delta for 830ml)
  - Weight stability: < 5,000 ADC unit change over 5 seconds
  - Upright stable gesture (bottle vertical and steady)
- ✅ Allows bottle to be filled while held or placed vertically - more flexible UX
- ✅ Automatic progression once bottle is filled, placed upright, and weight stabilizes
- ✅ Added weight stability tracking with countdown feedback
- ✅ 5-second timeout after completion/error, then auto-return to main screen
- ✅ UI updated: "Fill Bottle" screen now says "Fill to 830ml / Then place / upright"
- ✅ Firmware builds successfully (RAM: 6.8%, Flash: 31.8%)

**Testing Status:**
- ✅ Hardware deployed and operational
- ✅ Inverted hold gesture working correctly
- ✅ Empty measurement passing with adjusted variance threshold (3540 ADC² < 5000 threshold)
- ⏳ **Updated calibration flow ready for testing** (empty → fill → upright stable → complete)

**Next Steps:**
- [ ] Upload firmware and test new simplified calibration flow (empty → fill → place upright → complete)
- [ ] Verify weight delta threshold (300,000 ADC units) triggers correctly with observed ~360k delta
- [ ] Verify NVS storage persists across power cycles
- [ ] Measure calibration accuracy with known water volumes
- [ ] Re-enable deep sleep after calibration testing complete

**Key Features:**
- Two-point calibration using 830ml bottle (empty + full)
- Bottle weight mathematically cancels out (no assumptions needed)
- Target accuracy: ±10-15ml
- Gesture-based interaction (no buttons required)
- E-paper display with static updates only (no progress bars due to hardware limitation)

---

## Next Up

### Firmware - Phase 2: Storage & Display
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
- [Hardware Research](Plans/001-hardware-research.md) - Component selection analysis
- [Adafruit BOM](Plans/002-bom-adafruit-feather.md) - UK parts list for Feather config
- [SparkFun BOM](Plans/003-bom-sparkfun-qwiic.md) - UK parts list for Qwiic config
