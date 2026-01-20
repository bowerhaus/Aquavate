# Plan 025: "Shake to Empty" Gesture

## Summary

Add a new gesture that detects when the bottle is inverted (90°+) and shaken for 1-2 seconds. This indicates the bottle has been emptied and the last recorded drink should be removed from the daily total. A "Bottle Emptied" confirmation screen is shown for 3 seconds.

## User Requirements (Confirmed)

1. **Gesture:** Shake while tilted 90°+ (inverted) for 1-2 seconds
2. **Trigger:** Sets "empty pending" flag → executes when bottle returns to UPRIGHT_STABLE
3. **Weight verification:** Only process if bottle is approximately empty (<50ml) when returned upright
4. **Normal drink if not empty:** If bottle still has liquid (≥50ml), count as normal drink instead
5. **Edge case:** Silently ignore if no drinks recorded today
6. **Feedback:** Show "Bottle Emptied" screen for 3 seconds (same style as calibration screens)
7. **No cooldown needed:** Natural flow prevents rapid multiple cancellations

## Implementation Approach

### Key Insight: Mutual Exclusion with INVERTED_HOLD

The existing `GESTURE_INVERTED_HOLD` (calibration trigger) requires holding the bottle **still** for 5 seconds. The new shake gesture requires **shaking** while inverted. These are naturally mutually exclusive:

- **High variance** (shaking) → `GESTURE_SHAKE_WHILE_INVERTED`
- **Low variance** (still) for 5s → `GESTURE_INVERTED_HOLD`

### Detection Logic

```
IF bottle inverted (Y > -0.3g, approximately 70°+ from vertical)
  AND variance > SHAKE_VARIANCE_THRESHOLD (0.08g²)
  AND shake duration >= SHAKE_DURATION_MS (1500ms)
THEN return GESTURE_SHAKE_WHILE_INVERTED
```

The threshold Y > -0.3g corresponds to ~70° tilt (cos(70°) ≈ 0.34), ensuring the bottle is nearly inverted.

---

## Implementation Progress

Track implementation status here. Check off each step as completed.

### Step 1: Configuration Constants
- [x] Add to `firmware/src/config.h`:
  - `GESTURE_SHAKE_INVERTED_Y_THRESHOLD` (-0.3f)
  - `GESTURE_SHAKE_VARIANCE_THRESHOLD` (0.08f)
  - `GESTURE_SHAKE_DURATION_MS` (1500)
  - `BOTTLE_EMPTY_THRESHOLD_ML` (50)

### Step 2: Gesture Type Enum
- [x] Add `GESTURE_SHAKE_WHILE_INVERTED` to `GestureType` enum in `firmware/include/gestures.h`

### Step 3: Gesture Detection Logic
- [x] Add static variables to `firmware/src/gestures.cpp`:
  - `g_shake_start_time`
  - `g_shake_active`
  - `g_shake_triggered`
- [x] Add shake detection logic before INVERTED_HOLD check
- [x] Reset shake state in `gesturesReset()`

### Step 4: Drink Cancellation Function
- [x] Add `drinksCancelLast()` declaration to `firmware/include/drinks.h`
- [x] Implement `drinksCancelLast()` in `firmware/src/drinks.cpp`

### Step 5: UI Confirmation Screen
- [x] Add `uiShowBottleEmptied()` declaration to `firmware/include/ui_calibration.h`
- [x] Implement `uiShowBottleEmptied()` in `firmware/src/ui_calibration.cpp`

### Step 6: Main Loop Integration
- [x] Add `g_cancel_drink_pending` static variable to `firmware/src/main.cpp`
- [x] Handle `GESTURE_SHAKE_WHILE_INVERTED` (set pending flag)
- [x] Handle pending cancellation on `GESTURE_UPRIGHT_STABLE` with weight check

### Step 7: Build & Test
- [x] Build firmware successfully
- [ ] Test gesture detection (shake vs hold)
- [ ] Test cancellation with empty bottle
- [ ] Test normal drink when bottle not empty

---

## Files to Modify

