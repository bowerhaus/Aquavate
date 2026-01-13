# AGENTS.md

Additional guidance for AI agents working with the Aquavate codebase.

## Project Context

This is an active hardware + firmware + iOS app project. The firmware is well underway, with basic sensor integration and power management working. The iOS app is in skeleton stage.

## Working with This Codebase

### Before Making Changes

1. **Check PROGRESS.md** - Always read this first to understand active work and avoid conflicts
2. **Review recent commits** - Use `git log -10 --oneline` to see what's been done recently
3. **Check current branch** - This project uses feature branches, check which one you're on

### Firmware Development

**Key Files:**
- [firmware/src/main.cpp](firmware/src/main.cpp) - Main entry point, sensor initialization, UI rendering
- [firmware/src/calibration.cpp](firmware/src/calibration.cpp) - Two-point calibration state machine
- [firmware/src/gestures.cpp](firmware/src/gestures.cpp) - Gesture detection (inverted hold, upright stable)
- [firmware/src/weight.cpp](firmware/src/weight.cpp) - Load cell reading and stability detection
- [firmware/src/storage.cpp](firmware/src/storage.cpp) - NVS storage for calibration and settings
- [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp) - USB time setting commands
- [firmware/include/config.h](firmware/include/config.h) - Pin definitions, calibration constants, debug flags
- [firmware/platformio.ini](firmware/platformio.ini) - Build configurations for both hardware options

**Hardware Configurations:**
- Default: Adafruit ESP32 Feather V2 + 2.13" E-Paper FeatherWing
- Alternative: SparkFun ESP32-C6 Qwiic Pocket + Waveshare 1.54" E-Paper

**Build & Test:**
```bash
cd firmware
pio run                    # Build for Adafruit (default)
pio run -t upload          # Upload to board
pio device monitor         # Serial monitor
```

**Important Implementation Details:**
- LIS3DH INT1 pin must be wired to ESP32 GPIO 27 (not just I2C)
- LIS3DH needs always-on 3V power (not STEMMA QT which powers down)
- Current tilt threshold is 80° from vertical (configurable in config.h)
- Deep sleep current target is <200µA

### iOS Development

**Key Files:**
- [ios/Aquavate/Aquavate/AquavateApp.swift](ios/Aquavate/Aquavate/AquavateApp.swift) - App entry point
- [ios/Aquavate/Aquavate/ContentView.swift](ios/Aquavate/Aquavate/ContentView.swift) - Main view (currently "Hello World")

**Status:** Skeleton only - CoreBluetooth, CoreData, and UI implementation pending.

### Documentation Updates

When completing work:
1. Update [PROGRESS.md](PROGRESS.md) - Mark tasks complete, add new findings
2. Update [CLAUDE.md](CLAUDE.md) if key decisions or patterns change
3. Update [README.md](README.md) if features move from planned to implemented
4. Keep docs concise - move detailed research notes to Plans/ directory

### Code Style

**Firmware (C++):**
- Arduino-style setup() and loop() pattern
- Use Adafruit sensor libraries where available
- Add comments for hardware-specific behavior (interrupt config, power management)
- Serial debug output for development, with clear labels

**iOS (Swift):**
- SwiftUI for all views
- MVVM architecture with ObservableObject for state management
- CoreBluetooth for BLE (native, not third-party libraries)

## Common Tasks

### Adding a New Sensor Feature

1. Add hardware wiring notes to PRD.md
2. Update config.h with pin definitions or I2C addresses
3. Initialize in setup()
4. Read in loop() or create dedicated function
5. Update PROGRESS.md with what works and what's next

### Implementing BLE Characteristics

1. Reference PRD.md for GATT service design
2. Use NimBLE-Arduino library (already in platformio.ini)
3. Create separate ble/ directory under firmware/src/
4. Add to PROGRESS.md active work section

### Working with E-Paper Display

- Current implementation uses Adafruit_ThinkInk library
- Display updates are slow (~2s), plan accordingly
- Only update on wake events to save power
- Main screen shows: time (centered top), battery (top right), bottle graphic (left), water level in ml (right)
- Calibration UI has 7 static screens for the calibration wizard
- Smart refresh: only updates if water level changes by ≥5ml (configurable in config.h)

## Testing Hardware

### Without Hardware
- Code will compile but sensor init will fail gracefully
- Use serial monitor to see "NAU7802 not found" etc messages
- Good for testing build system and code structure

### With Hardware
- Check sensor I2C addresses: NAU7802 (0x2A), LIS3DH (0x18)
- Verify load cell wiring: Red (E+), Black (E-), White (A-), Green (A+)
- Test wake-on-tilt by tilting board >80°
- Monitor deep sleep current with multimeter (target <200µA)

## Reference Documents

- [docs/PRD.md](docs/PRD.md) - Complete product specification and firmware architecture
- [Plans/004-sensor-puck-design.md](Plans/004-sensor-puck-design.md) - Mechanical design and assembly
- [Plans/005-standalone-calibration-mode.md](Plans/005-standalone-calibration-mode.md) - Two-point calibration implementation
- [Plans/006-usb-time-setting.md](Plans/006-usb-time-setting.md) - USB time setting via serial commands
- [Plans/002-bom-adafruit-feather.md](Plans/002-bom-adafruit-feather.md) - UK parts list

## Questions?

When uncertain:
1. Check PRD.md for design decisions
2. Check PROGRESS.md for current status
3. Look at recent git commits for context
4. Ask the user if requirements are ambiguous
