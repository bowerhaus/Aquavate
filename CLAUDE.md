# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Aquavate is a smart water bottle project that measures daily water intake using a weight-based sensor system. The device communicates with an iOS app via Bluetooth Low Energy (BLE) and displays status on an e-paper display.

**Current Status:**
- Hardware design complete (v3.0 sensor puck)
- Firmware standalone features complete (phases 1-2.6)
- iOS app skeleton created
- Currently on branch: `extended-deep-sleep-backpack-mode` (ready to merge)

## Hardware Options

Two prototype configurations are being evaluated:

### Option A: Adafruit Feather Ecosystem (Recommended for simplicity)
- Adafruit ESP32 Feather V2 (STEMMA QT)
- 2.13" Mono E-Paper FeatherWing (stacks directly - no wiring)
- NAU7802 24-bit ADC for load cell
- LIS3DH accelerometer for wake-on-tilt
- UK BOM: [Plans/002-bom-adafruit-feather.md](Plans/002-bom-adafruit-feather.md)

### Option B: SparkFun Qwiic Ecosystem
- SparkFun ESP32-C6 Qwiic Pocket (smaller, WiFi 6)
- Waveshare 1.54" e-paper (requires SPI wiring)
- SparkFun Qwiic Scale NAU7802
- UK BOM: [Plans/003-bom-sparkfun-qwiic.md](Plans/003-bom-sparkfun-qwiic.md)

## Technology Stack

### Firmware (ESP32) - Standalone Complete
- **Framework:** PlatformIO + Arduino ✅
- **Language:** C++
- **Libraries Integrated:**
  - Adafruit EPD (ThinkInk) - 2.13" e-paper display ✅
  - Adafruit NAU7802 - Load cell ADC ✅
  - Adafruit LIS3DH - Accelerometer with wake interrupt ✅
  - NimBLE-Arduino - BLE communication (planned)

### iOS App - Skeleton Created
- **UI:** SwiftUI ✅
- **BLE:** CoreBluetooth (planned)
- **Storage:** CoreData (planned)
- **Architecture:** MVVM + ObservableObject (planned)

## Key Design Decisions

- **BLE over WiFi:** Lower power, excellent iOS support, industry standard for smart bottles
- **Weight-based tracking:** Load cell measures water removed rather than flow sensors
- **Wake-on-tilt:** Accelerometer interrupt wakes MCU from deep sleep when bottle is picked up
- **E-paper display:** Near-zero power when static, readable in sunlight
- **Dual deep sleep modes:** Normal (motion wake) + Extended (timer wake) for backpack scenarios

## User Requirements

- Battery life: 1-2 weeks between charges ✅
- Accuracy: ±15ml ✅ (currently ±10-15ml)
- Display: Daily total + battery status (landscape e-paper in side bump) ✅
- Deep sleep with wake-on-tilt power strategy ✅

## Important Implementation Notes

### Firmware Structure
- [firmware/src/main.cpp](firmware/src/main.cpp) - Main entry point, sensor initialization, wake/sleep logic, UI rendering
- [firmware/src/calibration.cpp](firmware/src/calibration.cpp) - Two-point calibration state machine
- [firmware/src/gestures.cpp](firmware/src/gestures.cpp) - Gesture detection (inverted hold, upright stable)
- [firmware/src/weight.cpp](firmware/src/weight.cpp) - Load cell reading and stability detection
- [firmware/src/drinks.cpp](firmware/src/drinks.cpp) - Daily intake tracking and drink detection
- [firmware/src/storage.cpp](firmware/src/storage.cpp) - NVS storage for calibration and settings
- [firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp) - NVS circular buffer for drink records
- [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp) - USB serial commands for configuration
- [firmware/src/ui_calibration.cpp](firmware/src/ui_calibration.cpp) - E-paper calibration UI screens
- [firmware/src/display.cpp](firmware/src/display.cpp) - Smart display state tracking and rendering
- [firmware/include/config.h](firmware/include/config.h) - Debug flags, power management, sensor thresholds, display config
- [firmware/include/aquavate.h](firmware/include/aquavate.h) - Version info and shared declarations
- [firmware/platformio.ini](firmware/platformio.ini) - Dual environment config (Adafruit Feather / SparkFun Qwiic)

### Build Commands
```bash
cd firmware
~/.platformio/penv/bin/platformio run                    # Build for Adafruit Feather (default)
~/.platformio/penv/bin/platformio run -e sparkfun_qwiic  # Build for SparkFun Qwiic
~/.platformio/penv/bin/platformio run -t upload          # Upload to connected board
~/.platformio/penv/bin/platformio device monitor         # Serial monitor
```

## Reference Documentation

Read these documents for progressive disclosure - CLAUDE.md keeps context light, these provide depth:

| Document | Purpose | When to Consult |
|----------|---------|-----------------|
| [PROGRESS.md](PROGRESS.md) | Current tasks and status updates | **Always check first** - Start of session, after context resume/reset, before making changes |
| [AGENTS.md](AGENTS.md) | Extended development workflow and patterns | Before making changes, understanding code style, common tasks, hardware testing |
| [docs/PRD.md](docs/PRD.md) | Complete product specification | Understanding features, BLE protocol, calibration logic, implementation phases - authoritative source for requirements |
| [Plans/004-sensor-puck-design.md](Plans/004-sensor-puck-design.md) | Mechanical design v3.0 | Hardware questions, physical constraints, assembly, dimensions - physical design affects firmware |
| [Plans/005-standalone-calibration-mode.md](Plans/005-standalone-calibration-mode.md) | Two-point calibration implementation | Understanding calibration flow, gestures, state machine |
| [Plans/006-usb-time-setting.md](Plans/006-usb-time-setting.md) | USB time setting via serial commands | Understanding time configuration, serial protocol |
| [Plans/007-daily-intake-tracking.md](Plans/007-daily-intake-tracking.md) | Daily intake tracking implementation | Understanding drink detection, daily reset, NVS storage |
| [Plans/008-drink-type-classification.md](Plans/008-drink-type-classification.md) | GULP vs POUR classification | Understanding drink types and thresholds |
| [Plans/009-smart-display-state-tracking.md](Plans/009-smart-display-state-tracking.md) | Display state tracking | Understanding display update logic and optimization |
| [Plans/010-deep-sleep-reinstatement.md](Plans/010-deep-sleep-reinstatement.md) | Deep sleep power management | Understanding normal sleep mode and wake-on-tilt |
| [Plans/011-extended-deep-sleep-backpack-mode.md](Plans/011-extended-deep-sleep-backpack-mode.md) | Extended deep sleep | Understanding dual sleep modes and backpack scenario |
| [Plans/001-hardware-research.md](Plans/001-hardware-research.md) | Component selection rationale | Understanding hardware limitations or evaluating alternatives |
| [Plans/002-bom-adafruit-feather.md](Plans/002-bom-adafruit-feather.md) | UK parts list (Adafruit) | Bill of materials for Feather configuration |
| [Plans/003-bom-sparkfun-qwiic.md](Plans/003-bom-sparkfun-qwiic.md) | UK parts list (SparkFun) | Bill of materials for Qwiic configuration |
