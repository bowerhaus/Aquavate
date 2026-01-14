# Plan 010: Deep Sleep Mode Reinstatement

**Status:** ✅ Complete
**Created:** 2026-01-14
**Completed:** 2026-01-14
**Branch:** `deep-sleep-reinstatement`

## Overview
Reinstate deep sleep functionality with 30-second awake duration, add "Zzzz" sleep indicator in top-left corner of display, and prevent sleep during calibration mode.

## Requirements Summary
1. **Deep sleep interval:** 30 seconds awake time (AWAKE_DURATION_MS already set correctly)
2. **Zzzz indicator:** Display "Zzzz" just before entering sleep, remove immediately on wake
3. **Display behavior:** Always force display update when showing/hiding Zzzz
4. **Calibration protection:** System cannot enter sleep while calibration is active
5. **Wake timer reset:** Reset 30s timer when exiting calibration (success or error)
6. **Debug control:** Serial command to set sleep timeout (0 = disable sleep for debugging)

## Implementation Steps

### 1. Extend DisplayState to Track Sleep Status
**File:** `firmware/include/display.h`

Add new field to DisplayState struct:
```cpp
struct DisplayState {
    // ... existing fields ...
    bool sleeping;  // True when showing Zzzz indicator
};
```

### 2. Add drawSleepIndicator() Function
**File:** `firmware/src/display.cpp`

Add static drawing function after existing icon functions (around line 560):
```cpp
static void drawSleepIndicator(int x, int y, bool sleeping) {
    if (sleeping) {
        g_display->setTextSize(1);
        g_display->setTextColor(EPD_BLACK);
        g_display->setCursor(x, y);
        g_display->print("Zzzz");
    }
}
```

Position: x=5, y=5 (top-left corner)

### 3. Update displayUpdate() to Accept Sleep Parameter
**File:** `firmware/src/display.cpp`

Modify function signature (line 517):
```cpp
void displayUpdate(float water_ml, uint16_t daily_total_ml,
                   uint8_t hour, uint8_t minute,
                   uint8_t battery_percent, bool sleeping)
```

Add sleep state change detection (line ~565):
```cpp
bool sleep_changed = (g_display_state.sleeping != sleeping);
g_display_state.sleeping = sleeping;
```

Return true from displayNeedsUpdate if sleep state changed.

### 4. Update drawMainScreen() to Render Zzzz
**File:** `firmware/src/display.cpp`

Add call to drawSleepIndicator after battery icon (around line 615):
```cpp
drawBatteryIcon(220, 5, g_display_state.battery_percent);
drawSleepIndicator(5, 5, g_display_state.sleeping);
```

### 5. Update Display API Header
**File:** `firmware/include/display.h`

Update function signatures:
```cpp
void displayUpdate(float water_ml, uint16_t daily_total_ml,
                   uint8_t hour, uint8_t minute,
                   uint8_t battery_percent, bool sleeping);

void displayForceUpdate(float water_ml, uint16_t daily_total_ml,
                       uint8_t hour, uint8_t minute,
                       uint8_t battery_percent, bool sleeping);
```

### 6. Modify Main Loop Sleep Logic
**File:** `firmware/src/main.cpp`

**Step 6a:** Add global sleep timeout variable (near line 60):
```cpp
uint32_t g_sleep_timeout_ms = AWAKE_DURATION_MS;  // Default 30 seconds
```

**Step 6b:** Un-comment and enhance deep sleep check block (lines 684-697):
```cpp
// Check if it's time to sleep (only if sleep enabled)
if (g_sleep_timeout_ms > 0 && millis() - wakeTime >= g_sleep_timeout_ms) {
    // Prevent sleep during active calibration
    if (calibrationIsActive()) {
        Serial.println("Sleep blocked - calibration in progress");
        wakeTime = millis(); // Reset timer to prevent immediate sleep after calibration
    } else {
        // Show Zzzz indicator before sleeping
        Serial.println("Displaying Zzzz indicator...");
        displayUpdate(weight_reading, g_daily_intake_ml,
                     time_hour, time_minute, battery_pct, true);

        delay(1000); // Brief pause to ensure display updates

        if (lisReady) {
            // Clear any pending interrupt before sleep
            readAccelReg(LIS3DH_REG_INT1SRC);
            Serial.print("INT pin before sleep: ");
            Serial.println(digitalRead(LIS3DH_INT_PIN));
        }
        enterDeepSleep();
    }
}
```

**Step 6c:** Update all displayUpdate() calls to pass sleeping=false parameter

### 7. Reset Wake Timer After Calibration Exit
**File:** `firmware/src/main.cpp`

Add wake timer reset when exiting calibration (lines 542-578):
```cpp
// In CAL_COMPLETE block:
calibrationCancel();
wakeTime = millis(); // Reset sleep timer

// In CAL_ERROR block:
calibrationCancel();
wakeTime = millis(); // Reset sleep timer
```

### 8. Update displayForceUpdate() Signature
**File:** `firmware/src/display.cpp`