### 1. firmware/src/config.h
Add new configuration parameters:
```cpp
// Shake-while-inverted gesture (shake to empty / bottle emptied)
#define GESTURE_SHAKE_INVERTED_Y_THRESHOLD  -0.3f   // Y > -0.3g for ~70° tilt
#define GESTURE_SHAKE_VARIANCE_THRESHOLD    0.08f   // Variance > 0.08g² indicates shaking
#define GESTURE_SHAKE_DURATION_MS           1500    // 1.5 seconds of shaking required
#define BOTTLE_EMPTY_THRESHOLD_ML           50      // Bottle considered empty if <50ml remaining
```

### 2. firmware/include/gestures.h
Add to `GestureType` enum:
```cpp
GESTURE_SHAKE_WHILE_INVERTED,  // Shake while inverted (shake to empty)
```

### 3. firmware/src/gestures.cpp
Add static variables for shake tracking:
- `g_shake_start_time` - when shake started
- `g_shake_active` - tracking shake duration
- `g_shake_triggered` - latch to prevent re-triggering

Add shake detection logic before existing INVERTED_HOLD check.

### 4. firmware/include/drinks.h
Add new function declaration:
```cpp
bool drinksCancelLast();  // Cancel most recent drink, returns true if successful
```

### 5. firmware/src/drinks.cpp
Implement `drinksCancelLast()`:
- Load last drink record from storage
- Check if drink exists and is from today
- Subtract amount from `daily_total_ml`
- Decrement `drink_count_today`
- Save updated `DailyState` to NVS
- Return true if cancelled, false if nothing to cancel

### 6. firmware/include/ui_calibration.h
Add new function declaration:
```cpp
void uiShowBottleEmptied();  // Show "Bottle Emptied" confirmation
```

### 7. firmware/src/ui_calibration.cpp
Implement `uiShowBottleEmptied()`:
```cpp
void uiShowBottleEmptied() {
    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);
    printCentered("Bottle", 35, 3);
    printCentered("Emptied", 70, 3);
    g_display->display();
}
```

### 8. firmware/src/main.cpp
Add integration:
- New static variable: `g_cancel_drink_pending`
- When `GESTURE_SHAKE_WHILE_INVERTED` detected: set `g_cancel_drink_pending = true`
- When `GESTURE_UPRIGHT_STABLE` detected AND `g_cancel_drink_pending`:
  - Check weight - if bottle empty (<50ml), call `drinksCancelLast()`
  - If successful: show `uiShowBottleEmptied()`, delay 3 seconds
  - If bottle not empty: process as normal drink
  - Reset `g_cancel_drink_pending = false`

---

## Detailed Code Specifications

### gestures.cpp - Shake Detection Logic

Insert before the existing inverted hold check (line ~130):

```cpp
// Check for shake-while-inverted gesture (highest priority when shaking)
// Y > -0.3g indicates bottle tilted 70°+ from vertical
if (g_current_y > GESTURE_SHAKE_INVERTED_Y_THRESHOLD) {
    float variance = gesturesGetVariance();
    uint32_t now = millis();

    // Check if actively shaking (high variance)
    if (variance > GESTURE_SHAKE_VARIANCE_THRESHOLD) {
        if (!g_shake_active) {
            g_shake_active = true;
            g_shake_triggered = false;
            g_shake_start_time = now;
            Serial.println("Gestures: Shake while inverted detected - keep shaking...");
        } else if (!g_shake_triggered) {
            uint32_t shake_duration = now - g_shake_start_time;
            if (shake_duration >= GESTURE_SHAKE_DURATION_MS) {
                Serial.println("Gestures: SHAKE_WHILE_INVERTED gesture triggered!");
                g_shake_triggered = true;
                return GESTURE_SHAKE_WHILE_INVERTED;
            }
        } else {
            // Already triggered - keep returning until bottle returns upright
            return GESTURE_SHAKE_WHILE_INVERTED;
        }
    } else {
        // Not shaking - reset shake tracking (allows INVERTED_HOLD to work)
        g_shake_active = false;
        g_shake_triggered = false;
    }
}
// ... existing INVERTED_HOLD code follows ...
```

### drinks.cpp - Cancel Last Drink

