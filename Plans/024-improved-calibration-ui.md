# 024 - Improved Calibration UI Flow

## Overview

Improve the calibration UX by adding clearer visual prompts and better error recovery. The new flow adds:
- "Calibration Started" acknowledgment screen (3s)
- Visual bottle graphics with "?" prompts
- Timeout handling (60s empty, 120s full)
- Retry on measurement errors instead of exit

## User Requirements

1. **After inversion (5s hold)** → Show "Calibration Started" screen for 3 seconds
2. **Empty bottle prompt** → Show empty bottle icon with "?" to prompt user to empty and place upright (60s timeout)
3. **Full bottle prompt** → Show full bottle icon with "?" to prompt user to fill and place upright (120s timeout)
4. **Error handling** → Show error screen for 5 seconds, then return to normal mode
5. **Measurement errors** → Return to previous prompt screen to retry (not exit calibration)
6. **Timeout reset** → Reset to full duration when returning from measurement error
7. **E-paper constraint** → One refresh per state transition only
8. **Abort gesture** → Inverted hold (5s) during any calibration state cancels calibration and returns to normal mode

## New Calibration Flow

```
CAL_IDLE
  ↓ (inverted hold 5s)
CAL_TRIGGERED
  ↓ (immediate transition)
CAL_STARTED (NEW) → "Calibration Started" screen
  ↓ (after 3s OR inverted hold 5s → CANCEL)
CAL_WAIT_EMPTY (MODIFIED) → Empty bottle with "?" graphic
  ↓ (GESTURE_UPRIGHT_STABLE OR 60s timeout OR inverted hold 5s → CANCEL)
CAL_MEASURE_EMPTY (MODIFIED) → "Measuring Empty..."
  ↓ (success → next, error → retry CAL_WAIT_EMPTY, inverted hold 5s → CANCEL)
CAL_WAIT_FULL (MODIFIED) → Full bottle with "?" graphic
  ↓ (weight delta + 5s stability OR 120s timeout OR inverted hold 5s → CANCEL)
CAL_MEASURE_FULL (MODIFIED) → "Measuring Full..."
  ↓ (success → next, error → retry CAL_WAIT_FULL, inverted hold 5s → CANCEL)
CAL_COMPLETE or CAL_ERROR or CAL_CANCELLED
```

## State Machine Changes

### New State

Add to `CalibrationState` enum in [calibration.h](../firmware/include/calibration.h):

```cpp
CAL_STARTED,           // NEW: Show "Calibration Started" for 3s
```

### New Timeout Tracking

Add to [calibration.cpp](../firmware/src/calibration.cpp):

```cpp
static uint32_t g_wait_empty_start = 0;   // For 60s timeout
static uint32_t g_wait_full_start = 0;    // For 120s timeout
```

### New Constants

Add to [config.h](../firmware/include/config.h):

```cpp
#define CAL_STARTED_DISPLAY_DURATION    3000   // 3 seconds
#define CAL_WAIT_EMPTY_TIMEOUT          60000  // 60 seconds
#define CAL_WAIT_FULL_TIMEOUT           120000 // 120 seconds
```

## UI Changes

### Unified Information Screens

All information screens use a consistent layout with 3x size centered text, displayed for 3 seconds:

| Screen | Line 1 (y=35) | Line 2 (y=70) | Purpose |
|--------|---------------|---------------|---------|
| **Started** | "Calibration" | "Started" | Acknowledge calibration triggered |
| **Complete** | "Calibration" | "Complete" | Confirm successful calibration |
| **Error** | "Calibration" | "Error" | Indicate timeout or failure |
| **Aborted** | "Calibration" | "Aborted" | Confirm user cancelled calibration |

### Prompt Screens

1. **uiCalibrationShowEmptyPrompt()** - Empty bottle graphic (0.0 fill) with black "?"
2. **uiCalibrationShowFullPrompt()** - Full bottle graphic (1.0 fill) with white "?" (visible on black water)

