# Plan: Secondary Deep Sleep Mode for Constant Motion Detection

## Executive Summary

Add an intelligent secondary deep sleep mode that uses timer-based wake instead of motion wake when the device is in constant motion (backpack scenario). This prevents battery drain from continuous wake cycles while maintaining responsiveness.

**Key Features**:
- Detects prolonged awake periods (2 minutes) and switches to timer-based sleep (60 seconds)
- Automatically returns to normal mode when motion stops
- Fully configurable via serial commands
- Shows "Zzzz.." indicator when entering extended sleep
- Preserves all existing normal sleep behavior

## Problem Statement

The current deep sleep implementation wakes on tilt motion (accelerometer interrupt), which works well for normal usage but causes excessive wake cycles when the bottle is in a backpack or constantly moving environment. Each movement triggers a wake, resets the sleep timer, and prevents the device from entering deep sleep, draining the battery.

## Current Behavior

- **Normal usage**: Pick up bottle → wake on tilt → use for ~30s → return to rest → sleep
- **Backpack scenario**: Constant movement → wake on every jolt → sleep timer resets → never sleeps → battery drain

**Existing implementation** (from code exploration):
- Sleep timeout: 30 seconds (configurable via `SET_SLEEP_TIMEOUT`)
- Wake source: Motion interrupt only (GPIO 27, LIS3DH Z-axis tilt > 0.80g)
- Timer resets: Gesture changes reset `wakeTime` in [main.cpp:888-901](firmware/src/main.cpp#L888-L901)
- No time-based wake currently implemented
- "Zzzz.." indicator available but disabled ([display.cpp:346](firmware/src/display.cpp#L346), `DISPLAY_SLEEP_INDICATOR=0`)

## Proposed Solution: Dual Deep Sleep Modes

### Mode 1: Normal Deep Sleep (existing)
- **Wake source**: Motion interrupt (accelerometer)
- **Entry condition**: 30 seconds of inactivity
- **Use case**: Bottle at rest on desk/table

### Mode 2: Extended Deep Sleep (new)
- **Wake source**: Timer-based wake (ESP32 timer wake)
- **Entry condition**: Device has been awake continuously for 2+ minutes without entering normal sleep
- **Use case**: Constant motion (backpack, car, frequent handling)

## Implementation Design

### Sleep Mode State Machine

```
Normal Operation
    ↓ (30s idle)
Normal Deep Sleep (motion wake)
    ↓ (wake on tilt)
[Back to Normal Operation]

OR if continuous motion prevents normal sleep:

Normal Operation
    ↓ (2 min continuous awake)
Extended Deep Sleep (timer wake: 60s)
    ↓ (timer expires)
Check Motion State
    ├─ Still moving → Extended Deep Sleep again
    └─ Stable/stationary → Return to Normal Operation
```

### Global State Variables

**New variables to add in main.cpp**:
```cpp
// Extended sleep tracking
unsigned long g_continuous_awake_start = 0;     // When current awake period started
bool g_in_extended_sleep_mode = false;          // Currently using extended sleep
uint32_t g_extended_sleep_timer_sec = 60;       // Timer wake duration (1 minute default)
uint32_t g_extended_sleep_threshold_sec = 120;  // Awake threshold (2 minutes default)
```

**RTC memory persistence** (survives deep sleep):
```cpp
RTC_DATA_ATTR uint32_t rtc_extended_sleep_magic = 0;
RTC_DATA_ATTR bool rtc_in_extended_sleep_mode = false;
RTC_DATA_ATTR unsigned long rtc_continuous_awake_start = 0;
```

### Detection Logic

**1. Track continuous awake time**:
- On wake from deep sleep:
  - If RTC memory valid → restore `g_continuous_awake_start`
  - If RTC memory invalid (power cycle) → set to `millis()`
- During loop: Gesture changes already reset normal sleep timer (no change needed)

**2. Check for extended sleep trigger**:
```cpp
unsigned long total_awake_time = millis() - g_continuous_awake_start;
if (!g_in_extended_sleep_mode &&
    total_awake_time >= (g_extended_sleep_threshold_sec * 1000) &&
    !calibrationIsActive()) {
    // Trigger extended sleep mode
    g_in_extended_sleep_mode = true;
    enterExtendedDeepSleep();
}
```

**3. Normal sleep still available**:
- If device becomes stationary before threshold → normal sleep with motion wake
- Extended sleep only triggers after continuous motion prevents normal sleep

### Entry Criteria for Extended Sleep

Enter extended sleep when ALL conditions met:
- Device has been continuously awake for ≥ 2 minutes (configurable)
- Not currently in extended sleep mode
- Not in calibration mode
- Normal sleep has been repeatedly delayed by motion/gesture changes

### Wake Configuration

**Normal Deep Sleep** (existing):
```cpp
esp_sleep_enable_ext0_wakeup(LIS3DH_INT_PIN, 1);  // Motion wake
```

**Extended Deep Sleep** (new):
```cpp
esp_sleep_enable_timer_wakeup(g_extended_sleep_timer_sec * 1000000ULL);  // Timer wake
// Note: Motion interrupt NOT configured - ignore motion during extended sleep
```

### Wake Handling Logic

**On wake from deep sleep**:
```cpp
esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    // Woke from timer - was in extended sleep
    Serial.println("Wake: Extended sleep timer");

    // Check if still experiencing constant motion
    if (isStillMovingConstantly()) {
        // Continue extended sleep
        Serial.println("Still moving - re-entering extended sleep");
        g_in_extended_sleep_mode = true;
        enterExtendedDeepSleep();
    } else {
        // Return to normal mode
        Serial.println("Motion stopped - returning to normal mode");
        g_in_extended_sleep_mode = false;
        g_continuous_awake_start = millis();  // Reset tracking
    }
}
else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // Woke from motion - was in normal sleep
    Serial.println("Wake: Motion interrupt");
    g_in_extended_sleep_mode = false;
    g_continuous_awake_start = millis();  // Start tracking new awake period
}
```

### Motion Detection on Timer Wake

**isStillMovingConstantly() implementation**:
```cpp
bool isStillMovingConstantly() {
    // Sample accelerometer for brief period (~500ms)
    // Check for gesture instability:
    // - Rapid gesture changes
    // - No stable gesture achieved
    // - Continuous tilt/movement detected

    // Simple approach: Check if gesture is SIDEWAYS_TILT or frequently changing
    unsigned long check_start = millis();
    GestureType initial_gesture = gesturesGetCurrentType();
    delay(500);
    GestureType final_gesture = gesturesGetCurrentType();

    // If gesture changed or is unstable (sideways tilt), still moving
    return (initial_gesture != final_gesture ||
            initial_gesture == SIDEWAYS_TILT ||
            final_gesture == SIDEWAYS_TILT);
}

## User Decisions (Finalized)

### Extended Sleep Timer Duration: **1 minute** (60 seconds)
- 60 wake cycles per hour in extended mode
- Quick responsiveness when removed from backpack
- Configurable via serial command: `SET_EXTENDED_SLEEP_TIMER seconds`

### Continuous Awake Threshold: **2 minutes**
- Tolerates brief active periods before switching to extended sleep
- Configurable via serial command: `SET_EXTENDED_SLEEP_THRESHOLD seconds`

### Wake Behavior: **Check motion, adapt**
- On timer wake: Check accelerometer for continuous motion
- If still moving → enter extended sleep again
- If stable/stationary → return to normal sleep mode
- Intelligent adaptation to environment

### Display Indicator: **Show "Zzzz.." indicator**
- Use existing `drawSleepIndicator()` function from [display.cpp:346](firmware/src/display.cpp#L346)
- Will display "Zzzz.." when entering extended sleep
- Matches existing sleep behavior (controlled by `DISPLAY_SLEEP_INDICATOR`)

### Serial Command Naming
- **Rename existing**: `SET_SLEEP_TIMEOUT` → `SET_NORMAL_SLEEP_TIMEOUT`
- **New command**: `SET_EXTENDED_SLEEP_TIMER` (duration in seconds, default 60s)
- **New command**: `SET_EXTENDED_SLEEP_THRESHOLD` (awake threshold in seconds, default 120s)

## Implementation Files to Modify

### Critical Files
- [main.cpp](firmware/src/main.cpp) - Core sleep logic, state machine
- [config.h](firmware/src/config.h) - Add thresholds and configuration constants
- [aquavate.h](firmware/include/aquavate.h) - Enum for sleep modes if needed

### Supporting Files
- [storage.cpp](firmware/src/storage.cpp) - Persist extended sleep state in RTC memory
- [serial_commands.cpp](firmware/src/serial_commands.cpp) - Add commands to configure thresholds

## Verification & Testing Plan

### Test Scenario 1: Normal Usage (Regression Test)
**Goal**: Verify existing behavior is unchanged

**Steps**:
1. Power on device (or wake from sleep)
2. Place bottle upright on desk
3. Wait 30 seconds without moving
4. **Expected**: Device enters normal deep sleep with motion wake
5. Tilt bottle to wake
6. **Expected**: Wakes immediately on motion
7. **Verify**: Serial log shows "Wake: Motion interrupt", not extended sleep mode

---

### Test Scenario 2: Extended Sleep Entry (Backpack Simulation)
**Goal**: Verify extended sleep triggers after 2 minutes of motion

**Steps**:
1. Power on device
2. Continuously move/tilt device (simulate backpack)
3. Observe serial output for 2+ minutes
4. **Expected at 2:00**: "Switching to extended sleep mode (2min threshold exceeded)"
5. **Expected**: "Entering extended deep sleep (timer wake: 60s)"
6. **Expected**: Display shows "Zzzz.." indicator
7. Device enters deep sleep
8. **Expected at 1:00 later**: Device wakes from timer (not motion)
9. **Verify**: Serial log shows "Wake: Extended sleep timer expired"

---

### Test Scenario 3: Persistent Motion (Stay in Extended Mode)
**Goal**: Verify device re-enters extended sleep when still moving

**Steps**:
1. Continue from Test Scenario 2 (in extended mode)
2. Keep device moving during timer wake check
3. **Expected**: "Still moving - re-entering extended sleep"
4. Device enters extended sleep again
5. Wait another 60 seconds
6. **Expected**: Cycle repeats while motion continues

---

### Test Scenario 4: Motion Stops (Return to Normal)
**Goal**: Verify automatic return to normal mode when stable

**Steps**:
1. Continue from Test Scenario 2/3 (in extended mode)
2. On timer wake: Place device upright and stable
3. **Expected**: "Motion stopped - returning to normal mode"
4. Device stays awake for normal operation
5. After 30 seconds of stability
6. **Expected**: Enters normal deep sleep (motion wake)
7. **Verify**: Wake source is motion interrupt, not timer

---

### Test Scenario 5: Configuration Commands
**Goal**: Verify all serial commands work correctly

**Steps**:
1. Test `SET_NORMAL_SLEEP_TIMEOUT 60` (rename of old command)
   - **Expected**: "Sleep timeout set: 60 seconds"
   - Verify persists across reboot (check NVS)
2. Test backward compatibility: `SET_SLEEP_TIMEOUT 45`
   - **Expected**: Still works (legacy name accepted)
3. Test `SET_EXTENDED_SLEEP_TIMER 120`
   - **Expected**: "Extended sleep timer set: 120 seconds"
   - Trigger extended mode, verify 120s timer used
4. Test `SET_EXTENDED_SLEEP_THRESHOLD 60`
   - **Expected**: "Extended sleep threshold set: 60 seconds"
   - Verify extended mode triggers after 60s (not 120s)
5. Test `GET_STATUS`
   - **Expected output includes**:
     - `normal_sleep_timeout_sec: 60`
     - `extended_sleep_timer_sec: 120`
     - `extended_sleep_threshold_sec: 60`
     - `in_extended_sleep_mode: false/true`
     - `continuous_awake_time_sec: X`

---

### Test Scenario 6: Calibration Interaction
**Goal**: Verify extended sleep doesn't interfere with calibration

**Steps**:
1. Trigger extended sleep mode (2 min motion)
2. Start calibration (inverted hold for 5+ seconds)
3. **Expected**: Extended sleep blocked during calibration
4. Complete calibration successfully
5. **Expected**: Continuous awake timer resets
6. **Expected**: Device returns to normal mode (not extended)

---

### Test Scenario 7: Power Cycle Persistence
**Goal**: Verify settings persist across power cycles

**Steps**:
1. Set custom values via serial:
   - `SET_EXTENDED_SLEEP_TIMER 90`
   - `SET_EXTENDED_SLEEP_THRESHOLD 180`
2. Power off device completely (remove power)
3. Power back on
4. Run `GET_STATUS`
5. **Expected**: Custom values restored from NVS

---

### Test Scenario 8: RTC Memory Persistence (Deep Sleep)
**Goal**: Verify continuous awake tracking survives deep sleep

**Steps**:
1. Wake device, start moving (backpack mode)
2. After 1 minute (before threshold): Let enter extended sleep via timer
3. Wake from timer (60s later)
4. **Expected**: Continuous awake time includes pre-sleep duration
5. **Expected**: If still at 2+ min total → stay in extended mode
6. **Verify**: RTC memory correctly persists `rtc_continuous_awake_start`

---

### Power Measurements (Hardware Testing)

**Baseline (existing)**:
- Normal deep sleep: ~10µA (motion wake)
- Wake cycle: ~30mA for 30 seconds

**New measurements needed**:
- Extended deep sleep: ~10µA (should match normal sleep)
- Timer wake cycle: ~30mA for ~1 second (motion check) then sleep
- Worst case (backpack): 60 wake cycles/hour = 30mA × 1s × 60 = 0.5mAh/hour
- Compare to current problem: Device never sleeps = 30mA continuous = 30mAh/hour

**Expected improvement**: ~60× reduction in backpack scenario power consumption

---

### Debug Serial Output Verification

**On extended sleep entry**:
```
Switching to extended sleep mode (2min threshold exceeded)
Entering extended deep sleep (timer wake: 60s)
[Display update: Zzzz..]
```

**On timer wake (still moving)**:
```
Wake: Extended sleep timer expired
Checking motion state...
Still moving - re-entering extended sleep
Entering extended deep sleep (timer wake: 60s)
```

**On timer wake (motion stopped)**:
```
Wake: Extended sleep timer expired
Checking motion state...
Motion stopped - returning to normal mode
Continuous awake timer reset
```

**On normal sleep**:
```
Sleep timeout reached (30s idle)
Entering normal deep sleep (motion wake)
[Optional: Display update: Zzzz.. if DISPLAY_SLEEP_INDICATOR enabled]
```

**During operation**:
```
Gesture: UPRIGHT_STABLE
  Water: 450.2ml (Daily: 1250ml)
  (sleep in 25s)    ← Shows normal sleep countdown

[After 2 min of motion]
Gesture: SIDEWAYS_TILT
  Continuous awake: 120s
  Extended sleep mode active
  (timer wake in 45s)    ← Shows extended sleep timer countdown
```

## Configuration Constants

**Add to config.h**:
```cpp
// Extended deep sleep configuration
#define EXTENDED_SLEEP_THRESHOLD_SEC    120     // 2 minutes continuous awake triggers extended sleep
#define EXTENDED_SLEEP_TIMER_SEC        60      // 1 minute timer wake in extended mode
#define EXTENDED_SLEEP_INDICATOR        1       // Show "Zzzz.." before extended sleep (matches normal sleep)
```

**Constants already defined**:
```cpp
#define AWAKE_DURATION_MS               30000   // 30 seconds normal sleep timeout (will be renamed)
#define DISPLAY_SLEEP_INDICATOR         0       // Currently disabled globally
```

## Detailed Implementation Steps

### Step 1: Add Configuration Constants
**File**: [config.h](firmware/src/config.h)
- Add `EXTENDED_SLEEP_THRESHOLD_SEC` (120 seconds)
- Add `EXTENDED_SLEEP_TIMER_SEC` (60 seconds)
- Add `EXTENDED_SLEEP_INDICATOR` (1 = enabled)
- Update comments for clarity

### Step 2: Add Global State Variables
**File**: [main.cpp](firmware/src/main.cpp)
- Add continuous awake tracking variables (global scope)
- Add extended sleep configuration variables
- Add RTC memory persistence variables with magic number

### Step 3: Add RTC Memory Save/Restore Functions
**File**: [main.cpp](firmware/src/main.cpp)
- `extendedSleepSaveToRTC()` - Save state before sleep
- `extendedSleepRestoreFromRTC()` - Restore state on wake
- Use magic number `0xEXT51234` for validation

### Step 4: Implement Motion Detection Helper
**File**: [main.cpp](firmware/src/main.cpp)
- Add `isStillMovingConstantly()` function
- Sample accelerometer for 500ms
- Check for gesture instability (changes or sideways tilt)

### Step 5: Add Extended Sleep Entry Function
**File**: [main.cpp](firmware/src/main.cpp)
- `enterExtendedDeepSleep()` function
- Display "Zzzz.." indicator if enabled
- Configure timer wake source
- Save RTC state
- Enter deep sleep

### Step 6: Update Wake Logic in setup()
**File**: [main.cpp](firmware/src/main.cpp) - setup() function
- Detect wakeup cause (timer vs motion)
- If timer wake → check motion → decide mode
- If motion wake → reset to normal mode
- Restore continuous awake timestamp from RTC

### Step 7: Update Sleep Decision Logic in loop()
**File**: [main.cpp](firmware/src/main.cpp) - loop() function
- Check for extended sleep threshold after normal sleep check
- If threshold exceeded → enter extended sleep
- Preserve normal sleep behavior (unchanged)
- Add debug output for both modes

### Step 8: Add Storage Functions
**File**: [storage.cpp](firmware/src/storage.cpp) + [storage.h](firmware/include/storage.h)
- `storageSaveExtendedSleepTimer(uint32_t seconds)` - Save timer duration
- `storageLoadExtendedSleepTimer(uint32_t* seconds)` - Load timer duration
- `storageSaveExtendedSleepThreshold(uint32_t seconds)` - Save threshold
- `storageLoadExtendedSleepThreshold(uint32_t* seconds)` - Load threshold
- Use NVS keys: `ext_sleep_tmr`, `ext_sleep_thr`

### Step 9: Update Serial Commands
**File**: [serial_commands.cpp](firmware/src/serial_commands.cpp)

**Rename existing command**:
- `SET_SLEEP_TIMEOUT` → `SET_NORMAL_SLEEP_TIMEOUT`
- Update help text and examples
- Maintain backward compatibility (accept both names)

**Add new commands**:
- `SET_EXTENDED_SLEEP_TIMER seconds` - Configure timer wake duration (1-3600s)
- `SET_EXTENDED_SLEEP_THRESHOLD seconds` - Configure awake threshold (30-600s)
- Add handlers: `handleSetExtendedSleepTimer()`, `handleSetExtendedSleepThreshold()`
- Update `handleHelp()` to show all three commands

### Step 10: Update GET_STATUS Command
**File**: [serial_commands.cpp](firmware/src/serial_commands.cpp)
- Add extended sleep mode status to output
- Show: `in_extended_sleep_mode`, `extended_sleep_timer_sec`, `extended_sleep_threshold_sec`
- Show continuous awake time if tracking

### Step 11: Add Debug Output
**Throughout main.cpp**:
- Log extended sleep entry: "Entering extended deep sleep (timer wake: 60s)"
- Log timer wake: "Wake: Extended sleep timer expired"
- Log mode transitions: "Switching to extended sleep mode (2min threshold exceeded)"
- Log motion check results: "Still moving - re-entering extended sleep"

---

## Implementation Summary

### Files Modified

| File | Changes | Lines Est. |
|------|---------|-----------|
| [config.h](firmware/src/config.h) | Add 3 constants for extended sleep | +15 |
| [main.cpp](firmware/src/main.cpp) | Core logic: state tracking, sleep modes, wake handling | +200 |
| [storage.cpp](firmware/src/storage.cpp) | NVS save/load for timer & threshold | +60 |
| [storage.h](firmware/include/storage.h) | Function declarations | +4 |
| [serial_commands.cpp](firmware/src/serial_commands.cpp) | Rename command, add 2 new commands, update status | +120 |

**Total estimated additions**: ~400 lines of code

### Key Implementation Points

1. **Non-Breaking Changes**:
   - All existing normal sleep behavior preserved unchanged
   - Backward compatible command naming (`SET_SLEEP_TIMEOUT` still works)
   - Extended mode only activates under specific conditions (2 min threshold)

2. **RTC Memory Usage**:
   - Add 3 RTC variables: magic number, mode flag, awake start timestamp
   - Magic number `0xEXT51234` validates RTC state after deep sleep
   - On power cycle: RTC invalid → defaults apply

3. **Power Optimization**:
   - Extended sleep uses same low-power state as normal sleep (~10µA)
   - Timer wake cycle: Quick motion check (~500ms) then back to sleep
   - No extra display updates unless configured

4. **Motion Detection Logic**:
   - Sample gesture for 500ms on timer wake
   - Check for instability: gesture changes or sideways tilt
   - Return to normal mode when stable upright detected

5. **Configuration Flexibility**:
   - Three independent settings: normal timeout, extended timer, extended threshold
   - All settings persist in NVS across power cycles
   - Serial commands for runtime adjustment without reflashing

### Critical Dependencies

**Existing functions used**:
- `gesturesGetCurrentType()` - Check current gesture ([gestures.cpp](firmware/src/gestures.cpp))
- `calibrationIsActive()` - Block sleep during calibration ([calibration.cpp](firmware/src/calibration.cpp))
- `displayUpdateUI()` - Show "Zzzz.." before sleep ([display.cpp](firmware/src/display.cpp))
- `displaySaveToRTC()` / `drinksSaveToRTC()` - Persist state ([display.cpp](firmware/src/display.cpp), [drinks.cpp](firmware/src/drinks.cpp))

**ESP32 APIs used**:
- `esp_sleep_enable_timer_wakeup(uint64_t time_in_us)` - Configure timer wake
- `esp_sleep_get_wakeup_cause()` - Detect wake source (timer vs motion)
- `esp_deep_sleep_start()` - Enter deep sleep (existing)

### Risk Mitigation

**Potential issues and solutions**:

1. **False positive extended sleep** (triggers during normal use):
   - Mitigation: 2-minute threshold is conservative
   - User can adjust via `SET_EXTENDED_SLEEP_THRESHOLD`

2. **Motion check delay on timer wake**:
   - 500ms check adds minimal latency
   - Only occurs in extended mode (backpack scenario)
   - User won't notice (device in backpack)

3. **Display power during extended sleep indicator**:
   - "Zzzz.." update costs ~30mA for 15s = 0.125mAh per sleep
   - At 60 cycles/hour = 7.5mAh/hour (still 4× better than staying awake)
   - Can disable by setting `EXTENDED_SLEEP_INDICATOR 0`

4. **RTC memory invalidation**:
   - Power cycle resets continuous awake tracking
   - Acceptable: User removing/replacing battery resets context
   - Device returns to normal mode automatically

### Success Criteria

Implementation successful when:
- [ ] Normal usage unchanged (regression test passes)
- [ ] Extended sleep triggers after 2 minutes of motion
- [ ] Device auto-returns to normal when motion stops
- [ ] All serial commands work and persist
- [ ] Power consumption in backpack reduced by ~60×
- [ ] Display shows "Zzzz.." when entering extended sleep
- [ ] Calibration not affected by extended sleep mode
- [ ] All 8 test scenarios pass
