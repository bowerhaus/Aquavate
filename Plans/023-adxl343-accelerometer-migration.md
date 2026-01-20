# Migration Plan: LIS3DH to ADXL343 Accelerometer

**Status:** ✅ COMPLETE (2026-01-20)

## Overview
Replace the LIS3DH 3-axis accelerometer with ADXL343. Wake-on-tilt functionality achieved using AC-coupled motion detection instead of orientation-based interrupts due to hardware differences between sensors.

## Final Implementation Summary

### Wake-on-Tilt Approach
**Changed from original plan:** ADXL343 lacks the simple axis high/low threshold interrupts that LIS3DH had. Instead of orientation-based detection (Y-axis threshold), we use **AC-coupled motion detection** with aggressive filtering.

**Final Configuration:**
- **Mode:** Activity interrupt, AC-coupled (detects motion changes, not static orientation)
- **Threshold:** 3.0g (0x30 register value)
- **Duration:** 1.6 seconds (0x14 register value, 20 samples @ 12.5Hz)
- **Axes:** X, Y, Z all enabled
- **Result:** Ignores table vibrations and train movement, requires deliberate shake/pick-up for ~2 seconds

### Key Differences from Original Plan

| Aspect | Original Plan (LIS3DH-like) | Final Implementation |
|--------|---------------------------|----------------------|
| Wake Method | Y-axis < 0.81g (orientation) | Motion > 3.0g for 1.6sec |
| Interrupt Type | Inactivity (DC-coupled) | Activity (AC-coupled) |
| Threshold | 0x0D (0.81g) | 0x30 (3.0g) |
| Duration Filter | None | 1.6 seconds required |
| Environmental Immunity | Excellent (orientation-based) | Good (requires aggressive thresholds) |
| User Experience | Instant wake on tilt | Requires deliberate shake (~2 sec) |

### Why the Change Was Necessary

1. **ADXL343 hardware limitation:** No simple axis high/low threshold interrupts like LIS3DH's ZLIE (Z-Low Interrupt Enable)
2. **DC-coupled activity doesn't work for orientation:** Compares absolute acceleration values, triggers constantly when Y=±1g (upright or inverted)
3. **AC-coupled motion detection required:** Only detects acceleration *changes*, immune to static gravity
4. **Trade-off accepted:** Battery life and vibration immunity prioritized over instant wake

## Implementation Steps Completed

### 1. Library Dependency Change ✅
**File:** [firmware/platformio.ini](../firmware/platformio.ini)

Changed from:
```ini
adafruit/Adafruit LIS3DH@^1.2.0
```

To:
```ini
adafruit/Adafruit ADXL343@^1.6.4
```

### 2. I2C Address Constant ✅
**File:** [firmware/include/aquavate.h](../firmware/include/aquavate.h)

Changed from:
```cpp
#define I2C_ADDR_LIS3DH     0x18
```

To:
```cpp
#define I2C_ADDR_ADXL343    0x53  // Default with SDO=LOW
```

### 3. Pin Configuration ✅
**File:** [firmware/src/config/pins_adafruit.h](../firmware/src/config/pins_adafruit.h)

```cpp
#define PIN_ACCEL_INT       27  // GPIO 27 (A10 on Feather silkscreen)
```

**Note:** Hardware physically wired to GPIO 27. Initial debugging required fixing mismatch (code had GPIO 14).

### 4. Header and Object Changes ✅
**File:** [firmware/src/main.cpp](../firmware/src/main.cpp)

```cpp
#include <Adafruit_ADXL343.h>

Adafruit_ADXL343 adxl = Adafruit_ADXL343(12345);  // 12345 = sensor ID
bool adxlReady = false;  // Renamed from lisReady
```

### 5. Initialization ✅
**File:** [firmware/src/main.cpp](../firmware/src/main.cpp)

```cpp
if (adxl.begin(I2C_ADDR_ADXL343)) {
    Serial.println("ADXL343 found!");
    adxl.setRange(ADXL343_RANGE_2_G);
    adxl.setDataRate(ADXL343_DATARATE_12_5_HZ);
    adxlReady = true;
    configureADXL343Interrupt();
} else {
    Serial.println("ADXL343 not found!");
    adxlReady = false;
}
```

### 6. Interrupt Configuration Function ✅
**File:** [firmware/src/main.cpp](../firmware/src/main.cpp)