```cpp
bool drinksCancelLast() {
    // Check if any drinks today
    if (g_daily_state.drink_count_today == 0) {
        Serial.println("Drinks: No drinks to cancel");
        return false;
    }

    // Load last drink record
    DrinkRecord last_drink;
    if (!storageLoadLastDrinkRecord(last_drink)) {
        Serial.println("Drinks: Failed to load last drink record");
        return false;
    }

    // Verify it's a drink (positive amount) not a refill
    if (last_drink.amount_ml <= 0) {
        Serial.println("Drinks: Last record is a refill, not cancelling");
        return false;
    }

    // Subtract from daily total
    uint16_t cancel_amount = (uint16_t)last_drink.amount_ml;
    if (g_daily_state.daily_total_ml >= cancel_amount) {
        g_daily_state.daily_total_ml -= cancel_amount;
    } else {
        g_daily_state.daily_total_ml = 0;
    }

    // Decrement drink count
    if (g_daily_state.drink_count_today > 0) {
        g_daily_state.drink_count_today--;
    }

    // Save updated state
    storageSaveDailyState(g_daily_state);

    Serial.printf("Drinks: Cancelled last drink of %dml. New total: %dml\n",
                  cancel_amount, g_daily_state.daily_total_ml);

    return true;
}
```

### main.cpp - Integration

Add near other gesture handling (~line 1020):

```cpp
// Handle shake-while-inverted gesture (set pending flag)
if (gesture == GESTURE_SHAKE_WHILE_INVERTED) {
    g_cancel_drink_pending = true;
    Serial.println("Main: Cancel drink pending - return bottle upright to confirm");
}

// Handle pending cancellation when bottle returns to upright stable
if (gesture == GESTURE_UPRIGHT_STABLE && g_cancel_drink_pending) {
    g_cancel_drink_pending = false;

    // Get current water level to verify bottle is actually empty
    float current_water_ml = cycleSnapshot.water_ml;

    if (current_water_ml < BOTTLE_EMPTY_THRESHOLD_ML) {
        // Bottle is empty - cancel the last drink
        if (drinksCancelLast()) {
            uiShowBottleEmptied();
            delay(3000);  // Show confirmation for 3 seconds
            displayForceUpdate();  // Return to normal display
        }
        // If drinksCancelLast() returns false (nothing to cancel), silently ignore
    } else {
        // Bottle still has liquid - treat as normal drink detection
        Serial.printf("Main: Bottle not empty (%.1fml) - processing as normal drink\n", current_water_ml);
        // Normal drinksUpdate() will be called below, detecting the drink as usual
    }
}
```

**Key Logic:** The weight verification ensures:
- If user shakes but bottle still has water → normal drink is recorded
- If user shakes and bottle is empty (<50ml) → last drink is cancelled
- This prevents accidental cancellations from misinterpreted gestures

---

## Verification Plan

1. **Build test:** Compile with `platformio run` - verify no errors
2. **Gesture detection tests:**
   - Invert and shake for 2 seconds → should trigger gesture
   - Invert and hold still for 5 seconds → should trigger calibration (not shake)
   - Shake while upright → should NOT trigger
   - Tilt 45° and shake → should NOT trigger (not inverted enough)
3. **Integration test - bottle emptied:**
   - Record a drink by drinking some water
   - Empty the bottle completely (pour out remaining water)
   - Invert bottle, shake for 2 seconds
   - Return bottle upright
   - Verify "Bottle Emptied" screen appears for 3 seconds
   - Verify daily total decreased by last drink amount
4. **Integration test - bottle NOT emptied:**
   - Record a drink by drinking some water (leave water in bottle)
   - Invert bottle, shake for 2 seconds
   - Return bottle upright (with water still in it)
   - Verify NO cancellation occurs
   - Verify normal drink detection works (any new water removed is counted)
5. **Edge case tests:**
   - Shake-while-inverted with no drinks recorded today → verify silent ignore
   - Shake-while-inverted with empty bottle but no drinks → verify silent ignore
   - Multiple shakes → verify only one cancellation per shake cycle

---

## Summary of Changes

| File | Change |
|------|--------|
| gestures.h | Add `GESTURE_SHAKE_WHILE_INVERTED` enum |
| gestures.cpp | Add shake detection logic (~30 lines) |
| config.h | Add 4 threshold constants |
| drinks.h | Add `drinksCancelLast()` declaration |
| drinks.cpp | Implement `drinksCancelLast()` (~35 lines) |
| ui_calibration.h | Add `uiShowBottleEmptied()` declaration |
| ui_calibration.cpp | Implement confirmation screen (~10 lines) |
| main.cpp | Add gesture handling + pending flag (~15 lines) |

**Estimated total new code:** ~90 lines
