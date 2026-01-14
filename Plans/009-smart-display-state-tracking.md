# Implementation Plan: Smart Display State Tracking

**Status:** ✅ **COMPLETE** (2026-01-14)

## Overview

Make the Aquavate display module smarter by tracking ALL displayed values (water level, daily intake, time, battery) and only updating when ANY value changes significantly. Currently, the display only updates when water level or daily intake changes, but time and battery are never independently tracked.

## Implementation Summary

Successfully implemented smart display state tracking with the following outcomes:

- **Files Created:**
  - [firmware/include/display.h](../firmware/include/display.h) - DisplayState structure and public API
  - [firmware/src/display.cpp](../firmware/src/display.cpp) - State tracking implementation (~550 lines)

- **Files Modified:**
  - [firmware/src/config.h](../firmware/src/config.h) - Added 4 display config constants (lines 157-162)
  - [firmware/src/main.cpp](../firmware/src/main.cpp) - Integrated display module, removed ~400 lines of drawing code

- **Build Status:** ✅ Compiles successfully
  - RAM: 7.0% (22,804 bytes) - +20 bytes
  - Flash: 35.7% (468,132 bytes) - +~3KB

- **Key Improvements:**
  - Time updates immediately on hour changes (e.g., 9:59am → 10:00am)
  - Battery tracked in 20% quantized steps
  - All updates only when bottle is UPRIGHT_STABLE (prevents display flicker)
  - Single e-paper refresh even when multiple values change

## User Requirements

- **Time updates:** Every 15 minutes
- **Battery updates:** 20% increment threshold
- **Update strategy:** Proactive checks (not just piggyback on other changes)
- **Goal:** Display tracks all values and only refreshes when needed

## Implementation Approach

Create a new display module with centralized state tracking. This separates display logic from main.cpp (currently 1220 lines) and follows the existing modular pattern (gestures, calibration, drinks, storage).

## Critical Files

1. **firmware/include/display.h** (NEW) - DisplayState structure and API
2. **firmware/src/display.cpp** (NEW) - State tracking implementation
3. **firmware/src/main.cpp** - Integration point, move drawing code
4. **firmware/src/config.h** - Add time/battery configuration constants
5. **firmware/include/drinks.h** - Reference for DailyState structure pattern

## Implementation Steps

### Step 1: Create Display Module Foundation

**Create firmware/include/display.h:**
```cpp
#ifndef AQUAVATE_DISPLAY_H
#define AQUAVATE_DISPLAY_H

#include <Adafruit_ThinkInk.h>

struct DisplayState {
    float water_ml;                    // Current bottle level (0-830ml)
    uint16_t daily_total_ml;           // Today's intake total
    uint8_t hour;                      // Current hour (0-23)
    uint8_t minute;                    // Current minute (0-59)
    uint8_t battery_percent;           // Battery % quantized to 20% steps
    uint32_t last_update_ms;           // Last full display update
    uint32_t last_time_check_ms;       // Last time check
    uint32_t last_battery_check_ms;    // Last battery check
    bool initialized;                  // Has state been initialized?
};

// Public API
void displayInit(ThinkInk_213_Mono_GDEY0213B74& display_ref);
bool displayNeedsUpdate(float current_water_ml,
                       uint16_t current_daily_ml,
                       bool time_interval_elapsed,
                       bool battery_interval_elapsed);
void displayUpdate(float water_ml, uint16_t daily_total_ml);
void displayForceUpdate();
void drawMainScreen();

#endif
```

**Create firmware/src/display.cpp:**
- Implement DisplayState management
- Core logic in `displayNeedsUpdate()`:
  - Check water level (5ml threshold)
  - Check daily intake (50ml threshold)
  - Check time (15 minute threshold, only if interval elapsed)
  - Check battery (20% threshold, only if interval elapsed)
- Helper function: `quantizeBatteryPercent()` rounds to 0, 20, 40, 60, 80, 100

