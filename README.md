# Aquavate

A smart water bottle that measures daily water intake using a weight-based sensor system. The device communicates with an iOS app via Bluetooth Low Energy (BLE) and displays status on an e-paper display.

## Project Structure

```
Aquavate/
├── firmware/          # ESP32 PlatformIO project
├── ios/               # iOS SwiftUI app (coming soon)
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
- ✅ Wake-on-tilt using LIS3DH accelerometer interrupt (80° threshold)
- ✅ E-paper display with battery status, time, and bottle graphic
- ✅ Deep sleep with EXT0 wake-up for 1-2 week battery life
- ✅ Battery voltage monitoring and percentage display
- ✅ Two-point calibration system (empty + full bottle)
- ✅ Gesture-based calibration (inverted hold for 5s triggers calibration)
- ✅ Real-time water level measurement and display
- ✅ Daily water intake tracking with drink detection (≥30ml threshold)
- ✅ Refill detection (≥100ml threshold) with baseline updates
- ✅ 5-minute drink aggregation window for multiple sips
- ✅ Daily reset at 4am boundary with 20-hour fallback logic
- ✅ Drink record storage in NVS (200-record circular buffer)
- ✅ Dual visualization modes (human figure or tumbler grid)
- ✅ USB time setting via granular serial commands:
  - SET_DATETIME (combined date+time+timezone)
  - SET_DATE (date only)
  - SET_TIME (time only with flexible HH[:MM[:SS]] format)
  - SET_TZ (timezone offset)
  - GET_TIME (display current time)
- ✅ Case-insensitive command parsing
- ✅ Timezone support with NVS persistence
- ✅ ESP32 internal RTC with smart time persistence:
  - Saves on drink/refill events (opportunistic)
  - Saves hourly on the hour (periodic fallback)
  - Restores from NVS on boot
  - Only saves when DS3231 RTC not present (future-proof)
- ✅ Runtime debug level control (0-4, 9) via single-character commands

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

## Current Status

**Branch:** `daily-water-intake-tracking`
**Phase:** Phase 2 complete - Daily water intake tracking fully implemented
**Status:** All standalone features working - Ready for BLE integration

See [PROGRESS.md](PROGRESS.md) for detailed status and next steps.

## License

MIT
