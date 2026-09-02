# Aquavate

A smart water bottle that measures daily water intake using a weight-based sensor system. The device communicates with an iOS app via Bluetooth Low Energy (BLE) and displays status on an e-paper display.

## Project Structure

```
Aquavate/
├── firmware/          # ESP32 PlatformIO project
├── ios/               # iOS SwiftUI app (skeleton)
├── hardware/          # 3D print files and PCB designs
└── Plans/             # Hardware research and BOMs
```

## Hardware Options

Two prototype configurations are supported:

| Feature | Adafruit Feather (Default) | SparkFun Qwiic |
|---------|---------------------------|----------------|
| MCU | ESP32 Feather V2 | ESP32-C6 Qwiic Pocket |
| Display | 2.13" E-Paper FeatherWing | Waveshare 1.54" E-Paper |
| Form Factor | Larger, easier assembly | Smaller, more wiring |
| UK Cost | ~£82 | ~£87 |

See [Plans/002-bom-adafruit-feather.md](Plans/002-bom-adafruit-feather.md) and [Plans/003-bom-sparkfun-qwiic.md](Plans/003-bom-sparkfun-qwiic.md) for full component lists.

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- USB cable for ESP32 programming

### Building Firmware

```bash
cd firmware

# Build for Adafruit Feather (default)
pio run

# Build for SparkFun Qwiic
pio run -e sparkfun_qwiic

# Upload to connected board
pio run -t upload

# Upload to specific environment
pio run -e sparkfun_qwiic -t upload

# Open serial monitor
pio device monitor
```

### Switching Between Hardware Configurations

The firmware uses PlatformIO environments to support both hardware configurations. Each environment sets appropriate build flags:

- `adafruit_feather` - Sets `-DBOARD_ADAFRUIT_FEATHER` and `-DDISPLAY_FEATHERWING_213`
- `sparkfun_qwiic` - Sets `-DBOARD_SPARKFUN_QWIIC` and `-DDISPLAY_WAVESHARE_154`

Pin definitions are automatically selected based on these flags.

## Features

### Implemented
- ✅ Weight-based water tracking via NAU7802 load cell ADC
- ✅ Wake-on-tilt using ADXL343 accelerometer interrupt (single-tap or motion)
- ✅ E-paper display with battery status, time, and bottle graphic
- ✅ Deep sleep with dual modes for 1-2 week battery life:
  - Normal sleep: 30s timeout with motion wake (EXT0 interrupt)
  - Extended sleep: 60s timer wake during continuous motion (backpack mode)
- ✅ Battery voltage monitoring with 20% quantized steps
- ✅ Two-point calibration system (empty + full bottle)
- ✅ Gesture-based calibration trigger (inverted hold for 5s)
- ✅ Real-time water level measurement and display (±10-15ml accuracy)
- ✅ Daily water intake tracking with drink detection:
  - GULP: <100ml (individual sips)
  - POUR: ≥100ml (refills with baseline update)
- ✅ Daily reset at 4am boundary with 20-hour fallback logic
- ✅ NVS circular buffer storage (600 records = 30 days history)
- ✅ Dual visualization modes (human figure or tumbler grid)
- ✅ Hardcoded 2500ml daily goal
- ✅ Smart display state tracking (only updates when values change)
- ✅ RTC memory persistence for display state and drink baseline
- ✅ USB serial commands for configuration:
  - Time/Timezone: SET DATETIME, SET DATE, SET TIME, SET TZ, GET TIME
  - Drink Tracking: GET DAILY STATE, GET LAST DRINK, DUMP DRINKS, RESET DAILY INTAKE, CLEAR DRINKS
  - Display Settings: SET DISPLAY MODE (0=human, 1=tumblers)
  - E-Paper Diagnostics: EPD PATTERN, EPD TEST, EPD WAIT, EPD TEMP, EPD VCOM, EPD LUT
  - Power Management: SET SLEEP TIMEOUT, SET EXTENDED SLEEP TIMER, SET EXTENDED SLEEP THRESHOLD
  - System Status: GET STATUS (shows all system settings)
  - Debug Control: 0-4, 9 (debug levels), T (test interrupt state)
- ✅ ESP32 internal RTC with NVS-based time persistence
- ✅ Timezone support with NVS persistence

### Planned
- 📋 BLE communication with iOS app
- 📋 Drink history sync protocol
- 📋 Empty gesture detection (invert + shake)

## Documentation

### Active Development
- [PROGRESS.md](PROGRESS.md) - Current work tracker and active tasks

### Product & Design
- [PRD.md](docs/PRD.md) - Full product requirements document
- [Sensor Puck Design](Plans/004-sensor-puck-design.md) - Mechanical design v3.0 for 3D printing
- [Hardware Research](Plans/001-hardware-research.md) - Component selection and analysis

### Configuration
- [CLAUDE.md](CLAUDE.md) - Guidance for Claude Code when working on this project
- [AGENTS.md](AGENTS.md) - Extended development workflow and patterns

## Current Status

**Branch:** `extended-deep-sleep-backpack-mode`
**Phase:** Phase 2.6 complete - Extended deep sleep (backpack mode) implemented
**Status:** All standalone features working - Ready for BLE integration

See [PROGRESS.md](PROGRESS.md) for detailed status and next steps.

## License

This project is dual-licensed:

- **Software** (firmware, iOS app): [PolyForm Noncommercial 1.0.0](licenses/POLYFORM-NONCOMMERCIAL-1.0.0.txt)
- **Hardware & Documentation**: [CC BY-NC-SA 4.0](licenses/CC-BY-NC-SA-4.0.txt)

See [LICENSE.md](LICENSE.md) for full details and commercial licensing inquiries.