### Step 2: Add Configuration Constants

**Edit firmware/src/config.h** (after line 153):
```cpp
// Time update parameters (15 minutes)
#define DISPLAY_TIME_UPDATE_INTERVAL_MS     900000
#define DISPLAY_TIME_UPDATE_THRESHOLD_MIN   15

// Battery update parameters
#define DISPLAY_BATTERY_UPDATE_INTERVAL_MS  900000
#define DISPLAY_BATTERY_UPDATE_THRESHOLD    20
```

### Step 3: Move Drawing Code to display.cpp

**Move from main.cpp to display.cpp:**
- `drawMainScreen()` (lines 651-771)
- `drawBottleGraphic()` (lines 344-378)
- `drawHumanFigure()` (lines 525-559)
- `drawGlassGrid()` (lines 567-629)
- `drawBatteryIcon()` (lines 380-395)
- `formatTimeForDisplay()` (lines 397-423)
- Bitmap constants: `water_drop_bitmap`, `human_figure_bitmap`, `human_figure_filled_bitmap`

**Update function signatures** to use global display reference or pass as parameter.

### Step 4: Implement State Tracking Logic

**In display.cpp, implement displayNeedsUpdate():**

```cpp
bool displayNeedsUpdate(float current_water_ml,
                       uint16_t current_daily_ml,
                       bool time_interval_elapsed,
                       bool battery_interval_elapsed) {
    bool needs_update = false;

    // 1. Water level check (5ml threshold)
    if (!g_display_state.initialized ||
        fabs(current_water_ml - g_display_state.water_ml) >= DISPLAY_UPDATE_THRESHOLD_ML) {
        needs_update = true;
    }

    // 2. Daily intake check (50ml threshold)
    if (abs((int)current_daily_ml - (int)g_display_state.daily_total_ml) >=
        DRINK_DISPLAY_UPDATE_THRESHOLD_ML) {
        needs_update = true;
    }

    // 3. Time check (if interval elapsed)
    if (time_interval_elapsed && g_time_valid) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec + (g_timezone_offset * 3600);
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);

        if (timeinfo.tm_hour != g_display_state.hour ||
            abs(timeinfo.tm_min - g_display_state.minute) >= DISPLAY_TIME_UPDATE_THRESHOLD_MIN) {
            needs_update = true;
        }
    }

    // 4. Battery check (if interval elapsed)
    if (battery_interval_elapsed) {
        float voltage = getBatteryVoltage();
        int raw_percent = getBatteryPercent(voltage);
        uint8_t quantized = quantizeBatteryPercent(raw_percent);

        if (abs((int)quantized - (int)g_display_state.battery_percent) >=
            DISPLAY_BATTERY_UPDATE_THRESHOLD) {
            needs_update = true;
        }
    }

    return needs_update;
}
```

**Implement displayUpdate():**
- Call `drawMainScreen()` to render
- Update DisplayState with new values
- Save timestamp

**Implement displayForceUpdate():**
- Reset DisplayState.initialized = false
- Call displayUpdate()

### Step 5: Integrate with main.cpp

**In setup() function (~line 824):**
```cpp
#include "display.h"

// After display.begin() and drawWelcomeScreen()
displayInit(display);
```

**Remove line 293:**
```cpp
// DELETE: float g_last_displayed_water_ml = -1.0f;
```

