/**
 * Aquavate - Calibration State Machine
 * Two-point calibration using empty + full 830ml bottle
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include "gestures.h"
#include "weight.h"
#include "storage.h"
#include "config.h"

// Calibration states (always defined - used in display logic)
enum CalibrationState {
    CAL_IDLE,              // Not in calibration mode
    CAL_TRIGGERED,         // User held bottle inverted - entering calibration
    CAL_STARTED,           // Show "Calibration Started" for 3s
    CAL_WAIT_EMPTY,        // Waiting for empty bottle placement
    CAL_MEASURE_EMPTY,     // Measuring empty bottle (10s)
    CAL_CONFIRM_EMPTY,     // Waiting for sideways tilt confirmation
    CAL_WAIT_FULL,         // Waiting for full bottle placement
    CAL_MEASURE_FULL,      // Measuring full bottle (10s)
    CAL_CONFIRM_FULL,      // Waiting for sideways tilt confirmation
    CAL_COMPLETE,          // Calibration successful
    CAL_ERROR,             // Error occurred
};

#if ENABLE_STANDALONE_CALIBRATION

// ==================== Standalone Calibration State Machine ====================
// Only available when ENABLE_STANDALONE_CALIBRATION is enabled
// In iOS mode, calibration is performed by the iOS app itself

// Calibration result
struct CalibrationResult {
    bool success;
    CalibrationState final_state;
    CalibrationData data;
    const char* error_message;
};

// Initialize calibration module
void calibrationInit();

// Start calibration process
void calibrationStart();

// Update calibration state machine (call regularly in loop)
// Returns current state
CalibrationState calibrationUpdate(GestureType gesture, int32_t load_reading);

// Get current calibration state
CalibrationState calibrationGetState();

// Check if calibration is active (not IDLE or COMPLETE)
bool calibrationIsActive();

// Get calibration result (call after completion)
CalibrationResult calibrationGetResult();

// Cancel calibration and return to IDLE
void calibrationCancel();

// Get state name as string (for debugging)
const char* calibrationGetStateName(CalibrationState state);

#else

// Stub function for when standalone calibration is disabled
inline const char* calibrationGetStateName(CalibrationState state) {
    return "IDLE";  // Always idle when standalone calibration disabled
}

#endif // ENABLE_STANDALONE_CALIBRATION

// ==================== Core Calibration Functions ====================
// These functions are ALWAYS available - needed for weight calculations

// Calculate scale factor from two-point calibration
// empty_adc: ADC reading of empty bottle
// full_adc: ADC reading of full bottle
// water_volume_ml: Known water volume (830ml for full bottle)
// Returns: scale_factor in ADC counts per gram
float calibrationCalculateScaleFactor(int32_t empty_adc, int32_t full_adc, float water_volume_ml);

// Convert ADC reading to water weight in grams using calibration
// current_adc: Current ADC reading
// cal: Calibration data
// Returns: Water weight in grams (negative if empty/tared)
float calibrationGetWaterWeight(int32_t current_adc, const CalibrationData& cal);

#endif // CALIBRATION_H