Add sleeping parameter to match displayUpdate():
```cpp
void displayForceUpdate(float water_ml, uint16_t daily_total_ml,
                       uint8_t hour, uint8_t minute,
                       uint8_t battery_percent, bool sleeping) {
    g_display_state.initialized = false;
    displayUpdate(water_ml, daily_total_ml, hour, minute, battery_percent, sleeping);
}
```

### 9. Add Runtime Sleep Timeout Control (Debug Command)
**Files:**
- `firmware/src/main.cpp` - Global variable already added in step 6a
- `firmware/src/serial_commands.cpp` - Add command handler

**Step 9a:** Add extern declaration in serial_commands.cpp (around line 35):
```cpp
extern uint32_t g_sleep_timeout_ms;
```

**Step 9b:** Add command handler (after handleSetDisplayMode, around line 612):
```cpp
// Handle SET_SLEEP_TIMEOUT command
// Format: SET_SLEEP_TIMEOUT seconds (0 = disable sleep)
static void handleSetSleepTimeout(char* args) {
    int seconds;
    if (!parseInt(args, seconds)) {
        Serial.println("ERROR: Invalid timeout");
        Serial.println("Usage: SET_SLEEP_TIMEOUT seconds");
        Serial.println("  0 = Disable sleep (debug mode)");
        Serial.println("  1-300 = Sleep after N seconds");
        Serial.println("Examples:");
        Serial.println("  SET_SLEEP_TIMEOUT 30  → Sleep after 30 seconds (default)");
        Serial.println("  SET_SLEEP_TIMEOUT 0   → Never sleep (for debugging)");
        return;
    }

    if (seconds < 0 || seconds > 300) {
        Serial.println("ERROR: Timeout must be 0-300 seconds");
        return;
    }

    g_sleep_timeout_ms = (uint32_t)seconds * 1000;

    if (seconds == 0) {
        Serial.println("Sleep DISABLED (debug mode)");
        Serial.println("Device will never enter deep sleep");
    } else {
        Serial.printf("Sleep timeout set: %d seconds\n", seconds);
        Serial.println("Note: Does NOT persist across reboots");
    }
}
```

**Step 9c:** Add command to processCommand() switch (around line 746):
```cpp
} else if (strcmp(cmd, "SET_SLEEP_TIMEOUT") == 0) {
    handleSetSleepTimeout(args);
```

**Step 9d:** Update help text (around line 767):
```cpp
Serial.println("\nPower Management:");
Serial.println("  SET_SLEEP_TIMEOUT sec - Set sleep timeout (0=disable, default=30)");
```

## Critical Files Modified

1. `firmware/include/display.h` - DisplayState struct + function signatures
2. `firmware/src/display.cpp` - Zzzz rendering, state tracking, update logic
3. `firmware/src/main.cpp` - Sleep enablement, calibration blocking, timer reset, Zzzz display calls
4. `firmware/src/serial_commands.cpp` - SET_SLEEP_TIMEOUT command handler

## Testing & Verification

### Serial Monitor Tests
1. Wake from deep sleep - verify no Zzzz on wake
2. Sleep countdown - verify Zzzz appears at ~29s
3. Calibration blocking - verify sleep blocked during calibration
4. Wake timer reset - verify 30s reset after calibration
5. Debug command `SET_SLEEP_TIMEOUT 0` - verify sleep disabled
6. Debug command `SET_SLEEP_TIMEOUT 10` - verify custom timeout
7. Debug command `SET_SLEEP_TIMEOUT 30` - verify default restored

### Hardware Tests
1. Deep sleep current draw <100µA
2. Wake-on-tilt response <1 second
3. Calibration workflow uninterrupted
4. Battery life <5% drain per day

## Success Criteria

- ✅ Device enters deep sleep after 30 seconds of inactivity
- ✅ "Zzzz" appears in top-left corner just before sleep (optional, DISPLAY_SLEEP_INDICATOR=0)
- ✅ "Zzzz" disappears immediately on wake
- ✅ Sleep blocked during active calibration
- ✅ Wake timer resets after calibration exit
- ✅ RTC memory state persistence (display + drinks)
- ✅ Welcome screen only on power-on/reset (not on wake)
- ✅ Display updates forced when showing/hiding Zzzz
- ✅ Serial command `SET_SLEEP_TIMEOUT` with NVS persistence
- ✅ 'T' test command for interrupt diagnostics

## Known Limitations

**Wake-on-tilt:** Currently only reliably triggers on left/right tilts. Forward/backward tilts may not consistently wake the device from deep sleep. This is a hardware interrupt configuration issue with the LIS3DH accelerometer that requires further investigation. The original Z-axis low interrupt (INT1_CFG=0x02) works for detecting significant tilts but doesn't catch all orientations equally.

## Related Documents
- [Plans/009-smart-display-state-tracking.md](009-smart-display-state-tracking.md) - Display state architecture
- [Plans/005-standalone-calibration-mode.md](005-standalone-calibration-mode.md) - Calibration state machine
- [firmware/src/config.h](../firmware/src/config.h) - AWAKE_DURATION_MS definition
