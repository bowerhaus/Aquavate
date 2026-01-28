# Calibration Circular Dependency Fix

## Problem Summary

The calibration system has a **circular dependency** that makes recalibration impossible when calibration data becomes corrupt:

1. Calibration (`CAL_WAIT_EMPTY`) requires `GESTURE_UPRIGHT_STABLE`
2. `UPRIGHT_STABLE` requires weight stability within **6ml** for 2+ seconds ([gestures.cpp:298](firmware/src/gestures.cpp))
3. Weight in ml is calculated using: `water_ml = adc_from_empty / scale_factor`
4. If `scale_factor` is corrupt (too small/large), tiny ADC noise → huge ml swings
5. Weight never stabilizes → `UPRIGHT_STABLE` never triggers → **can't recalibrate**

---

## Root Cause Analysis: How Calibration Became Corrupt

### Confirmed Cause: iOS App Bottle Settings Change

The user changed bottle settings in the iOS app, which triggered `bleSaveBottleConfig()` ([ble_service.cpp:592-617](firmware/src/ble_service.cpp)).

**The corruption path:**
1. iOS app writes `BLE_BottleConfig` struct with `scale_factor`, `tare_weight_grams`, `capacity`, `goal`
2. `bleSaveBottleConfig()` accepts values **with NO validation**
3. Firmware overwrites calibration: `cal.scale_factor = bottleConfig.scale_factor`
4. Recalculates ADC: `empty_adc = tare_weight_grams * scale_factor`
5. If iOS app sent wrong values (e.g., `scale_factor = 1.0` default), calibration is corrupted

**Key issue in [ble_service.cpp:598](firmware/src/ble_service.cpp):**
```cpp
cal.scale_factor = bottleConfig.scale_factor;  // No validation!
cal.empty_bottle_adc = (int32_t)(bottleConfig.tare_weight_grams * bottleConfig.scale_factor);
```