Uses existing `drawBottleGraphic()` from [display.cpp](../firmware/src/display.cpp#L370-L406):
- Export function by removing `static` keyword
- Add declaration to [display.h](../firmware/include/display.h)
- Conditional text color: white "?" on filled bottles (>50%), black "?" on empty

### Modified State Handler

Update `uiCalibrationUpdateForState()` in [ui_calibration.cpp](../firmware/src/ui_calibration.cpp):

```cpp
case CAL_STARTED:         → uiCalibrationShowStarted()
case CAL_WAIT_EMPTY:      → uiCalibrationShowEmptyPrompt()
case CAL_WAIT_FULL:       → uiCalibrationShowFullPrompt()
case CAL_COMPLETE:        → uiCalibrationShowComplete()
case CAL_ERROR:           → uiCalibrationShowError()
```

Note: `uiCalibrationShowAborted()` is called directly from main.cpp abort handler, not via state machine.

## State Machine Logic Changes

### CAL_TRIGGERED
```cpp
// Change: Transition to CAL_STARTED instead of CAL_WAIT_EMPTY
g_state = CAL_STARTED;
g_state_start_time = millis();
```

### CAL_STARTED (NEW)
```cpp
// Wait 3 seconds, then transition to CAL_WAIT_EMPTY
if (millis() - g_state_start_time >= CAL_STARTED_DISPLAY_DURATION) {
    g_state = CAL_WAIT_EMPTY;
    g_wait_empty_start = millis();
}
```

### CAL_WAIT_EMPTY
```cpp
// Add timeout check at beginning
if (millis() - g_wait_empty_start >= CAL_WAIT_EMPTY_TIMEOUT) {
    g_state = CAL_ERROR;
    g_result.error_message = "Timeout - try again";
    break;
}
// Rest of existing logic...
```

### CAL_MEASURE_EMPTY
```cpp
// Change error handling from CAL_ERROR to retry
if (!measurement.valid || !measurement.stable) {
    g_state = CAL_WAIT_EMPTY;  // Retry instead of error
    g_wait_empty_start = millis();  // Reset timeout
    break;
}
// Success path...
g_state = CAL_WAIT_FULL;
g_wait_full_start = millis();
```

### CAL_WAIT_FULL
```cpp
// Add timeout check at beginning
if (millis() - g_wait_full_start >= CAL_WAIT_FULL_TIMEOUT) {
    g_state = CAL_ERROR;
    g_result.error_message = "Timeout - try again";
    break;
}
// Rest of existing logic (weight delta + stability)...
```

### CAL_MEASURE_FULL
```cpp
// Change error handling from CAL_ERROR to retry
if (!measurement.valid || !measurement.stable) {
    g_state = CAL_WAIT_FULL;  // Retry instead of error
    g_wait_full_start = millis();  // Reset timeout
    g_weight_is_stable = false;
    break;
}
// Success path unchanged...
```

## Abort Gesture Implementation

### Overview
Allow users to cancel calibration at any time by performing the inverted hold gesture (same gesture that starts calibration). This provides an easy escape mechanism if the user changes their mind or encounters issues.

### Detection Logic
Check for `GESTURE_INVERTED_HOLD` during all active calibration states:
- `CAL_STARTED`
- `CAL_WAIT_EMPTY`
- `CAL_MEASURE_EMPTY`
- `CAL_WAIT_FULL`
- `CAL_MEASURE_FULL`

### State Transitions
When inverted hold detected during calibration:
```cpp
if (current_gesture == GESTURE_INVERTED_HOLD) {
    calibrationCancel();  // Call existing cancel function
    return;  // Exit early from calibrationUpdate()
}
```

The existing `calibrationCancel()` function already handles:
- Setting state to `CAL_IDLE`
- Clearing result data
- Resetting gesture tracking
- UI update to normal mode (handled by main loop)

### Implementation Location
Add abort check at the **beginning** of each calibration state handler in `calibrationUpdate()` (before any other logic):
- This ensures immediate response to abort gesture
- Prevents any state-specific processing after abort
- Consistent behavior across all states

## Implementation Checklist

**Phase 1: Header Files** ✅ COMPLETE
- [x] Add `CAL_STARTED` to enum in [calibration.h](../firmware/include/calibration.h)
- [x] Update `calibrationGetStateName()` in [calibration.h](../firmware/include/calibration.h)
- [x] Add timeout constants to [config.h](../firmware/include/config.h)
- [x] Add new UI function declarations to [ui_calibration.h](../firmware/include/ui_calibration.h)
- [x] Export `drawBottleGraphic()` in [display.h](../firmware/include/display.h)

**Phase 2: Display Module** ✅ COMPLETE
- [x] Remove `static` from `drawBottleGraphic()` in [display.cpp](../firmware/src/display.cpp)

**Phase 3: Calibration State Machine** ✅ COMPLETE
- [x] Add timeout tracking variables to [calibration.cpp](../firmware/src/calibration.cpp)
- [x] Initialize timeout variables in `calibrationInit()`
- [x] Initialize timeout variables in `calibrationStart()`
- [x] Reset timeout variables in `calibrationCancel()`
- [x] Modify `CAL_TRIGGERED` handler to transition to `CAL_STARTED`
- [x] Add `CAL_STARTED` state handler (3s timeout → CAL_WAIT_EMPTY)
- [x] Add abort gesture check to `CAL_STARTED` handler
- [x] Add timeout check to `CAL_WAIT_EMPTY` handler
- [x] Add abort gesture check to `CAL_WAIT_EMPTY` handler
- [x] Change `CAL_MEASURE_EMPTY` error handling to retry
- [x] Add abort gesture check to `CAL_MEASURE_EMPTY` handler
- [x] Add timeout check to `CAL_WAIT_FULL` handler
- [x] Add abort gesture check to `CAL_WAIT_FULL` handler
- [x] Change `CAL_MEASURE_FULL` error handling to retry
- [x] Add abort gesture check to `CAL_MEASURE_FULL` handler
- [x] Update `calibrationGetStateName()` to handle `CAL_STARTED`

**Phase 4: UI Module** ✅ COMPLETE
- [x] Implement `uiCalibrationShowStarted()` in [ui_calibration.cpp](../firmware/src/ui_calibration.cpp)
- [x] Implement `uiCalibrationShowEmptyPrompt()` in [ui_calibration.cpp](../firmware/src/ui_calibration.cpp)
- [x] Implement `uiCalibrationShowFullPrompt()` in [ui_calibration.cpp](../firmware/src/ui_calibration.cpp)
- [x] Update `uiCalibrationUpdateForState()` switch cases

**Phase 5: Testing** ✅ COMPLETE
- [x] Build firmware (`platformio run`)
- [x] Verify IRAM usage < 95% (RAM: 7.0%, Flash: 35.9%)
- [x] Test happy path (complete calibration)
- [x] Test abort gesture - shows "Calibration / Aborted" screen correctly
- [x] Verify e-paper refreshes (5 for success path)
- [x] Verify serial logging for all state transitions

**Bug Fixes Applied:**
- [x] Abort screen not displaying - moved abort handling inside `calibrationIsActive()` block in main.cpp
- [x] Error state exit not working - moved error handling before `g_last_cal_state` update

## E-Paper Refresh Budget

**Success path**: 5 refreshes (optimized - no refresh during measurement)
1. CAL_STARTED → "Calibration / Started" (3s)
2. CAL_WAIT_EMPTY → Empty bottle with black "?"
3. CAL_WAIT_FULL → Full bottle with white "?"
4. CAL_COMPLETE → "Calibration / Complete" (3s)
5. Main screen (after 3s delay)

**Abort path**: 2 refreshes
- User inverted hold → "Calibration / Aborted" (3s) → Main screen

**Error/Timeout path**: 3 refreshes
- CAL_STARTED → prompt → timeout → "Calibration / Error" (3s) → Main screen

**Retry path**: +1 refresh per retry
- Measurement fails → stays on same prompt screen (no refresh) → retry

## Files Modified

| File | Purpose |
|------|---------|
| [calibration.h](../firmware/include/calibration.h) | Add CAL_STARTED state |
| [calibration.cpp](../firmware/src/calibration.cpp) | State machine logic, timeouts, retry, abort checks |
| [ui_calibration.h](../firmware/include/ui_calibration.h) | New UI function declarations incl. uiCalibrationShowAborted() |
| [ui_calibration.cpp](../firmware/src/ui_calibration.cpp) | Unified info screens (Started/Complete/Error/Aborted), bottle prompts |
| [config.h](../firmware/include/config.h) | Timeout constants (3s info, 60s empty, 120s full) |
| [display.h](../firmware/include/display.h) | Export drawBottleGraphic |
| [display.cpp](../firmware/src/display.cpp) | Remove static, conditional "?" color (white on filled) |
| [main.cpp](../firmware/src/main.cpp) | Abort handler with aborted screen, 3s display for all info screens |

## Notes

- Unified information screen design: "Calibration / {Status}" at 3x size, centered, 3s display
- Bottle graphics reuse existing `drawBottleGraphic()` function (40x90px)
- Conditional "?" color: white on filled bottles (>50%), black on empty (visibility fix)
- Abort requires gesture release before re-triggering (prevents immediate restart)
- Each e-paper refresh corresponds to meaningful state change (no unnecessary refreshes)
- All info screens (Started, Complete, Error, Aborted) use same 3s display duration