**Replace display update block (lines 1099-1147):**
```cpp
// Add static timing variables at function scope
static unsigned long last_level_check = 0;
static unsigned long last_time_check = 0;
static unsigned long last_battery_check = 0;

// In the gesture check section:
if (cal_state == CAL_IDLE && g_calibrated && gesture == GESTURE_UPRIGHT_STABLE) {
    if (millis() - last_level_check >= DISPLAY_UPDATE_INTERVAL_MS) {
        last_level_check = millis();

        // Check interval timers
        bool time_interval_elapsed = (millis() - last_time_check >= DISPLAY_TIME_UPDATE_INTERVAL_MS);
        bool battery_interval_elapsed = (millis() - last_battery_check >= DISPLAY_BATTERY_UPDATE_INTERVAL_MS);

        // Get current values
        float current_water_ml = current_water_ml;  // From sensor snapshot
        DailyState daily_state;
        drinksGetState(daily_state);

        // Update drink tracking
        if (g_time_valid) {
            drinksUpdate(current_adc, g_calibration);
            drinksGetState(daily_state);  // Refresh after update
        }

        // Check if display needs update
        if (displayNeedsUpdate(current_water_ml, daily_state.daily_total_ml,
                              time_interval_elapsed, battery_interval_elapsed)) {
            displayUpdate(current_water_ml, daily_state.daily_total_ml);

            // Reset interval timers if they were checked
            if (time_interval_elapsed) last_time_check = millis();
            if (battery_interval_elapsed) last_battery_check = millis();
        }
    }
}
```

**Update forced refresh calls:**
- Line 1078: `drawMainScreen()` → `displayForceUpdate()`
- Line 1095: `drawMainScreen()` → `displayForceUpdate()`

**Update firmware/platformio.ini:**
Add display.cpp to build sources (should auto-detect, verify it compiles)

### Step 6: Optional - Trigger from Drinks Module

**In firmware/src/drinks.cpp line 195** (currently has TODO comment):
```cpp
// After daily_total_ml update in drinksUpdate()
// Optional: Add flag to force display check on next iteration
extern bool g_display_needs_check;  // Declare in display.h
g_display_needs_check = true;
```

This allows drink detection to hint that display should check soon.

## Edge Cases Handled

1. **First boot:** DisplayState.initialized = false forces first update
2. **After calibration:** `displayForceUpdate()` resets state
3. **Time not set:** Skip time checks if `g_time_valid == false`
4. **Battery reading errors:** Keep last known value, log error
5. **Deep sleep wake:** State persists in RAM, check on first upright stable
6. **Hour boundaries:** Time threshold detects hour changes
7. **Multiple triggers:** Single update even if multiple values changed

## Verification Plan

### Unit Testing (Manual)

1. **Water level updates:**
   - Pour water, verify 5ml threshold works
   - Check display doesn't flicker on small (<5ml) changes

2. **Daily intake updates:**
   - Record multiple drinks
   - Verify 50ml threshold works
   - Check daily total display and visualization update

3. **Time updates:**
   - Set time, wait 15 minutes with stable water level
   - Verify display refreshes to show new time
   - Test across hour boundaries

4. **Battery updates:**
   - Start with full battery (100%)
   - Drain to ~80%, verify update
   - Check 20% thresholds: 80%, 60%, 40%, 20%, 0%

5. **Combined updates:**
   - Change water level during time interval
   - Verify single refresh, both values update

6. **Edge cases:**
   - Boot device, verify first update works
   - Complete calibration, verify display force updates
   - Test with time not set (skip time checks)

### Build Verification

```bash
cd firmware
~/.platformio/penv/bin/platformio run
# Verify clean build with no errors
# Check binary size increase (~3KB expected)
```

### Integration Testing

1. Flash firmware to ESP32 Feather
2. Monitor serial output for display update logs
3. Use oscilloscope on EPD_BUSY pin to measure refresh frequency
4. Verify battery life impact (should be minimal)

## Memory Impact

- **RAM:** +20 bytes (DisplayState) - negligible
- **Flash:** +~3KB (new module code) - acceptable
- **Current usage:** 6.9% RAM, 34.7% Flash - plenty of headroom

## Configuration Tuning

After testing, these values may need adjustment:

- `DISPLAY_TIME_UPDATE_INTERVAL_MS` - Currently 15 min (900000ms)
- `DISPLAY_TIME_UPDATE_THRESHOLD_MIN` - Currently 15 minutes
- `DISPLAY_BATTERY_UPDATE_INTERVAL_MS` - Currently 15 min
- `DISPLAY_BATTERY_UPDATE_THRESHOLD` - Currently 20%