With `scale_factor = 1.0` (the firmware's default value):
- Tiny ADC noise (±100 units) = ±100ml fluctuation
- Weight never stabilizes → can't recalibrate

### Other Corruption Paths (Lower Risk)

| Path | Validation Gap | Risk |
|------|----------------|------|
| **BLE CAL_SET_DATA** ([ble_service.cpp:222](firmware/src/ble_service.cpp)) | Only checks `scale_factor > 0`, no upper bound | iOS app could send bad values |
| **Standalone calibration** ([calibration.cpp:291](firmware/src/calibration.cpp)) | Checks `0 < scale_factor ≤ 1000` | 1000 is still too high (expected: 200-600) |
| **Zero-tare via BLE** ([main.cpp:1246](firmware/src/main.cpp)) | Recalculates full_bottle_adc from existing scale_factor | If scale_factor already wrong, makes it worse |
| **Serial tare** ([serial_commands.cpp:776](firmware/src/serial_commands.cpp)) | Recalculates scale from mismatched ADC values | Not applicable (serial was disabled) |

---

## Diagnostic Steps

Before implementing the fix, check current calibration state via serial (build with `IOS_MODE=0`):

```
s         # GET STATUS - shows current calibration values
```

Look for:
- `scale_factor` value (expected: 200-600 ADC/g)
- `empty_adc` and `full_adc` values
- Check if: `(full_adc - empty_adc) / 830 ≈ scale_factor`

If scale_factor is outside 200-600 range, calibration is corrupt.

---

## Implementation Plan

### Phase 1: Add Scale Factor Sanity Check (Prevents Future Corruption)

**File: [firmware/include/config.h](firmware/include/config.h)**

Add bounds constants:
```cpp
// Calibration scale factor bounds (ADC counts per gram)
// Based on NAU7802 at 128 gain with typical load cells:
// Expected range: ~200-600 ADC/g
#define CALIBRATION_SCALE_FACTOR_MIN    100.0f
#define CALIBRATION_SCALE_FACTOR_MAX    800.0f
```

**File: [firmware/src/storage.cpp](firmware/src/storage.cpp)**

In `storageLoadCalibration()`, add validation after loading:
```cpp
// Sanity check: validate scale factor is within reasonable bounds
if (cal.calibration_valid == 1) {
    if (cal.scale_factor < CALIBRATION_SCALE_FACTOR_MIN ||
        cal.scale_factor > CALIBRATION_SCALE_FACTOR_MAX) {
        Serial.printf("Storage: WARNING - scale_factor %.2f out of range [%.0f-%.0f], marking invalid\n",
                     cal.scale_factor, CALIBRATION_SCALE_FACTOR_MIN, CALIBRATION_SCALE_FACTOR_MAX);
        cal.calibration_valid = 0;
    }
}
```

### Phase 2: Add Calibration Mode to Gestures (Breaks Circular Dependency)

**File: [firmware/include/gestures.h](firmware/include/gestures.h)**

Add new function declaration:
```cpp
// Set calibration mode - skips ml-based weight stability check
void gesturesSetCalibrationMode(bool enabled);
```

**File: [firmware/src/gestures.cpp](firmware/src/gestures.cpp)**

Add static flag and implementation:
```cpp
static bool g_calibration_mode = false;

void gesturesSetCalibrationMode(bool enabled) {
    g_calibration_mode = enabled;
    if (enabled) {
        g_upright_active = false;
        g_upright_start_time = 0;
        g_last_stable_weight = 0.0f;
        Serial.println("Gestures: Calibration mode ENABLED (accelerometer-only stability)");
    } else {
        Serial.println("Gestures: Calibration mode DISABLED (normal weight stability)");
    }
}
```

Modify UPRIGHT_STABLE detection (around line 295):
```cpp
bool weight_stable;
if (g_calibration_mode) {
    weight_stable = true;  // Skip weight check during calibration
} else {
    float weight_delta = fabs(weight_ml - g_last_stable_weight);
    weight_stable = (weight_delta < 6.0f);
    // ... existing logic ...
}
```

### Phase 3: Enable Calibration Mode During Calibration

**File: [firmware/src/calibration.cpp](firmware/src/calibration.cpp)**

In `calibrationStart()`:
```cpp
gesturesSetCalibrationMode(true);
```

In `calibrationCancel()` and CAL_ERROR transitions:
```cpp
gesturesSetCalibrationMode(false);
```

After successful calibration (CAL_COMPLETE):
```cpp
gesturesSetCalibrationMode(false);
```

### Phase 4: Fix BLE Bottle Config Validation (Root Cause Fix)

**File: [firmware/src/ble_service.cpp](firmware/src/ble_service.cpp)**

In `bleSaveBottleConfig()`, add validation before saving (around line 597):
```cpp
void bleSaveBottleConfig() {
    CalibrationData cal;

    // Load existing calibration to preserve ADC values
    if (storageLoadCalibration(cal)) {
        // VALIDATE incoming scale_factor before accepting
        if (bottleConfig.scale_factor < CALIBRATION_SCALE_FACTOR_MIN ||
            bottleConfig.scale_factor > CALIBRATION_SCALE_FACTOR_MAX) {
            BLE_DEBUG_F("ERROR: Rejecting invalid scale_factor %.2f (range: %.0f-%.0f)",
                       bottleConfig.scale_factor, CALIBRATION_SCALE_FACTOR_MIN, CALIBRATION_SCALE_FACTOR_MAX);
            // Reload correct value from NVS
            bottleConfig.scale_factor = cal.scale_factor;
            return;  // Don't save corrupt data
        }

        // Update scale factor (now validated)
        cal.scale_factor = bottleConfig.scale_factor;
        // ... rest unchanged
    }
}
```

Also add validation in `BottleConfigCallbacks::onWrite()` (around line 128):
```cpp
// Validate scale_factor before accepting
if (bottleConfig.scale_factor < CALIBRATION_SCALE_FACTOR_MIN ||
    bottleConfig.scale_factor > CALIBRATION_SCALE_FACTOR_MAX) {
    BLE_DEBUG_F("WARNING: Invalid scale_factor %.2f received, ignoring",
               bottleConfig.scale_factor);
    bleLoadBottleConfig();  // Restore valid values
    return;
}
```

### Phase 5: Fix Serial Tare Command Validation (IOS_MODE=0 only)

**File: [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp)**

After recalculating scale_factor, validate it:
```cpp
cal.scale_factor = (float)(cal.full_bottle_adc - cal.empty_bottle_adc) / 830.0f;

// Validate new scale factor
if (cal.scale_factor < CALIBRATION_SCALE_FACTOR_MIN ||
    cal.scale_factor > CALIBRATION_SCALE_FACTOR_MAX) {
    Serial.printf("WARNING: Tare would create invalid scale_factor (%.2f)\n", cal.scale_factor);
    Serial.println("Full recalibration required - run standalone calibration");
    cal.calibration_valid = 0;
}
```

---

## Files to Modify

| File | Changes |
|------|---------|
| [firmware/src/config.h](firmware/src/config.h) | Add `CALIBRATION_SCALE_FACTOR_MIN/MAX` constants |
| [firmware/include/gestures.h](firmware/include/gestures.h) | Add `gesturesSetCalibrationMode()` declaration |
| [firmware/src/gestures.cpp](firmware/src/gestures.cpp) | Add calibration mode flag and bypass logic |
| [firmware/src/storage.cpp](firmware/src/storage.cpp) | Add scale_factor range validation on load |
| [firmware/src/calibration.cpp](firmware/src/calibration.cpp) | Enable/disable calibration mode at start/end |
| [firmware/src/ble_service.cpp](firmware/src/ble_service.cpp) | **Root cause fix**: Validate scale_factor in `bleSaveBottleConfig()` and `BottleConfigCallbacks` |
| [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp) | Validate scale_factor after tare recalculation (IOS_MODE=0 only) |

### Documentation Update

| File | Changes |
|------|---------|
| [CLAUDE.md](CLAUDE.md) | Update IOS_MODE section (lines 84-94) to reflect that standalone calibration is now enabled in both modes |

**Current (outdated):**
```
- `IOS_MODE=1` (Production - default):
  - Standalone calibration disabled (saves ~1KB IRAM)
```

**Should be:**
```
- `IOS_MODE=1` (Production - default):
  - Standalone calibration enabled (bottle-driven, iOS mirrors state)
```

---

## Verification

### Testing the Fix
Standalone calibration is enabled in both iOS mode and USB mode, so the fix works in normal production builds:

1. **Boot with corrupt calibration** - Device detects invalid scale_factor on load
2. **Trigger calibration** - Hold bottle inverted 5 seconds (works in iOS mode)
3. **Place empty bottle upright** - Should detect `UPRIGHT_STABLE` (accelerometer-only mode)
4. **Complete calibration** - Fill to 830ml, place upright
5. **Test normal operation** - Verify drink detection and weight stability work

### Optional: Serial Debug (IOS_MODE=0)
For detailed logging, build with `IOS_MODE=0` to see:
- `"Storage: WARNING - scale_factor X.XX out of range, marking invalid"`
- Calibration state transitions
- Use serial `s` command to verify scale_factor is in 200-600 range

### Key Verification Points
- **No mode switching required** - Fix works in production iOS mode
- **Automatic detection** - Invalid scale_factor is caught on boot
- **Self-recovery** - Device can recalibrate using inverted hold gesture

### iOS App Fix (Separate Task)

**IMPORTANT:** When implementing features that update bottle configuration (e.g., daily goal), the iOS app must handle the `BLE_BottleConfig` struct carefully to avoid corrupting calibration.

#### The Problem

The `BLE_BottleConfig` struct contains multiple fields:
```swift
struct BLE_BottleConfig {
    var scale_factor: Float        // Calibration: ADC counts per gram
    var tare_weight_grams: Int32   // Calibration: empty bottle weight
    var bottle_capacity_ml: UInt16 // User setting: bottle size
    var daily_goal_ml: UInt16      // User setting: daily water goal
}
```

When the iOS app writes this characteristic, **all fields are sent to the firmware**. If the app only wants to update `daily_goal_ml` but doesn't properly preserve `scale_factor` and `tare_weight_grams`, the calibration gets corrupted.

#### Recommended Approach: Read-Modify-Write

When updating any field in `BLE_BottleConfig`:

1. **Read the current config from the firmware first**
   ```swift
   // Read the Bottle Config characteristic to get current values
   let currentConfig = readBottleConfig()
   ```

2. **Modify only the field(s) you want to change**
   ```swift
   var updatedConfig = currentConfig
   updatedConfig.daily_goal_ml = newGoalValue
   // Leave scale_factor and tare_weight_grams unchanged!
   ```

3. **Write the complete config back**
   ```swift
   writeBottleConfig(updatedConfig)
   ```

#### Alternative: Use Separate Commands

For settings that don't involve calibration (like daily goal), consider using separate BLE commands instead of the BottleConfig characteristic:

- `BLE_CMD_SET_DAILY_GOAL` (if implemented) - only updates goal, doesn't touch calibration
- This avoids the read-modify-write pattern entirely

#### Validation

Even with read-modify-write, validate before sending:
```swift
// Sanity check - scale_factor should be in valid range
if updatedConfig.scale_factor < 100 || updatedConfig.scale_factor > 800 {
    // Something is wrong - don't send this config
    print("WARNING: Invalid scale_factor, aborting config update")
    return
}
```

#### Firmware Protection (Now Implemented)

As of Issue #84, the firmware now validates incoming `scale_factor` values:
- Values outside 100-800 ADC/g range are rejected
- The firmware will NOT save corrupt calibration data
- However, the iOS app should still implement proper read-modify-write to avoid rejected writes

---

## Summary

**Root cause**: While implementing the goal weight feature in the iOS app (on another branch), the app sent a `BLE_BottleConfig` struct to update `daily_goal_ml`. However, the struct also includes `scale_factor`, which was likely uninitialized or set to the default `1.0`. The firmware accepted this without validation and corrupted the calibration.

**The fix has two parts:**
1. **Break the circular dependency** (Phase 2-3) - Use accelerometer-only stability during calibration, enabling self-recovery
2. **Prevent future corruption** (Phase 1, 4-5) - Validate scale_factor on load and on all write paths (BLE, serial)

After the fix, recovery is automatic - just trigger calibration with the inverted hold gesture. No serial commands or mode switching required.

**Expected range for scale_factor**: 200-600 ADC/g (based on NAU7802 at 128 gain with typical load cells)
