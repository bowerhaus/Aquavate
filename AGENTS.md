# AGENTS.md

Additional guidance for AI agents working with the Aquavate codebase.

## Project Context

This is an active hardware + firmware + iOS app project. The firmware standalone features are complete (phases 1-2.6), with sensor integration, power management, daily intake tracking, and deep sleep working. The iOS app is in skeleton stage. Next phase is BLE integration.

## Working with This Codebase

### Before Making Changes

1. **Check PROGRESS.md** - Always read this first to understand active work and avoid conflicts
2. **Review recent commits** - Use `git log -10 --oneline` to see what's been done recently
3. **Check current branch** - This project uses feature branches, check which one you're on

### Firmware Development

**Key Files:**
- [firmware/src/main.cpp](firmware/src/main.cpp) - Main entry point, sensor initialization, wake/sleep logic, UI rendering
- [firmware/src/calibration.cpp](firmware/src/calibration.cpp) - Two-point calibration state machine
- [firmware/src/gestures.cpp](firmware/src/gestures.cpp) - Gesture detection (inverted hold, upright stable)
- [firmware/src/weight.cpp](firmware/src/weight.cpp) - Load cell reading and stability detection
- [firmware/src/drinks.cpp](firmware/src/drinks.cpp) - Daily intake tracking and drink detection
- [firmware/src/storage.cpp](firmware/src/storage.cpp) - NVS storage for calibration and settings
- [firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp) - NVS circular buffer for drink records
- [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp) - USB serial commands for configuration
- [firmware/src/display.cpp](firmware/src/display.cpp) - Smart display state tracking and rendering
- [firmware/include/config.h](firmware/include/config.h) - Pin definitions, calibration constants, debug flags, power settings
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
- Wake-on-tilt threshold: 0.80g (0x32)
- Deep sleep: Dual modes (normal 30s with motion wake, extended 60s with timer wake)
- RTC memory: Used for display state and drink baseline persistence across sleep
- NVS: Circular buffer with 600 records (30 days history)

### iOS Development

**Key Files:**
- [ios/Aquavate/Aquavate/AquavateApp.swift](ios/Aquavate/Aquavate/AquavateApp.swift) - App entry point
- [ios/Aquavate/Aquavate/ContentView.swift](ios/Aquavate/Aquavate/ContentView.swift) - Main view (currently "Hello World")

**Status:** Skeleton only - CoreBluetooth, CoreData, and UI implementation pending.

### Documentation Updates

When completing work:
1. **PROGRESS.md** - Keep lean and current. Delete completed sections, only track active work.
2. **CLAUDE.md** - Update if key decisions or patterns change
3. **README.md** - Update if features move from planned to implemented
4. Keep docs concise - move detailed research notes to Plans/ directory

### Code Style

**Firmware (C++):**
- Arduino-style setup() and loop() pattern
- Use Adafruit sensor libraries where available
- Add comments for hardware-specific behavior (interrupt config, power management)
- Serial debug output for development, with debug level control (0-4, 9)
- Smart state tracking: only update when values change

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
- Smart state tracking: display.cpp tracks all displayed values and only updates when changed
- Main screen shows: time (centered top), battery (top right), bottle graphic (left), water level (right), daily intake visualization (center)
- Optional "Zzzz" indicator for extended sleep mode

### Serial Commands for Testing

The firmware supports comprehensive serial commands for configuration and testing:

**Debug Control:**
- `0-4`, `9` - Set debug level (0=OFF, 1=Events, 2=+Gestures, 3=+Weight, 4=+Accel, 9=All)
- `T` - Test interrupt state (shows LIS3DH INT1_SRC register)

**Time/Timezone:**
- `SET DATETIME YYYY-MM-DD HH:MM:SS [tz]` - Set date, time, and timezone
- `SET DATE YYYY-MM-DD` - Set date only
- `SET TIME HH[:MM[:SS]]` - Set time (flexible format)
- `SET TZ offset` - Set timezone offset
- `GET TIME` - Show current time

**Drink Tracking:**
- `GET DAILY STATE` - Show current daily intake
- `GET LAST DRINK` - Show most recent drink record
- `DUMP DRINKS` - Display all drink records
- `SET DAILY INTAKE ml` - Set current daily intake
- `RESET DAILY INTAKE` - Reset daily counter
- `CLEAR DRINKS` - Clear all drink records

**Display:**
- `SET DISPLAY MODE mode` - Switch visualization (0=human, 1=tumblers)

**Power Management:**
- `SET SLEEP TIMEOUT sec` - Normal sleep timeout (0=disable, default=30)
- `SET EXTENDED SLEEP TIMER sec` - Extended sleep wake timer (default=60)
- `SET EXTENDED SLEEP THRESHOLD sec` - Awake threshold for extended mode (default=120)

**System:**
- `GET STATUS` - Show all system status and settings

## Testing Hardware

### Without Hardware
- Code will compile but sensor init will fail gracefully
- Use serial monitor to see "NAU7802 not found" etc messages
- Good for testing build system and code structure

### With Hardware
- Check sensor I2C addresses: NAU7802 (0x2A), LIS3DH (0x18)
- Verify load cell wiring: Red (E+), Black (E-), White (A-), Green (A+)
- Test wake-on-tilt by tilting board
- Monitor deep sleep current with multimeter (target <200µA)
- Use serial commands to configure and test features

## Reference Documents

- [docs/PRD.md](docs/PRD.md) - Complete product specification and firmware architecture
- [Plans/004-sensor-puck-design.md](Plans/004-sensor-puck-design.md) - Mechanical design and assembly
- [Plans/005-standalone-calibration-mode.md](Plans/005-standalone-calibration-mode.md) - Two-point calibration implementation
- [Plans/006-usb-time-setting.md](Plans/006-usb-time-setting.md) - USB time setting via serial commands
- [Plans/007-daily-intake-tracking.md](Plans/007-daily-intake-tracking.md) - Daily intake tracking implementation
- [Plans/010-deep-sleep-reinstatement.md](Plans/010-deep-sleep-reinstatement.md) - Deep sleep power management
- [Plans/011-extended-deep-sleep-backpack-mode.md](Plans/011-extended-deep-sleep-backpack-mode.md) - Extended deep sleep for backpack mode
- [Plans/002-bom-adafruit-feather.md](Plans/002-bom-adafruit-feather.md) - UK parts list

## Questions?

When uncertain:
1. Check PRD.md for design decisions
2. Check PROGRESS.md for current status
3. Look at recent git commits for context
4. Ask the user if requirements are ambiguous
