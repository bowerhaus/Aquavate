#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include "gestures.h"

// Master system states
enum SystemState {
    SYSTEM_STARTUP,           // Initial boot, sensor initialization
    SYSTEM_NORMAL_OPERATION,  // Normal drinking tracking mode
    SYSTEM_CALIBRATION,       // Two-point calibration mode
    SYSTEM_DEEP_SLEEP         // Low power mode (future)
};

// System context - sensor readings cached once per loop iteration
// This prevents stale reads and ensures all subsystems see consistent data
struct SystemContext {
    uint32_t loop_timestamp;      // millis() at start of loop
    int32_t current_adc;          // Raw ADC reading from load cell
    float current_water_ml;       // Calibrated water level in ml
    GestureState gesture_state;   // Current gesture state (includes type, acknowledgment, timestamp)
    bool gesture_consumed;        // True if gesture has been processed
    bool time_valid;              // True if RTC time is set
};

// State transition functions
void enterSystemState(SystemState new_state);
void exitSystemState(SystemState old_state);
void transitionSystemState(SystemState to_state);

// State query functions
const char* getSystemStateName(SystemState state);
SystemState getCurrentSystemState();

#endif // SYSTEM_STATE_H
