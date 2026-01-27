# Plan: Fix Missing Drinks During Active Sessions

## Issue Summary

The bottle sometimes misses recording drinks when a second drink is taken during the same session. User suspected it might be related to "Bluetooth weight period" when connected to the app.

## Root Cause Analysis

**The issue is NOT BLE-related.** Investigation confirmed:
- There is no "Bluetooth weight period" concept in the code
- BLE connection does not affect drink detection timing or frequency
- BLE callbacks run asynchronously and only set flags; the main loop processes drinks independently

**The actual root cause is in [drinks.cpp:295-298](firmware/src/drinks.cpp#L295-L298):**

```cpp
// No significant change - update baseline for drift compensation
else {
    g_daily_state.last_recorded_adc = current_adc;
}
```

This line **unconditionally updates the baseline ADC every time `drinksUpdate()` is called** (every 5 seconds when bottle is stable), even when no drink is detected. This "drift compensation" mechanism causes the baseline to become contaminated.

### Failure Scenario

1. **T=0s**: First drink (100ml) detected
   - Baseline updated to post-drink level (ADC=X)

2. **T=5s**: Second drink (40ml) taken, but ADC shows intermediate value due to stabilization
   - Current ADC: Y (intermediate reading during hand motion)
   - Delta from baseline: 20ml (< 30ml threshold)
   - **No drink detected**, but baseline updated to Y

3. **T=10s**: True post-drink level stabilizes
   - Current ADC: Z (actual 40ml lower than baseline)
   - But baseline is now Y (contaminated), not X
   - Delta: only 20ml from Y to Z
   - **Second drink missed** because delta < 30ml threshold

### Contributing Factors

1. **5-second update interval**: Drinks only checked every 5 seconds when `UPRIGHT_STABLE`
2. **No hysteresis on baseline**: Baseline changes immediately on every reading
3. **ADC jitter during stabilization**: Intermediate readings contaminate the baseline

## Proposed Fix

Add hysteresis to the baseline update logic. Only update baseline for drift if the change is very small (true drift, not a drink in progress).

### Changes to [firmware/src/drinks.cpp](firmware/src/drinks.cpp)

**Before (lines 295-298):**
```cpp
// No significant change - update baseline for drift compensation
else {
    g_daily_state.last_recorded_adc = current_adc;
}
```

**After:**
```cpp
// No significant change - update baseline for drift compensation
// Only update if change is very small (< 15ml) to avoid contaminating
// baseline with intermediate readings during second drink
else {
    float abs_delta = (delta_ml > 0) ? delta_ml : -delta_ml;
    if (abs_delta < DRINK_DRIFT_THRESHOLD_ML) {
        g_daily_state.last_recorded_adc = current_adc;
    }
    // If delta is between drift threshold and drink threshold,
    // don't update baseline - a drink may be in progress
}
```

### Changes to [firmware/include/config.h](firmware/include/config.h)

Add new threshold constant:

```cpp
// Drift compensation threshold (ml)
// Only update baseline if change is smaller than this to avoid
// contaminating baseline during multi-drink scenarios
#define DRINK_DRIFT_THRESHOLD_ML        15
```

## Alternative Considered

**Time-based hysteresis**: Only allow baseline updates once every 30 seconds. This would work but is less elegant - the issue is about magnitude, not time. A small 5ml drift should be corrected immediately; a 20ml intermediate reading should never update the baseline regardless of time.

## Files to Modify

1. [firmware/src/drinks.cpp](firmware/src/drinks.cpp) - Add drift threshold check (lines 295-298)
2. [firmware/include/config.h](firmware/include/config.h) - Add `DRINK_DRIFT_THRESHOLD_ML` constant

## Verification

1. Build firmware: `~/.platformio/penv/bin/platformio run`
2. Manual testing on device:
   - Take a drink, wait 5 seconds, take second drink
   - Both drinks should be recorded
   - Enable debug: set `g_debug_drink_tracking = true` and monitor serial output
3. Edge cases to test:
   - Two drinks within 10 seconds
   - Small drink (30-40ml) followed by larger drink
   - Drink followed by jittery hand motion

## Risk Assessment

- **Low risk**: The change only affects when baseline is updated for drift compensation
- **No regression risk**: Drink and refill detection logic unchanged
- **Possible side effect**: Less drift correction during true sensor drift - but 15ml tolerance should handle normal drift
