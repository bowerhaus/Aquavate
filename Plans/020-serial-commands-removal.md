# IRAM Reduction Plan: Conditional BLE/Serial Commands Compilation

**Date:** 2026-01-17
**Status:** ✅ Complete
**Branch:** `serial-commands-removal`

---

## Objective
Reduce IRAM usage to fit within ESP32 limits by **conditionally compiling** BLE and serial command modules with mutually exclusive flags.

## User Requirements
1. Revert from ESP-IDF BLE back to NimBLE (ESP-IDF didn't save IRAM)
2. Keep serial logging/debug output (Serial.print/println) - unconditional
3. Add TWO compile-time flags for different use modes
4. Flags are mutually exclusive (compile error if both enabled)
5. Single build configuration (flags in config.h)
6. Target: Get under 131,072 bytes IRAM limit

---

## Strategy Summary

**Single master flag `IOS_MODE` controls two mutually exclusive compile-time flags:**

### Master Flag: `IOS_MODE`
- Set to 1 for iOS App Mode (default)
- Set to 0 for Standalone USB Mode
- Auto-configures `ENABLE_BLE` and `ENABLE_SERIAL_COMMANDS`

### Auto-Configured Flags:

#### `ENABLE_BLE` (controlled by IOS_MODE)
- IOS_MODE=1: ENABLE_BLE=1 (enabled for production)
- IOS_MODE=0: ENABLE_BLE=0 (disabled, saves ~45.5KB IRAM)

#### `ENABLE_SERIAL_COMMANDS` (controlled by IOS_MODE)
- IOS_MODE=1: ENABLE_SERIAL_COMMANDS=0 (disabled, saves ~3.7KB IRAM)
- IOS_MODE=0: ENABLE_SERIAL_COMMANDS=1 (enabled for USB configuration)

### Two Use Modes:

**Mode 1: iOS App Mode (Production - default)**
- Set `IOS_MODE=1` in [config.h](firmware/src/config.h)
- BLE enabled for iOS app communication
- Serial commands disabled (saves ~3.7KB IRAM)
- Debug output still works via Serial.print
- **IRAM:** 127,362 bytes / 131,072 bytes (97.2%) ✅
- **Result:** Fits comfortably with 3,710 bytes headroom

**Mode 2: Standalone USB Mode (Development)**
- Set `IOS_MODE=0` in [config.h](firmware/src/config.h)
- BLE disabled (saves ~45.5KB IRAM)
- Serial commands enabled for USB configuration
- **IRAM:** 81,846 bytes / 131,072 bytes (62.4%) ✅
- **Result:** Fits with large 49,226 bytes headroom

### Sanity Check:
- Compile-time error if both `ENABLE_BLE` and `ENABLE_SERIAL_COMMANDS` are enabled
- Prevents accidental IRAM overflow

**Files to modify:**
- config.h - Add both flags + sanity check
- ble_service.cpp/.h - Wrap in #if ENABLE_BLE
- serial_commands.cpp/.h - Wrap in #if ENABLE_SERIAL_COMMANDS
- main.cpp - Wrap includes and calls in #if blocks
- platformio.ini - Add NimBLE back (revert from ESP-IDF)

**Verified IRAM savings:**
- iOS App Mode: ~3.7KB saved (serial commands disabled)
- Standalone USB Mode: ~45.5KB saved (BLE disabled)

---

## Implementation Phases

### Phase 1: Revert ESP-IDF BLE back to NimBLE ✅
- Checkout ble_service.cpp/h from ble-integration branch
- Add NimBLE library to platformio.ini

### Phase 2: Add Feature Flags to config.h ✅
- Added IOS_MODE master flag (default: 1)
- Auto-configure ENABLE_BLE based on IOS_MODE
- Auto-configure ENABLE_SERIAL_COMMANDS based on IOS_MODE
- Add sanity check: error if both enabled
- Add documentation comments for both modes

### Phase 3: Wrap BLE Module ✅
- Wrap ble_service.h with #if ENABLE_BLE
- Wrap ble_service.cpp with #if ENABLE_BLE
- All BLE code conditionally compiled

### Phase 4: Wrap Serial Commands Module ✅
- Wrap serial_commands.h with #if ENABLE_SERIAL_COMMANDS
- Wrap serial_commands.cpp with #if ENABLE_SERIAL_COMMANDS
- All serial command code conditionally compiled

### Phase 5: Update main.cpp ✅
- Wrap BLE includes and calls with #if ENABLE_BLE
- Wrap serial command includes and calls with #if ENABLE_SERIAL_COMMANDS
- Ensure debug output (Serial.print) remains unconditional

### Phase 6: Build and Verify ✅
- Build with IOS_MODE=1 (ENABLE_BLE=1, ENABLE_SERIAL_COMMANDS=0)
  - IRAM: 127,362 / 131,072 bytes (97.2%) ✅
  - Headroom: 3,710 bytes
- Build with IOS_MODE=0 (ENABLE_BLE=0, ENABLE_SERIAL_COMMANDS=1)
  - IRAM: 81,846 / 131,072 bytes (62.4%) ✅
  - Headroom: 49,226 bytes
- Verified compile error when both flags enabled ✅

### Phase 7: Update Documentation ✅
- Updated PROGRESS.md with results
- Updated this plan with IOS_MODE master flag
- Documented simplified mode switching (single flag)

---

## Usage

### Switching Between Modes

Edit [firmware/src/config.h](firmware/src/config.h) and change the `IOS_MODE` flag:

```cpp
// For iOS App Mode (Production - default)
#define IOS_MODE    1

// For Standalone USB Mode (Development)
#define IOS_MODE    0
```

Then rebuild:
```bash
cd firmware
~/.platformio/penv/bin/platformio run
```

### Mode Comparison

| Feature | iOS Mode (IOS_MODE=1) | USB Mode (IOS_MODE=0) |
|---------|----------------------|----------------------|
| BLE Communication | ✅ Enabled | ❌ Disabled |
| Serial Commands | ❌ Disabled | ✅ Enabled |
| Serial Debug Output | ✅ Enabled | ✅ Enabled |
| IRAM Usage | 127,362 bytes (97.2%) | 81,846 bytes (62.4%) |
| IRAM Headroom | 3,710 bytes | 49,226 bytes |
| Use Case | Production firmware | USB configuration |

### Serial Commands (USB Mode Only)

Available when `IOS_MODE=0`:
- Time setting
- Calibration control
- Configuration commands

See [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp) for full command list.

---

## Results

✅ **Problem Solved:** IRAM overflow resolved for both build modes
- Production mode fits comfortably with 3,710 bytes headroom
- Development mode has large 49,226 bytes headroom
- Single flag (`IOS_MODE`) controls entire configuration
- Sanity check prevents accidental IRAM overflow