**Final implementation (after extensive tuning):**

```cpp
void configureADXL343Interrupt() {
    Serial.println("\n=== ADXL343 Interrupt Configuration ===");

    pinMode(PIN_ACCEL_INT, INPUT_PULLDOWN);  // Active-high interrupt

    // ADXL343 Register Definitions
    const uint8_t THRESH_ACT = 0x1C;        // Activity threshold
    const uint8_t TIME_ACT = 0x22;          // Activity duration
    const uint8_t ACT_INACT_CTL = 0x27;     // Axis enable for activity/inactivity
    const uint8_t POWER_CTL = 0x2D;         // Power control
    const uint8_t INT_ENABLE = 0x2E;        // Interrupt enable
    const uint8_t INT_MAP = 0x2F;           // Interrupt mapping
    const uint8_t DATA_FORMAT = 0x31;       // Data format

    // Step 1: Configure data format (±2g range)
    writeAccelReg(DATA_FORMAT, 0x00);       // ±2g, 13-bit resolution, right-justified

    // Step 2: Set activity threshold - EXTREMELY HIGH
    // Threshold = 3.0g = 3000 mg (only very strong deliberate motion)
    // Scale = 62.5 mg/LSB
    // Value = 3000 / 62.5 = 48 (0x30)
    writeAccelReg(THRESH_ACT, 0x30);        // ~3.0g activity threshold

    // Step 3: Set activity duration - require very sustained motion
    // At 12.5 Hz sample rate: 1 LSB = 1/12.5 = 80ms
    // Value = 20 → 1600ms (~1.6 seconds) of sustained motion required
    writeAccelReg(TIME_ACT, 0x14);          // 20 samples = ~1600ms

    // Step 4: Enable all axes for activity detection (AC-coupled)
    // Bits: 7=ACT_acdc(1=AC), 6-4=ACT_X/Y/Z (111=all axes)
    writeAccelReg(ACT_INACT_CTL, 0xF0);     // All axes, AC-coupled

    // Step 5: Enable measurement mode
    writeAccelReg(POWER_CTL, 0x08);         // Measurement mode (bit 3)

    // Step 6: Enable activity interrupt
    writeAccelReg(INT_ENABLE, 0x10);        // Activity interrupt (bit 4)

    // Step 7: Route activity to INT1 pin
    writeAccelReg(INT_MAP, 0x00);           // All interrupts to INT1

    Serial.println("Wake condition: Very strong sustained motion (>3.0g for >1.6sec, AC-coupled)");
}
```

### 7. Interrupt Clearing ✅
**File:** [firmware/src/main.cpp](../firmware/src/main.cpp)

```cpp
// ADXL343 auto-clears interrupt when motion stops
// Read INT_SOURCE for diagnostics only
uint8_t int_source = readAccelReg(0x30);  // INT_SOURCE register
```

### 8. Gestures Module Updates ✅
**File:** [firmware/src/gestures.cpp](../firmware/src/gestures.cpp)

```cpp
static Adafruit_ADXL343* g_adxl = nullptr;  // Changed from g_lis

void gesturesInit(Adafruit_ADXL343& adxl) {
    g_adxl = &adxl;
    // ... rest unchanged
}

static float rawToGs(int16_t raw) {
    // ADXL343 at ±2g: 13-bit resolution, 256 LSB/g (4 mg/LSB)
    return raw / 256.0f;
}

// In gesturesUpdate():
int16_t x, y, z;
g_adxl->getXYZ(x, y, z);
g_current_x = rawToGs(x);
g_current_y = rawToGs(y);
g_current_z = rawToGs(z);
```

**Orientation preserved:** Y=-1g when upright (original LIS3DH orientation)

### 9. Header File Updates ✅
**File:** [firmware/include/gestures.h](../firmware/include/gestures.h)

```cpp
class Adafruit_ADXL343;  // Forward declaration

void gesturesInit(Adafruit_ADXL343& adxl);
void gesturesInit(Adafruit_ADXL343& adxl, const GestureConfig& config);
```

## Threshold Tuning Journey

Multiple iterations were required to find acceptable sensitivity:

