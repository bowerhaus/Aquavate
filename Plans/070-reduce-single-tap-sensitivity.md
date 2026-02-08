# Plan: Reduce Single-Tap Wake Sensitivity

**Issue**: #110

## Context

The single-tap wake feature (added in Issue #63) allows the bottle to wake from normal deep sleep when tapped. However, slight table nudges were triggering false wakes from sleep.

## Root Cause

Initial investigation assumed the single-tap threshold (3.0g) was too low. Testing with tap thresholds up to 9.0g still produced false wakes. Disabling the activity interrupt while keeping single-tap confirmed the real culprit: the **activity detection threshold** was set to just 0.5g, which table nudges easily exceed. The single-tap detection was never the problem.

## Fix Applied

### 1. Increase activity wake threshold in [config.h](firmware/src/config.h)

```cpp
// Before:
#define ACTIVITY_WAKE_THRESHOLD     0x08    // 0.5g threshold (8 x 62.5mg/LSB)

// After:
#define ACTIVITY_WAKE_THRESHOLD     0x18    // 1.5g threshold (24 x 62.5mg/LSB)
```

### 2. Fix misleading comment in [config.h](firmware/src/config.h)

```cpp
// Before:
// Tap detection for backpack mode wake (ADXL343 double-tap interrupt)

// After:
// Tap detection threshold (shared by single-tap wake + double-tap backpack mode)
```

### 3. Improve serial output in [main.cpp](firmware/src/main.cpp)

Updated the configuration complete message to show both thresholds:
```
Sleep wake: Single-tap (>3.0g) OR activity (>1.5g)
```

`TAP_WAKE_THRESHOLD` left at original `0x30` (3.0g) — no change needed.

## Files Modified

| File | Change |
|------|--------|
| [firmware/src/config.h](firmware/src/config.h) | `ACTIVITY_WAKE_THRESHOLD` 0x08 → 0x18, fix tap comment |
| [firmware/src/main.cpp](firmware/src/main.cpp) | Update serial output message |

## Verification

1. Build: `cd firmware && ~/.platformio/penv/bin/platformio run` ✅
2. User tests on hardware:
   - Table nudges no longer wake the bottle ✅
   - Picking up bottle to pour still wakes it (>1.5g) ✅
   - Single-tap still wakes it ✅
   - If 1.5g is still too sensitive, increase to `0x20` (2.0g)
