# Standalone Calibration Mode - Implementation Plan

## Context Summary

**User Requirements:**
- End-user setup without precision reference weights (just household items)
- Moderate accuracy target: ±15-30ml
- Simple interaction with minimal steps
- Calibration persists until explicitly reset by user
- Should work before iOS app is available

**Physical Design (CRITICAL):**
- **Bottle is permanently attached to the puck** via friction-fit silicone sleeve
- Bottle sits on top plate connected to load cell (cantilever configuration)
- **Bottle capacity: 830ml** (user's specific bottle)
- Bottle cannot be removed during calibration
- Load cell measures total weight: bottle + water

**Current Hardware Capabilities:**
- **Load Cell (NAU7802)**: 24-bit ADC, 128x gain, 10 SPS reading rate
- **Accelerometer (LIS3DH)**: 3-axis, currently configured for wake-on-tilt (>80° angle)
- **E-Paper Display**: 250×122px monochrome, SSD1680 controller
  - Full refresh only (~1-2 seconds per update)
  - NO partial updates or fast refresh modes
  - Best for static content with minimal updates
- **Battery Monitoring**: Can read voltage and display percentage
- **No Physical Buttons**: Only sensor-based input mechanisms available

**Current Firmware State:**
- Basic sensor initialization working ✅
- Wake-on-tilt interrupt configured ✅
- Display rendering functional ✅
- **Missing**: No NVS storage, no calibration logic, no gesture detection

---

## Calibration Options

### Option 1: Two-Point Calibration (Empty + Full Bottle) ⭐ RECOMMENDED
**Concept:** Use the permanently-attached 830ml bottle as both calibration references

**How It Works:**
1. User enters calibration mode via gesture (hold bottle inverted for 5 seconds)
2. Display shows: "Empty bottle completely"
3. User empties all water from bottle (bottle stays attached to puck)
4. User returns bottle to upright position
5. Display shows: "Measuring empty... hold still"
6. System takes stable reading (10 samples, 10 seconds) → `empty_adc`
7. User confirms with sideways tilt gesture
8. Display shows: "Fill to 830ml line"
9. User fills bottle to the 830ml capacity mark
10. Display shows: "Measuring full... hold still"
11. System takes stable reading → `full_adc`
12. User confirms with sideways tilt
13. Calculate: `scale_factor = (full_adc - empty_adc) / 830.0` (grams of water)
14. Store both `empty_adc` and `scale_factor` to NVS

**Gesture Sequence:**
```
User Action                Display Feedback                     Accelerometer State
----------------------------------------------------------------------------------
Hold inverted 5s      -->  "Calibration Mode"              -->  Z-axis < -0.8g for 5s
                           (full refresh)
User empties bottle   -->  "Empty bottle completely"       -->  (user action, bottle attached)
                           "Hold still when ready"
                           (full refresh)
Return upright        -->  "Measuring empty..."            -->  Z-axis > 0.9g, stable
Wait 10 seconds       -->  (no display updates during      -->  (background sampling)
                            measurement - e-paper limit)
Measurement done      -->  "Empty: 12345 ADC"              -->  (after 10s stability check)
                           "Tilt sideways to confirm"
                           (full refresh)
Tilt sideways         -->  "Empty recorded!"               -->  |X or Y| > 0.5g spike
                           "Fill to 830ml line"
                           (full refresh)
User fills bottle     -->  (user fills, bottle attached)
Return upright        -->  "Measuring full..."             -->  Z-axis > 0.9g, stable
Wait 10 seconds       -->  (no display updates during      -->  (background sampling)
                            measurement - e-paper limit)
Measurement done      -->  "Full: 54321 ADC"               -->  (after 10s stability check)
                           "Tilt sideways to confirm"
                           (full refresh)
Tilt sideways         -->  "Calibration complete!"         -->  |X or Y| > 0.5g
                           "Scale: 50.6 ct/g"
                           (full refresh)
```

**Note on E-Paper Display Limitations:**
- SSD1680 controller does NOT support partial updates
- Full refresh takes ~1-2 seconds per update
- Progress bars/animations are not practical (would require 10+ refreshes = 10+ seconds)
- **Solution:** Static messages during measurement, update only at state changes
- Battery efficient: ~5 total refreshes for entire calibration process

**Pros:**
✅ **Perfect accuracy reference**: 830ml water = 830g (water density = 1g/ml)
✅ **No assumptions needed**: Bottle weight cancels out in the difference calculation
✅ **No external weights required**: Uses the attached bottle itself
✅ **Simple user interaction**: Just empty, confirm, fill, confirm
✅ **Verifiable**: User knows exactly what the reference weight is (830g)
✅ **Repeatable**: Can re-run calibration anytime to verify/improve

**Cons:**
⚠️ Requires user to empty and fill bottle during calibration (~2-3 minutes total)
⚠️ User must fill exactly to 830ml mark (most bottles have capacity markings)
⚠️ Requires ~20-25 seconds of measurement time (2x 10-second stable readings)
⚠️ If bottle doesn't have 830ml marking, user must measure with measuring cup

**Accuracy Estimate:** ±10-15ml (best possible without external reference weights)

**Implementation Complexity:** Medium
- Requires: Gesture detection, two-step wizard UI, NVS storage, stability monitoring
- Estimated development: 5-7 hours

**Why This is Perfect for Your Design:**
- The bottle is **already attached** - no removal needed
- 830ml is a **known, precise reference weight** (830g of water)
- Bottle weight (empty) is automatically factored out
- This is essentially the same as using precision calibration weights!

---

### Option 2: Single-Point Calibration (Default Scale Factor + Tare)
**Concept:** Ship with a default scale factor from NAU7802 datasheet, calibrate only the tare (empty bottle weight)

**How It Works:**
1. Firmware ships with typical scale factor for NAU7802 + load cell combination
2. User enters calibration mode (hold inverted 5s)
3. Display shows: "Empty bottle completely"
4. User empties bottle, returns upright
5. System measures empty bottle ADC reading → `tare_adc`
6. User confirms with sideways tilt
7. Store `tare_adc` to NVS
8. During normal operation: `current_water_ml = (current_adc - tare_adc) / default_scale_factor`

**Gesture Sequence:**
```
User Action                Display Feedback                     Accelerometer State
----------------------------------------------------------------------------------
Hold inverted 5s      -->  "Calibration Mode"              -->  Z-axis < -0.8g for 5s
                           "Empty bottle completely"
User empties bottle   -->  (user action)
Return upright        -->  "Measuring empty..."            -->  Z-axis > 0.9g, stable
Wait 5 seconds        -->  "Empty bottle tared"
                           "Tilt to confirm"
Tilt sideways         -->  "Ready to track!"               -->  |X or Y| > 0.5g
```

**Pros:**
✅ Fastest calibration (single 5-second measurement)
✅ Simplest user interaction (just empty and confirm)
✅ Can be repeated anytime
✅ Good for quick setup or re-taring after bottle swap

**Cons:**
⚠️ Depends on accuracy of default scale factor (±15-25% typical variance)
⚠️ Requires manufacturer lab testing to determine good default scale factor
⚠️ NAU7802 + load cell characteristics may vary between units
⚠️ Less accurate than two-point calibration

**Accuracy Estimate:** ±25-40ml (depends on default scale factor quality)

**Implementation Complexity:** Low
- Requires: Gesture detection, single-screen UI, NVS storage
- Estimated development: 3-4 hours
- **BUT**: Requires upfront lab work to determine default scale factor

**Use Case:** Good as a "quick tare" feature **in addition to** two-point calibration

---

### Option 3: Partial Fill Calibration (Flexible Volume)
**Concept:** Allow user to calibrate with any known water volume (not just full bottle)

**How It Works:**
1. User enters calibration mode (hold inverted 5s)
2. Display shows: "Empty bottle completely"
3. User empties bottle → measure `empty_adc`
4. Display shows: "Fill with known volume"
5. Display cycles through common volumes: "200ml?", "500ml?", "830ml?"
6. User inverts briefly to cycle, tilts sideways to confirm volume
7. User fills bottle to selected volume
8. System measures `filled_adc`
9. Calculate: `scale_factor = (filled_adc - empty_adc) / selected_volume_ml`
10. Store to NVS

**Gesture Sequence:**
```
User Action                Display Feedback                     Accelerometer State
----------------------------------------------------------------------------------
Hold inverted 5s      -->  "Calibration Mode"              -->  Z-axis < -0.8g for 5s
                           "Empty bottle completely"
User empties bottle   -->  (user action)
Return upright        -->  "Measuring empty..."            -->  Z-axis > 0.9g, stable
Wait 5 seconds        -->  "Empty recorded!"
Tilt sideways         -->  "Select fill volume:"
                           "200ml?"
Invert briefly        -->  "500ml?"                        -->  Z < -0.8g (cycle)
Invert briefly        -->  "830ml?"                        -->  Z < -0.8g (cycle)
Tilt sideways         -->  "Fill to 500ml"                 -->  |X or Y| > 0.5g
User fills bottle     -->  (user fills to 500ml)
Return upright        -->  "Measuring full..."             -->  Z-axis > 0.9g, stable
Wait 10 seconds       -->  "Calibration complete!"
```

**Pros:**
✅ Flexible - works with any measurable volume
✅ User doesn't need to fill completely (useful if bottle hard to fill)
✅ Can use measuring cup or kitchen scale to verify volume
✅ Good accuracy if user measures carefully

**Cons:**
⚠️ More complex UI (volume selection adds steps)
⚠️ User must accurately measure water volume (source of error)
⚠️ Longer calibration process (~40-50 seconds)
⚠️ More opportunities for user error

**Accuracy Estimate:** ±15-25ml (depends on user's volume measurement accuracy)

**Implementation Complexity:** Medium-High
- Requires: Gesture detection, multi-screen wizard, volume selection state machine, NVS storage
- Estimated development: 6-8 hours

**Use Case:** Good if bottle doesn't have volume markings, or user wants to use measuring cup

---

### Option 4: Automatic Recalibration (Zero Offset Only)
**Concept:** Assume user will calibrate fully via iOS app eventually, just track zero offset drift

**How It Works:**
1. Firmware ships with no calibration (displays raw ADC values only)
2. User enters "zero tare" mode (hold inverted 5s)
3. Display shows: "Empty bottle completely"
4. System measures `zero_adc` (baseline for drift tracking)
5. System displays: "Uncalibrated - use iOS app"
6. Device tracks relative changes from zero, but doesn't show accurate ml values
7. When iOS app connects, it performs full two-point calibration

**Pros:**
✅ Simplest firmware (minimal standalone logic)
✅ Prevents user confusion from inaccurate readings
✅ Forces users toward more accurate iOS calibration
✅ Still allows basic "relative change" tracking

**Cons:**
⚠️ Not truly "standalone" - requires iOS app for full functionality
⚠️ Doesn't meet user requirement for standalone calibration
⚠️ User can't see accurate ml readings before iOS app setup
⚠️ Poor user experience for standalone operation

**Accuracy Estimate:** N/A (no ml conversion until iOS calibration)

**Implementation Complexity:** Very Low
- Requires: Basic gesture detection, simple UI, NVS storage
- Estimated development: 2-3 hours

**Use Case:** Only if you want to simplify standalone mode and prioritize iOS app calibration

---

## Comparison Matrix

| Aspect | Option 1: Two-Point (Empty+Full) | Option 2: Default + Tare | Option 3: Flexible Volume | Option 4: Zero Only |
|--------|----------------------------------|--------------------------|--------------------------|---------------------|
| **Accuracy** | ±10-15ml ⭐ | ±25-40ml | ±15-25ml | N/A (uncalibrated) |
| **User Time** | ~25 seconds | ~10 seconds ⭐ | ~40 seconds | ~10 seconds |
| **Simplicity** | Medium ⭐ | High ⭐ | Low | High |
| **Assumptions** | None! ✅ | Default scale factor | User measures volume | Everything |
| **Reference Weights Needed** | None (uses 830g water) ✅ | None ✅ | None (user measures) ✅ | None ✅ |
| **Manufacturing Setup** | None ✅ | Lab testing required ⚠️ | None ✅ | None ✅ |
| **Repeatability** | High ⭐ | Medium | High | Low |
| **Standalone Functional** | Yes ⭐ | Yes | Yes | No ⚠️ |
| **Volume Flexibility** | Fixed (830ml) | N/A | Any volume ⭐ | N/A |

---

## Recommended Approach

**🏆 Primary Recommendation: Option 1 (Two-Point: Empty + Full 830ml Bottle)**

**Rationale:**
- **Best accuracy** (±10-15ml) - meets your ±15-30ml target perfectly ✅
- **Zero assumptions** - 830ml water is a precise known weight (830g)
- **Bottle weight cancels out** mathematically - no guessing needed
- **Perfectly suited to your hardware** - bottle is permanently attached
- **Simple user interaction** - just empty, confirm, fill to 830ml mark, confirm
- **No external equipment needed** - bottle itself is the reference
- **Repeatable** - user can re-run anytime to verify/refine
- **No manufacturing overhead** - works out of the box

**This is essentially professional-grade calibration using your attached bottle as a precision weight!**

---

## Alternative: Hybrid Approach (Option 1 + Option 2)

**Best of Both Worlds:**

1. **Full Calibration (Option 1)** - Primary method
   - User runs this once for best accuracy
   - Empty → measure → fill to 830ml → measure
   - Store `empty_adc`, `full_adc`, `scale_factor`

2. **Quick Re-tare (Option 2)** - Secondary feature
   - If user suspects drift or wants to verify zero
   - Just empty bottle and re-measure `empty_adc`
   - Uses existing `scale_factor` from full calibration
   - Faster for periodic checks

**Gesture Triggers:**
- Hold inverted 5 seconds → Full calibration (Option 1)
- Hold inverted + tilt sideways immediately → Quick re-tare only (Option 2)

**Benefits:**
✅ Best accuracy when user needs it (full calibration)
✅ Quick maintenance when user wants to re-zero
✅ Covers both initial setup and ongoing maintenance
✅ Users appreciate having both options

---

## Implementation Details for Option 1 (Two-Point Empty+Full Calibration)

### Hardware Requirements
✅ All requirements already met:
- LIS3DH accelerometer (gesture detection)
- NAU7802 load cell (weight measurement)
- E-Paper display (user feedback)
- No additional hardware needed

### Firmware Components to Implement

#### 1. Gesture Detection Module
**File:** `firmware/src/gestures.cpp`

```cpp
// Gesture states
enum GestureType {
    GESTURE_NONE,
    GESTURE_INVERTED_HOLD,    // Z < -0.8g for 5s (calibration trigger)
    GESTURE_UPRIGHT_STABLE,   // Z > 0.9g, low variance (bottle placement)
    GESTURE_SIDEWAYS_TILT,    // |X| or |Y| > 0.5g (confirmation)
};

// Functions to implement
GestureType detectGesture(Adafruit_LIS3DH& lis);
bool isStable(float variance_threshold);
float calculateVariance(int32_t* samples, int count);
```

**Detection Logic:**
- Sample accelerometer at 10Hz (current rate)
- Track Z-axis for inverted/upright detection
- Track X/Y axes for sideways tilt confirmation
- Calculate variance over 1-second window for stability

#### 2. Calibration State Machine
**File:** `firmware/src/calibration.cpp`

```cpp
enum CalibrationState {
    CAL_IDLE,
    CAL_TRIGGERED,        // User held inverted
    CAL_WAIT_EMPTY,       // Waiting for empty bottle placement
    CAL_MEASURE_EMPTY,    // Measuring empty bottle (10s)
    CAL_CONFIRM_EMPTY,    // Waiting for sideways tilt
    CAL_WAIT_FULL,        // Waiting for full bottle placement
    CAL_MEASURE_FULL,     // Measuring full bottle (10s)
    CAL_CONFIRM_FULL,     // Waiting for sideways tilt
    CAL_COMPLETE,         // Show success message
};

// State machine handler
void updateCalibrationStateMachine(GestureType gesture, int32_t load_reading);
```

#### 3. Weight Measurement with Stability Check
**File:** `firmware/src/weight.cpp`

```cpp
struct WeightMeasurement {
    int32_t raw_adc;       // Raw ADC reading
    float variance;        // Sample variance
    bool stable;           // Stability flag
    int sample_count;      // Number of samples averaged
};

// Take stable weight reading (10 samples, outlier removal)
WeightMeasurement measureStableWeight(Adafruit_NAU7802& nau, int duration_seconds);
```

**Algorithm:**
- Sample at 10 SPS for specified duration
- Remove outliers (±2 standard deviations)
- Calculate mean and variance
- Mark stable if variance < threshold

#### 4. Calibration Data Storage (NVS)
**File:** `firmware/src/storage.cpp`

```cpp
#include <Preferences.h>

struct CalibrationData {
    float scale_factor;         // ADC counts per gram
    int32_t empty_bottle_adc;   // ADC reading of empty bottle
    int32_t full_bottle_adc;    // ADC reading of full bottle (500ml)
    uint32_t calibration_timestamp; // Unix timestamp
    uint8_t calibration_valid;  // 0=invalid, 1=valid
};

// NVS functions
bool saveCalibration(const CalibrationData& cal);
bool loadCalibration(CalibrationData& cal);
void resetCalibration();
```

**NVS Namespace:** `"aquavate"`
**Keys:**
- `scale_factor` (float)
- `empty_adc` (int32)
- `full_adc` (int32)
- `cal_timestamp` (uint32)
- `cal_valid` (uint8)

#### 5. Calibration UI Screens
**File:** `firmware/src/ui_calibration.cpp`

**E-Paper Display Strategy:**
- SSD1680 controller does NOT support partial updates (full refresh only)
- Each `display.display()` call takes ~1-2 seconds
- Limit updates to state changes only (no progress animations)
- Total calibration: ~5-6 full refreshes (~6-10 seconds of display time)

```cpp
// Display screens for calibration wizard (static updates only)
void showCalibrationStart();          // "Calibration Mode" + "Empty bottle completely"
void showMeasuringEmpty();            // "Measuring empty..." (static during 10s)
void showEmptyConfirm(int32_t adc);   // "Empty: XXXXX ADC" + "Tilt to confirm"
void showFillBottle();                // "Fill to 830ml line" + "Hold still when ready"
void showMeasuringFull();             // "Measuring full..." (static during 10s)
void showFullConfirm(int32_t adc);    // "Full: XXXXX ADC" + "Tilt to confirm"
void showCalibrationComplete(float scale_factor); // "Complete!" + "Scale: XX.X ct/g"
void showCalibrationError(const char* msg);       // "Error: <message>" + "Try again"
```

**UI Flow:**
```cpp
// Each function follows this pattern:
void showCalibrationStart() {
    display.clearBuffer();
    display.setTextSize(2);
    display.setTextColor(EPD_BLACK);

    // Center title
    display.setCursor(20, 20);
    display.print("Calibration");

    // Instructions
    display.setTextSize(1);
    display.setCursor(10, 50);
    display.print("Empty bottle completely");
    display.setCursor(10, 70);
    display.print("Hold still when ready");

    display.display();  // Full refresh (~1-2 seconds)
}
```

**Display Update Points (5-6 total):**
1. Calibration start → Empty instructions
2. Begin empty measurement → "Measuring..."
3. Empty measurement done → Show ADC + confirm prompt
4. Empty confirmed → Fill instructions
5. Begin full measurement → "Measuring..."
6. Full measurement done → Show ADC + confirm prompt
7. Calibration complete → Show scale factor

#### 6. Scale Factor Calculation
**File:** `firmware/src/calibration.cpp`

```cpp
float calculateScaleFactor(int32_t empty_adc, int32_t full_adc, float water_volume_ml) {
    // For 830ml bottle:
    // water_volume_ml = 830ml → 830g (water density = 1g/ml)
    //
    // Bottle weight completely cancels out:
    // - empty_adc = bottle_weight_grams * unknown_scale + offset
    // - full_adc = (bottle_weight_grams + 830g) * unknown_scale + offset
    // - full_adc - empty_adc = 830g * unknown_scale
    // - Therefore: scale_factor = (full_adc - empty_adc) / 830g

    int32_t adc_diff = full_adc - empty_adc;
    float scale_factor = adc_diff / water_volume_ml;  // ADC counts per gram
    return scale_factor;
}

// For runtime weight measurement:
float getWaterWeightGrams(int32_t current_adc, int32_t empty_adc, float scale_factor) {
    int32_t adc_from_empty = current_adc - empty_adc;
    float water_grams = adc_from_empty / scale_factor;
    return water_grams;  // grams = ml for water
}
```

**Key Insight:** Bottle weight (empty) **completely cancels out** when taking the ADC difference!
- We never need to know the bottle weight
- We only need to know the water volume (830ml = 830g)
- This makes the calibration highly accurate with zero assumptions

#### 7. Integration into main.cpp

**setup():**
- Load calibration from NVS
- Check if valid
- If not valid, display "Calibration needed" message

**loop():**
- If not calibrated:
  - Monitor for inverted-hold gesture
  - Enter calibration mode when detected
- If calibrated:
  - Normal weight tracking mode
  - Convert ADC readings to grams using scale factor
  - Detect drinks, update daily total

---

## Critical Files to Modify

1. **firmware/src/main.cpp** - Add calibration mode logic, state machine integration
2. **firmware/src/config.h** - Add calibration constants (timeouts, thresholds, variance limits)
3. **firmware/include/aquavate.h** - Add calibration data structure definition
4. **firmware/platformio.ini** - Verify Preferences library is included (for NVS)

### New Files to Create

5. **firmware/src/gestures.cpp** + **firmware/include/gestures.h** - Gesture detection
6. **firmware/src/calibration.cpp** + **firmware/include/calibration.h** - Calibration logic
7. **firmware/src/weight.cpp** + **firmware/include/weight.h** - Weight measurement utilities
8. **firmware/src/storage.cpp** + **firmware/include/storage.h** - NVS persistence
9. **firmware/src/ui_calibration.cpp** + **firmware/include/ui_calibration.h** - Calibration UI screens

---

## Verification & Testing Plan

### Unit Tests (via Serial Monitor)
1. **Gesture Detection:**
   - Hold bottle inverted 5s → should trigger calibration mode
   - Place bottle upright → should detect stable vertical orientation
   - Tilt sideways → should detect confirmation gesture

2. **Weight Measurement:**
   - Place known weights (or bottles) → verify ADC readings are consistent
   - Check variance calculation → should be low when stable, high when moving
   - Verify outlier removal → place object, jiggle it, verify readings stabilize

3. **NVS Storage:**
   - Save calibration → power cycle → verify data persists
   - Reset calibration → verify data cleared

### End-to-End Calibration Test (830ml Bottle)
1. Hold bottle inverted 5 seconds
2. Verify display shows "Calibration Mode" → "Empty bottle completely"
3. Empty the 830ml bottle completely (pour out all water)
4. Return bottle to upright position
5. Wait 10 seconds for measurement (display shows progress)
6. Tilt sideways to confirm empty reading
7. Verify display shows "Empty recorded!" → "Fill to 830ml line"
8. Fill bottle to the 830ml capacity mark
9. Wait 10 seconds for measurement (display shows progress)
10. Tilt sideways to confirm full reading
11. Verify display shows "Calibration complete! Scale: X.XX"
12. Power cycle device
13. Verify calibration persists (check via serial log for stored scale_factor)

### Accuracy Verification (830ml Bottle)
1. After calibration, fill bottle with known water amounts:
   - 200ml, 400ml, 600ml, 830ml (full)
2. Measure displayed water level via serial output or display
3. Compare measured ml to actual ml
4. Target: ±10-15ml accuracy (should easily meet ±15-30ml requirement)

---

## Development Effort Estimate

| Task | Estimated Time |
|------|----------------|
| Gesture detection module | 2 hours |
| Weight measurement utilities | 1.5 hours |
| NVS storage implementation | 1 hour |
| Calibration state machine | 2 hours |
| Calibration UI screens | 2 hours |
| Integration into main.cpp | 1.5 hours |
| Testing & debugging | 3 hours |
| **Total** | **13 hours** |

---

## Summary & Key Advantages

### Why Option 1 (Two-Point Empty+Full 830ml) is Perfect for This Design:

1. **The Bottle is Permanently Attached**
   - No need to remove/place bottle
   - User just empties and fills while bottle stays on puck
   - Eliminates "place bottle carefully" errors

2. **830ml Water is a Perfect Reference Weight**
   - 830ml water = 830g (precise, known weight)
   - No external reference weights needed
   - No assumptions required
   - Anyone can verify: 1ml water = 1g

3. **Bottle Weight Mathematically Cancels Out**
   ```
   empty_adc = (bottle_weight + 0ml_water) × scale
   full_adc  = (bottle_weight + 830ml_water) × scale
   difference = (830g) × scale
   → scale_factor = difference / 830g
   ```
   - We never need to know bottle weight!
   - Works for any bottle weight (50g, 100g, 150g - doesn't matter)
   - Eliminates a major source of calibration error

4. **Meets All User Requirements**
   - ✅ No precision reference weights needed
   - ✅ ±10-15ml accuracy (better than ±15-30ml target)
   - ✅ Simple gesture-based interaction
   - ✅ Persistent storage in NVS
   - ✅ Works standalone before iOS app available
   - ✅ No manufacturing overhead

5. **User-Friendly Process**
   - Clear visual prompts on e-paper display
   - Only two confirmation gestures (sideways tilts)
   - Takes ~25 seconds total
   - Can be repeated anytime for verification

### Alternative Options Comparison

| Why Not... | Reason |
|-----------|--------|
| **Option 2 (Default + Tare)** | Requires lab testing for default scale factor; less accurate (±25-40ml); assumes load cell consistency across units |
| **Option 3 (Flexible Volume)** | More complex UI with volume selection; user must measure water accurately; longer process (~40s); more error-prone |
| **Option 4 (Zero Only)** | Doesn't provide standalone functionality; user can't see ml readings; defeats purpose of standalone mode |

---

## E-Paper Display Research Findings

### Hardware Specifications
- **Model**: Adafruit 2.13" Mono E-Paper FeatherWing (GDEY0213B74)
- **Controller**: SSD1680 (via Adafruit_ThinkInk library)
- **Resolution**: 250×122 pixels (monochrome)
- **Refresh Time**: ~1-2 seconds per full update

### Partial Update Support: NOT AVAILABLE

**Critical Limitation:** The SSD1680 controller does NOT support partial/fast refresh modes.

**Controllers with Partial Update Support (for reference):**
- IL0373
- SSD1681 (newer version)
- SSD1683
- UC8151D

**Implications for Calibration UI:**
- ❌ Cannot show real-time progress bars (would require 10+ refreshes = 10+ seconds)
- ❌ Cannot animate "..." pending indicators smoothly
- ❌ Cannot update specific regions independently
- ✅ CAN show static messages with updates only at state transitions
- ✅ Battery efficient: fewer refreshes = longer battery life

### Recommended Display Update Strategy

**Calibration Flow (5-7 updates total):**
1. **Trigger calibration** → Show "Calibration Mode" + empty instructions (1 refresh)
2. **Detect upright bottle** → Show "Measuring empty..." (1 refresh)
3. **10-second measurement** → NO updates (static display)
4. **Measurement complete** → Show ADC value + confirm prompt (1 refresh)
5. **User confirms** → Show fill instructions (1 refresh)
6. **Detect upright bottle** → Show "Measuring full..." (1 refresh)
7. **10-second measurement** → NO updates (static display)
8. **Measurement complete** → Show ADC value + confirm prompt (1 refresh)
9. **User confirms** → Show "Complete!" + scale factor (1 refresh)

**Total Display Time:** ~6-10 seconds (for display refreshes only)
**Total Calibration Time:** ~40-50 seconds (including measurements and user actions)

### Alternative: Serial Monitor Feedback

For development and debugging, provide real-time feedback via Serial:
```cpp
// During 10-second measurement:
Serial.println("Sample 1/10: 12345 ADC");
Serial.println("Sample 2/10: 12348 ADC");
...
Serial.println("Variance: 2.3 (stable)");
```

This allows developers to see progress without updating the e-paper display.

---

## Implementation Priority

**Phase 1 - Core Calibration (MVP):**
1. Gesture detection (inverted hold, sideways tilt, upright stable)
2. Two-point calibration state machine
3. NVS storage (save/load scale_factor and empty_adc)
4. Basic calibration UI screens
5. Integration into main.cpp

**Phase 2 - Enhancements (Optional):**
6. Quick re-tare feature (hybrid approach)
7. Calibration verification mode (display current scale factor)
8. Calibration reset gesture (e.g., hold inverted + double sideways tilt)

---

## Open Questions

None - all requirements clarified:
- ✅ End-user setup without reference weights
- ✅ Moderate accuracy target (±15-30ml)
- ✅ Simple interaction
- ✅ Persistent calibration
- ✅ Bottle is permanently attached (critical design constraint)
- ✅ 830ml bottle capacity confirmed

---

## Next Steps

1. ✅ User review and approve this plan
2. Implement gesture detection module (foundation)
3. Implement weight measurement utilities (stable readings, variance checking)
4. Add NVS storage layer (Preferences library)
5. Build calibration state machine
6. Create calibration UI screens for e-paper display
7. Integrate into main loop (calibration mode vs tracking mode)
8. Test with physical hardware (empty bottle, fill to 830ml, verify accuracy)