## Pros & Cons

### Pros
- Unified state tracking for ALL displayed values
- Extensible (easy to add temperature, WiFi status, etc.)
- Testable (isolated display logic)
- Efficient (only checks when intervals elapsed)
- Clean separation from main.cpp

### Cons
- Adds 2 new files to codebase
- Requires moving ~400 lines from main.cpp
- Need to test all update combinations
- Time/battery intervals may need field tuning

## Dependencies

- Existing: Adafruit_ThinkInk, RTClib, battery functions
- No new external libraries required
- Compatible with both Feather and Qwiic builds

## Rollback Plan

If issues arise:
1. Revert display.h and display.cpp
2. Restore main.cpp to original state
3. Previous behavior: water/daily updates only (working state)

---

## Implementation Results (2026-01-14)

### ✅ Completed Features

1. **Display Module Created**
   - New modular architecture separates display logic from main.cpp
   - DisplayState structure tracks all on-screen values
   - Clean API: `displayInit()`, `displayNeedsUpdate()`, `displayUpdate()`, `displayForceUpdate()`

2. **Smart Time Updates**
   - Checks time every 2 seconds when bottle is stable
   - Updates immediately when hour changes (e.g., 9:59 → 10:00)
   - Also updates every 15 minutes for minute-level changes
   - Time format: "Wed 10am" (day + 12-hour format)

3. **Battery Monitoring**
   - Checks battery every 15 minutes when stable
   - Quantized to 20% steps: 0%, 20%, 40%, 60%, 80%, 100%
   - Only triggers display update on 20% threshold crossings

4. **Optimized Update Logic**
   - All checks only run when `GESTURE_UPRIGHT_STABLE`
   - Single e-paper refresh even if multiple values changed
   - Water level: 5ml threshold
   - Daily intake: 50ml threshold

### 📊 Performance Metrics

- **Memory Impact:** +20 bytes RAM, +3KB flash (negligible)
- **Build Time:** ~11 seconds
- **Update Frequency:** Every 2 seconds when bottle stable on table
- **E-Paper Refresh:** Only when values cross thresholds

### 🔧 Configuration Added

```cpp
// firmware/src/config.h (lines 157-162)
#define DISPLAY_TIME_UPDATE_INTERVAL_MS     900000  // Check time every 15 min
#define DISPLAY_TIME_UPDATE_THRESHOLD_MIN   15      // Update if ≥15 min elapsed
#define DISPLAY_BATTERY_UPDATE_INTERVAL_MS  900000  // Check battery every 15 min
#define DISPLAY_BATTERY_UPDATE_THRESHOLD    20      // Update if ≥20% change
```

### 🎯 User Experience Improvements

- **Before:** Time could become stale indefinitely if bottle sits unused
- **After:** Time updates within 2 seconds of hour changes, guaranteed fresh every 15 minutes

- **Before:** Battery percentage never updated unless water level changed
- **After:** Battery monitored independently, updates on 20% thresholds

- **Before:** Display logic scattered across 400+ lines in main.cpp
- **After:** Clean modular design, all display code in dedicated module

### 🧪 Verification Status

- ✅ Firmware builds successfully
- ✅ Hour boundary updates confirmed (9:59am → 10:00am logic verified)
- ✅ All updates gated by UPRIGHT_STABLE gesture
- ⏳ Hardware testing pending (requires device flash)

### 📝 Notes

- Time/battery checks run every 2 seconds but only check *interval timers* every 15 minutes
- Hour changes bypass the 15-minute interval check for immediate updates
- Battery quantization reduces unnecessary e-paper refreshes (e.g., 83% and 79% both show "80%")
- Display module now owns all drawing code (bottle, human figure, tumbler grid, battery icon)