| Iteration | Threshold | Duration | Result |
|-----------|-----------|----------|--------|
| 1 (Original Plan) | 0.31g DC | 0ms | Too sensitive - false wakes constantly |
| 2 (AC-coupled) | 0.31g AC | 0ms | Still too sensitive - table taps wake |
| 3 | 0.5g AC | 240ms | Much better but train vibrations still wake |
| 4 | 1.0g AC | 400ms | Better but still too sensitive |
| 5 | 1.5g AC | 800ms | Good improvement |
| 6 (Final) | 3.0g AC | 1600ms | Acceptable - ignores vibrations, requires deliberate shake |

## Verification Results

### Phase 1: Basic Sensor Communication ✅
- ✅ ADXL343 found and initialized
- ✅ Accelerometer readings accurate
- ✅ Y-axis reads ~-1.0g when upright

### Phase 2: Interrupt Function ✅
- ✅ Interrupt pin LOW when stationary
- ✅ Interrupt triggers on sustained strong motion (>3g for >1.6sec)
- ✅ Auto-clears when motion stops
- ✅ Immune to table vibrations and train movement

### Phase 3: Deep Sleep Wake ✅
- ✅ Device enters deep sleep successfully
- ✅ Wakes on deliberate shake/pick-up motion
- ✅ Multiple wake/sleep cycles working
- ✅ Wake logging shows EXT0 interrupt trigger

### Phase 4: Gesture Detection ✅
- ✅ GESTURE_UPRIGHT_STABLE detection working
- ✅ GESTURE_INVERTED_HOLD working (Y > +0.7g threshold)
- ✅ Drink detection unaffected
- ✅ Variance calculations stable with 13-bit resolution

### Phase 5: Power Consumption ✅
- ✅ Deep sleep current <100 μA (ADXL343 standby ~0.1 μA)
- ✅ Battery life projections unchanged

## Critical Files Modified

- [firmware/platformio.ini](../firmware/platformio.ini) - Library dependency
- [firmware/src/config/pins_adafruit.h](../firmware/src/config/pins_adafruit.h) - PIN_ACCEL_INT=27
- [firmware/src/main.cpp](../firmware/src/main.cpp) - Sensor init, AC-coupled interrupt config (3g, 1.6sec)
- [firmware/src/gestures.cpp](../firmware/src/gestures.cpp) - ADXL343 API calls, Y=-1g orientation
- [firmware/include/gestures.h](../firmware/include/gestures.h) - Function signatures
- [firmware/include/aquavate.h](../firmware/include/aquavate.h) - I2C address constant

## Known Differences from LIS3DH

1. **Wake method:** Motion-based (AC-coupled) instead of orientation-based (DC-coupled)
   - **Impact:** User must shake bottle deliberately for ~2 seconds to wake
   - **Benefit:** Immune to environmental vibrations

2. **Non-latching interrupts:** ADXL343 auto-clears when condition ends
   - **Impact:** No manual INT_SOURCE read required
   - **Benefit:** Simpler code

3. **Lower resolution:** 13-bit (256 LSB/g) vs 16-bit (~16000 LSB/g)
   - **Impact:** Minimal - gesture detection still accurate
   - **Benefit:** None, but acceptable trade-off

4. **12.5 Hz vs 10 Hz:** Slightly faster sampling rate
   - **Impact:** Minimal power impact
   - **Benefit:** None, but acceptable (no 10Hz option available)

## Key Learnings

1. **Hardware capabilities matter:** ADXL343 lacks LIS3DH's axis-specific threshold interrupts, requiring completely different wake strategy

2. **DC-coupled activity is not orientation detection:** Tests confirmed DC-coupled mode compares absolute values, not deviations from gravity

3. **AC-coupled requires aggressive filtering:** Environmental vibration filtering requires very high thresholds (3g) and long durations (1.6sec)

4. **User experience trade-off:** Battery life and robustness prioritized over instant wake - acceptable for real-world use

5. **Iterative tuning essential:** Six iterations required to find optimal threshold/duration balance

## Future Considerations

If instant wake on gentle tilt is required in the future, consider:

1. **Different accelerometer:** One with axis-specific high/low threshold interrupts (like LIS3DH)
2. **External comparator:** Hardware circuit to detect orientation changes
3. **Firmware-based wake:** Timer wake every 30-60 seconds to check orientation (rejected due to drink detection latency)

For current use case (smart water bottle), motion-based wake with 3g threshold is acceptable and provides excellent battery life.
