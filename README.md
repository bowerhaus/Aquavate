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

## Features (Planned)

- Weight-based water tracking via NAU7802 load cell ADC
- Wake-on-tilt using LIS3DH accelerometer interrupt
- E-paper display showing current level and daily total
- BLE communication with iOS app
- Deep sleep for 1-2 week battery life
- DS3231 RTC for standalone daily tracking

## Documentation

- [Hardware Research](Plans/001-hardware-research.md) - Component selection and analysis
- [Sensor Puck Design](Plans/004-sensor-puck-design.md) - Mechanical design for 3D printing

## License

MIT
