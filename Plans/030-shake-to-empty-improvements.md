# Shake-to-Empty Feature Improvements

**Status:** COMPLETED
**Date:** 2026-01-21

## Problem Summary

### Issue 1: Extended Sleep Lockout After Shaking
When emptying the bottle and shaking, the device would enter extended deep sleep mode (designed for backpack scenarios) because the continuous motion exceeded the 5-minute awake threshold. This locked the user out with only timer-based wake (60 seconds) instead of motion wake.

### Issue 2: Refill Detection After Shake-to-Empty
After shake-to-empty, the baseline was reset to the current water level. If the bottle wasn't completely empty when put down, refill detection wouldn't work because the delta was too small.

### Issue 3: Water Level Debug Messages Missing
Water level debug output wasn't showing in IOS_MODE=1 (production mode) because it was nested inside the `#if ENABLE_STANDALONE_CALIBRATION` block.

---

## Solutions Implemented

### Fix 1: Reset Extended Sleep Timer on User Interactions

Extended sleep is for passive motion (backpack), not active user interactions. Added timer resets for unambiguous user actions:

**Changes in main.cpp:**

1. **Shake detected** (~line 1027): Reset `g_continuous_awake_start` when shake gesture triggers
2. **Drink recorded** (~line 1337): Reset timer when `drinksUpdate()` returns true
3. **UPRIGHT_STABLE** (~line 1477): Moved reset to happen continuously (not just after threshold exceeded)

**Changes in drinks.cpp/h:**
- Changed `drinksUpdate()` to return `bool` (true if drink recorded)

### Fix 2: Debug Output Available in All Modes

Moved water level debug output outside the `#if ENABLE_STANDALONE_CALIBRATION` block so it runs regardless of IOS_MODE setting.

### Fix 3: Added Drink Tracking Debug Output

Added debug output in `drinksUpdate()` to show baseline/current/delta values:
```
Drinks: baseline=497.6ml, current=497.6ml, delta=0.0ml
```

---

## Files Modified

| File | Changes |
|------|---------|
| [main.cpp](../firmware/src/main.cpp) | Extended sleep timer resets on shake/drink/stable; moved debug output outside calibration block |
| [drinks.cpp](../firmware/src/drinks.cpp) | `drinksUpdate()` returns bool; added delta debug output |
| [drinks.h](../firmware/include/drinks.h) | Updated `drinksUpdate()` signature to return bool |

---

## Design Decisions

### Refill Detection Trade-off
After shake-to-empty, the baseline is reset to the **current** water level (not empty). This means:
- **Pro:** Shaking mid-pour doesn't incorrectly mark bottle as empty
- **Con:** No refill detection message after shake-to-empty + refill

This trade-off was accepted because false "bottle emptied" on mid-pour shake would be more confusing than missing refill detection.

### Extended Sleep User Activity Signals
Only these signals reset the extended sleep timer (all unambiguous):
- Shake gesture detected
- Drink recorded
- UPRIGHT_STABLE achieved (bottle placed on surface)

**NOT included** (too ambiguous):
- Weight changes (sloshing in backpack)
- BLE activity (could be background sync)
- Generic gesture changes (constant in backpack)

---

## Testing Performed

1. **Shake-to-empty flow:** Pour out water, shake while inverted, put down
   - Result: "Bottle Emptied" shown, no false drink recorded

2. **Extended sleep after shake:** Shake for several minutes
   - Result: No extended sleep lockout (timer resets on shake)

3. **Normal drinking:** Drink, put bottle down
   - Result: Drink recorded correctly, no false shake triggers

4. **Debug output:** Water level messages visible in serial monitor
   - Result: Shows in IOS_MODE=1 (production mode)

---

## Verification Complete

- [x] Firmware builds successfully
- [x] Shake-to-empty prevents false drink recording
- [x] Extended sleep timer resets on user interactions
- [x] Water level debug output visible in all modes
- [x] No regression in normal drink detection
