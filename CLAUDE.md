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
- ADXL343 accelerometer for wake-on-tilt
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
  - Adafruit ADXL343 - Accelerometer with wake interrupt ✅
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
- [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp) - USB serial commands for configuration (conditionally compiled)
- [firmware/src/ui_calibration.cpp](firmware/src/ui_calibration.cpp) - E-paper calibration UI screens (conditionally compiled)
- [firmware/src/display.cpp](firmware/src/display.cpp) - Smart display state tracking and rendering
- [firmware/include/config.h](firmware/include/config.h) - Debug flags, power management, sensor thresholds, IOS_MODE flag
- [firmware/include/aquavate.h](firmware/include/aquavate.h) - Version info and shared declarations
- [firmware/platformio.ini](firmware/platformio.ini) - Dual environment config (Adafruit Feather / SparkFun Qwiic)

### IRAM Constraints (Temporary - ESP32 V2 only)
The current ESP32 Feather V2 has limited IRAM (131KB). To fit BLE + all features, we use conditional compilation:

**IOS_MODE flag** in [config.h](firmware/include/config.h) - single flag that controls three mutually exclusive features:
- `IOS_MODE=1` (Production - default):
  - BLE enabled (iOS app communication)
  - Serial commands disabled (saves ~3.7KB IRAM)
  - Standalone calibration disabled (saves ~1KB IRAM)
  - IRAM: 125KB / 131KB (95.3%)
- `IOS_MODE=0` (Development/USB mode):
  - BLE disabled (saves ~45.5KB IRAM)
  - Serial commands enabled (USB configuration)
  - Standalone calibration enabled (inverted-hold gesture trigger)
  - IRAM: 82KB / 131KB (62.4%)

See [Plans/020-serial-commands-removal.md](Plans/020-serial-commands-removal.md) for implementation details.

### Firmware Debug Logging System

**IMPORTANT:** When adding any `Serial.print`, `Serial.println`, or `Serial.printf` to firmware code, first determine whether it should use the conditional debug system.

**Debug levels** (set via serial command `d0`-`d4`, `d9`):
- Level 0: All OFF
- Level 1: Events (drinks, refills, display) - enables `g_debug_drink_tracking`
- Level 2: + Gestures - enables `g_debug_calibration`
- Level 3: + Weight readings - enables `g_debug_water_level`
- Level 4: + Accelerometer - enables `g_debug_accelerometer`
- Level 9: All ON (including `g_debug_ble`)

**When to use unconditional `Serial.printf`:**
- ERROR messages (critical failures that always need visibility)
- WARNING messages (important but non-fatal issues)
- Major events like `=== DRINK DETECTED ===`, `=== REFILL DETECTED ===`

**When to use conditional `DEBUG_PRINTF` macro:**
- Verbose debug info (baseline values, intermediate calculations)
- Retry attempts, state transitions
- Any output that would clutter logs during normal operation

**Usage** (macros defined in [config.h](firmware/include/config.h)):
```cpp
#include "config.h"

// Unconditional - always shown
Serial.printf("ERROR: Something critical failed (code: 0x%x)\n", err);

// Conditional - only at debug level 1+ (drink tracking enabled)
DEBUG_PRINTF(g_debug_drink_tracking, "Drinks: baseline=%.1fml\n", baseline);

// Conditional - only at debug level 3+ (water level enabled)
DEBUG_PRINTF(g_debug_water_level, "Weight: ADC=%d\n", adc);
```

**TODO - Future Hardware Upgrade:** When upgrading to ESP32-S3 Feather (512KB IRAM):
- Remove IOS_MODE and all related conditional compilation
- Remove `#if ENABLE_BLE`, `#if ENABLE_SERIAL_COMMANDS`, `#if ENABLE_STANDALONE_CALIBRATION` blocks
- Enable all features simultaneously (BLE + serial commands + standalone calibration)
- Simplify [config.h](firmware/include/config.h), [main.cpp](firmware/src/main.cpp), [ble_service.cpp](firmware/src/ble_service.cpp), [serial_commands.cpp](firmware/src/serial_commands.cpp), [ui_calibration.cpp](firmware/src/ui_calibration.cpp)

### Firmware Build Commands
```bash
cd firmware
~/.platformio/penv/bin/platformio run                    # Build for Adafruit Feather (default)
~/.platformio/penv/bin/platformio run -e sparkfun_qwiic  # Build for SparkFun Qwiic
~/.platformio/penv/bin/platformio run -t upload          # Upload to connected board
~/.platformio/penv/bin/platformio device monitor         # Serial monitor
```

**Note:** The user will handle firmware uploads and device restarts manually. Do not attempt to upload firmware or wait for upload confirmation - just build and inform the user when ready.

### iOS Build Commands
```bash
cd ios/Aquavate
xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build
```

**Important:** Always use `iPhone 17` as the simulator name for test builds. Other common simulator names (e.g., iPhone 16) are not available on this system.

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
