# Plan: Fix Daily Intake Reset at 4am (Issue #17)

**Status:** ✅ COMPLETE (2026-01-21)

## Problem Summary

The bottle's daily intake total is not resetting correctly at 4am. The iOS app shows the correct total (drinks from 4am onwards), but the bottle shows a higher total because it includes drinks from before 4am.

## Root Cause

The `daily_total_ml` and `drink_count_today` are stored as accumulated values in NVS. When the bottle boots, it loads these stale values instead of computing them from actual drink records.

## Solution: Full Cleanup

Remove redundant cached fields from `DailyState` and always compute daily totals dynamically from drink records using the 4am boundary logic (matching the iOS app).

### Files to Modify

1. **[firmware/include/drinks.h](firmware/include/drinks.h)** - Simplify DailyState struct
2. **[firmware/src/drinks.cpp](firmware/src/drinks.cpp)** - Remove accumulation logic, always compute from records
3. **[firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp)** - Update NVS functions for smaller struct

### Changes

#### 1. drinks.h - Simplify DailyState

Remove redundant fields:
```cpp
// DailyState structure: Tracks drink detection state (8 bytes, down from 18)
struct DailyState {
    int32_t  last_recorded_adc;         // Baseline ADC value for drink detection
    uint16_t last_displayed_total_ml;   // Last displayed total (for hysteresis)
    uint16_t _reserved;                 // Padding for alignment
};
```

Removed:
- `last_reset_timestamp` - not needed, we use record timestamps
- `last_drink_timestamp` - not needed for current logic
- `daily_total_ml` - always computed from records
- `drink_count_today` - always computed from records

Add new function declaration:
```cpp
uint16_t drinksGetDailyTotal();  // Returns computed daily total
uint16_t drinksGetDrinkCount();  // Returns computed drink count
```

#### 2. drinks.cpp - Major refactor

- `drinksInit()`: Just load baseline ADC, call `drinksRecalculateTotal()` to initialize computed values
- `drinksUpdate()`: Remove accumulation of `daily_total_ml`, rely on records
- Remove `shouldResetDailyCounter()` and `performAtomicDailyReset()` - no longer needed
- `drinksRecalculateTotal()`: Already correct, becomes the authoritative source
- Add `drinksGetDailyTotal()` and `drinksGetDrinkCount()` that call recalculate or return cached computed values
- Update `drinksCancelLast()` to just mark record deleted and recalculate
- Update `drinksResetDaily()` to clear records for today

#### 3. storage_drinks.cpp - Update for smaller struct

The NVS load/save will automatically handle the smaller struct size. Old data will fail to load (size mismatch), which is fine - we'll initialize fresh and records are preserved.

#### 4. main.cpp, display.cpp, ble_service.cpp - Use new API

Replace `daily_state.daily_total_ml` with `drinksGetDailyTotal()` calls.

### Data Migration

- **Drink records**: Preserved (stored separately in circular buffer)
- **Daily total**: Recomputed from records on first boot
- **No data loss**: All historical drinks remain intact

## Verification

1. **Build firmware:** `cd firmware && ~/.platformio/penv/bin/platformio run`
2. **Test scenario:**
   - Record drinks before midnight
   - After 4am the next day, power cycle the bottle
   - Verify bottle shows 0ml (only today's drinks from 4am onwards)
   - Verify iOS app and bottle totals match
3. **Serial monitor:** Look for "Drinks: Recalculated total = Xml (Y drinks)" at boot
