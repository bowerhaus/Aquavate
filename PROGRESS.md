# Aquavate - Active Development Progress

**Last Updated:** 2026-01-12

---

## Current Branch: `Calibration`

Working on wake-on-tilt functionality and sensor calibration.

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

### Firmware - Calibration & Measurement Logic
- [ ] Implement vertical orientation detection (Z-axis > 0.9g)
- [ ] Add stability detection (variance monitoring over 2s window)
- [ ] Create weight measurement routine (10 samples, outlier removal, averaging)
- [ ] Implement drink detection algorithm (±30ml threshold)
- [ ] Add tare calibration support
- [ ] Add load cell scale factor calibration
- [ ] Store calibration data in NVS

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
- `Calibration` - Active: Wake-on-tilt working, adding calibration next
- `Hello-IOS` - Merged: Initial iOS project
- `Hardware-design` - Merged: Sensor puck v3.0 design
- `Initial-PRD` - Merged: Product requirements document

---

## Reference Documents

- [PRD.md](docs/PRD.md) - Full product requirements
- [Sensor Puck Design](Plans/004-sensor-puck-design.md) - Mechanical design v3.0
- [Hardware Research](Plans/001-hardware-research.md) - Component selection analysis
- [Adafruit BOM](Plans/002-bom-adafruit-feather.md) - UK parts list for Feather config
- [SparkFun BOM](Plans/003-bom-sparkfun-qwiic.md) - UK parts list for Qwiic config
