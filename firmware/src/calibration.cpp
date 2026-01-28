/**
 * Aquavate - Calibration State Machine
 * Implementation
 */

#include "calibration.h"
#include "config.h"
#include "gestures.h"

#if ENABLE_STANDALONE_CALIBRATION

// ==================== Standalone Calibration State Machine ====================
// This section is only compiled when ENABLE_STANDALONE_CALIBRATION is enabled.
// In iOS mode, calibration is performed via the iOS app instead.

// Static variables
static CalibrationState g_state = CAL_IDLE;
static CalibrationResult g_result;
static int32_t g_empty_adc = 0;
static int32_t g_full_adc = 0;
static uint32_t g_state_start_time = 0;

// Weight stability tracking for CAL_WAIT_FULL state
static uint32_t g_weight_stable_start = 0;
static int32_t g_last_stable_weight = 0;
static bool g_weight_is_stable = false;

// Timeout tracking for user prompts
static uint32_t g_wait_empty_start = 0;   // For 60s timeout
static uint32_t g_wait_full_start = 0;    // For 120s timeout

// External references (set by main.cpp or UI module)
extern Adafruit_NAU7802 nau;

void calibrationInit() {
    g_state = CAL_IDLE;
    g_result.success = false;
    g_result.final_state = CAL_IDLE;
    g_result.error_message = nullptr;
    g_result.data = storageGetEmptyCalibration();
    g_empty_adc = 0;
    g_full_adc = 0;
    g_weight_stable_start = 0;
    g_last_stable_weight = 0;
    g_weight_is_stable = false;
    g_wait_empty_start = 0;
    g_wait_full_start = 0;
}

void calibrationStart() {
    Serial.println("Calibration: Starting...");

    // Enable accelerometer-only stability detection for calibration
    // This bypasses ml-based weight stability to allow calibration with corrupt/missing calibration data
    gesturesSetCalibrationMode(true);

    g_state = CAL_TRIGGERED;
    g_state_start_time = millis();
    g_empty_adc = 0;
    g_full_adc = 0;
    g_result.success = false;
    g_result.error_message = nullptr;
    g_weight_stable_start = 0;
    g_last_stable_weight = 0;
    g_weight_is_stable = false;
    g_wait_empty_start = 0;
    g_wait_full_start = 0;
}

CalibrationState calibrationUpdate(GestureType gesture, int32_t load_reading) {
    // State machine
    switch (g_state) {
        case CAL_IDLE:
            // Not in calibration mode - external code should check for trigger gesture
            break;

        case CAL_TRIGGERED:
            // Just entered calibration mode - show "Calibration Started" screen
            Serial.println("Calibration: Triggered - showing started screen");
            g_state = CAL_STARTED;
            g_state_start_time = millis();
            break;

        case CAL_STARTED:
            // Check for abort gesture (inverted hold)
            if (gesture == GESTURE_INVERTED_HOLD) {
                Serial.println("Calibration: Aborted by user (inverted hold during STARTED)");
                calibrationCancel();
                return g_state;
            }

            // Show "Calibration Started" for 3 seconds
            if (millis() - g_state_start_time >= CAL_STARTED_DISPLAY_DURATION) {
                Serial.println("Calibration: Starting - waiting for empty bottle");
                g_state = CAL_WAIT_EMPTY;
                g_wait_empty_start = millis();
            }
            break;

        case CAL_WAIT_EMPTY:
            // Check for abort gesture (inverted hold)
            if (gesture == GESTURE_INVERTED_HOLD) {
                Serial.println("Calibration: Aborted by user (inverted hold during WAIT_EMPTY)");
                calibrationCancel();
                return g_state;
            }

            // Waiting for user to place empty bottle upright
            // Check for timeout first
            if (millis() - g_wait_empty_start >= CAL_WAIT_EMPTY_TIMEOUT) {
                Serial.println("Calibration: Empty bottle timeout");
                g_state = CAL_ERROR;
                g_result.error_message = "Timeout - try again";
                gesturesSetCalibrationMode(false);  // Reset on error
                break;
            }

            if (gesture == GESTURE_UPRIGHT_STABLE) {
                Serial.println("Calibration: Empty bottle detected - measuring...");
                g_state = CAL_MEASURE_EMPTY;
                g_state_start_time = millis();
            }
            break;

        case CAL_MEASURE_EMPTY:
            // Check for abort gesture (inverted hold)
            if (gesture == GESTURE_INVERTED_HOLD) {
                Serial.println("Calibration: Aborted by user (inverted hold during MEASURE_EMPTY)");
                calibrationCancel();
                return g_state;
            }

            // Measure empty bottle weight
            {
                Serial.println("Calibration: Taking empty measurement...");
                WeightMeasurement measurement = weightMeasureStable();

                if (!measurement.valid || !measurement.stable) {
                    Serial.println("Calibration: Empty measurement failed - retry");
                    g_state = CAL_WAIT_EMPTY;
                    g_wait_empty_start = millis();  // Reset timeout
                    break;
                }

                g_empty_adc = measurement.raw_adc;
                Serial.print("Calibration: Empty ADC = ");
                Serial.println(g_empty_adc);

                // Skip confirmation - go directly to filling
                Serial.println("Calibration: Empty recorded - fill bottle to 830ml");
                g_state = CAL_WAIT_FULL;
                g_wait_full_start = millis();
                // Reset weight stability tracking
                g_weight_stable_start = 0;
                g_last_stable_weight = 0;
                g_weight_is_stable = false;
            }
            break;

        case CAL_CONFIRM_EMPTY:
            // DEPRECATED - No longer used (skipped after MEASURE_EMPTY)
            // Kept for backward compatibility with state enum
            Serial.println("Calibration: Unexpected CONFIRM_EMPTY state");
            g_state = CAL_WAIT_FULL;
            break;

        case CAL_WAIT_FULL:
            // Check for abort gesture (inverted hold)
            if (gesture == GESTURE_INVERTED_HOLD) {
                Serial.println("Calibration: Aborted by user (inverted hold during WAIT_FULL)");
                calibrationCancel();
                return g_state;
            }

            // Waiting for user to fill bottle and place upright with stable weight
            // Check for timeout first
            if (millis() - g_wait_full_start >= CAL_WAIT_FULL_TIMEOUT) {
                Serial.println("Calibration: Full bottle timeout");
                g_state = CAL_ERROR;
                g_result.error_message = "Timeout - try again";
                gesturesSetCalibrationMode(false);  // Reset on error
                break;
            }

            // Check if bottle is upright and stable (gesture)
            if (gesture == GESTURE_UPRIGHT_STABLE) {
                // Check if weight has changed significantly from empty
                int32_t current_reading = load_reading;
                int32_t weight_delta = abs(current_reading - g_empty_adc);

                // Expect at least 300,000 ADC units difference for 830ml of water
                // (based on typical scale factors of ~400-500 ADC/gram)
                // 830ml = 830g → 830g * 400 ADC/g = 332,000 ADC units minimum
                if (weight_delta > 300000) {
                    // Weight has changed significantly - now check for stability
                    // Weight is stable if it hasn't changed by more than 5000 ADC units
                    int32_t weight_change = abs(current_reading - g_last_stable_weight);

                    if (weight_change < 5000) {
                        // Weight is stable - check if it's been stable long enough
                        if (!g_weight_is_stable) {
                            // Just became stable - start timer
                            g_weight_is_stable = true;
                            g_weight_stable_start = millis();
                            g_last_stable_weight = current_reading;
                            Serial.println("Calibration: Weight is now stable, waiting 5 seconds...");
                        } else {
                            // Already stable - check duration
                            uint32_t stable_duration = millis() - g_weight_stable_start;
                            if (stable_duration >= 5000) {
                                // Stable for 5 seconds - proceed to measurement
                                Serial.print("Calibration: Weight stable for 5s (delta=");
                                Serial.print(weight_delta);
                                Serial.println(") - taking full measurement...");
                                g_state = CAL_MEASURE_FULL;
                                g_state_start_time = millis();
                                // Reset stability tracking
                                g_weight_is_stable = false;
                            } else {
                                // Still waiting for 5 seconds
                                static uint32_t last_progress_print = 0;
                                if (millis() - last_progress_print > 1000) {
                                    Serial.print("Calibration: Stable for ");
                                    Serial.print(stable_duration / 1000);
                                    Serial.println("s...");
                                    last_progress_print = millis();
                                }
                            }
                        }
                    } else {
                        // Weight changed too much - reset stability timer
                        if (g_weight_is_stable) {
                            Serial.println("Calibration: Weight changed, restarting stability timer");
                        }
                        g_weight_is_stable = false;
                        g_last_stable_weight = current_reading;
                    }
                } else {
                    // Weight not changed enough yet - bottle still being filled or not filled enough
                    g_weight_is_stable = false;
                    g_last_stable_weight = current_reading;

                    static uint32_t last_status_print = 0;
                    if (millis() - last_status_print > 2000) {
                        Serial.print("Calibration: Waiting for bottle to be filled (current delta=");
                        Serial.print(weight_delta);
                        Serial.println(")");
                        last_status_print = millis();
                    }
                }
            } else {
                // Not upright stable - reset weight stability tracking
                if (g_weight_is_stable) {
                    Serial.println("Calibration: Bottle moved, restarting stability timer");
                }
                g_weight_is_stable = false;
            }
            break;

        case CAL_MEASURE_FULL:
            // Check for abort gesture (inverted hold)
            if (gesture == GESTURE_INVERTED_HOLD) {
                Serial.println("Calibration: Aborted by user (inverted hold during MEASURE_FULL)");
                calibrationCancel();
                return g_state;
            }

            // Measure full bottle weight
            {
                Serial.println("Calibration: Taking full measurement...");
                WeightMeasurement measurement = weightMeasureStable();

                if (!measurement.valid || !measurement.stable) {
                    Serial.println("Calibration: Full measurement failed - retry");
                    g_state = CAL_WAIT_FULL;
                    g_wait_full_start = millis();  // Reset timeout
                    g_weight_is_stable = false;
                    break;
                }

                g_full_adc = measurement.raw_adc;
                Serial.print("Calibration: Full ADC = ");
                Serial.println(g_full_adc);

                // Calculate scale factor immediately (no confirmation needed)
                Serial.println("Calibration: Calculating scale factor...");

                float scale_factor = calibrationCalculateScaleFactor(
                    g_empty_adc,
                    g_full_adc,
                    CALIBRATION_BOTTLE_VOLUME_ML
                );

                Serial.print("Calibration: Scale factor = ");
                Serial.print(scale_factor);
                Serial.println(" ADC/g");

                // Validate scale factor
                if (scale_factor <= 0.0f || scale_factor > 1000.0f) {
                    Serial.println("Calibration: Invalid scale factor");
                    g_state = CAL_ERROR;
                    g_result.error_message = "Invalid scale factor";
                    gesturesSetCalibrationMode(false);  // Reset on error
                    break;
                }

                // Create calibration data
                CalibrationData cal;
                cal.scale_factor = scale_factor;
                cal.empty_bottle_adc = g_empty_adc;
                cal.full_bottle_adc = g_full_adc;
                cal.calibration_timestamp = millis();
                cal.calibration_valid = 1;

                // Save to NVS
                if (!storageSaveCalibration(cal)) {
                    Serial.println("Calibration: Failed to save");
                    g_state = CAL_ERROR;
                    g_result.error_message = "Save failed";
                    gesturesSetCalibrationMode(false);  // Reset on error
                    break;
                }

                // Success!
                g_result.success = true;
                g_result.data = cal;
                g_result.final_state = CAL_COMPLETE;
                g_state = CAL_COMPLETE;
                g_state_start_time = millis();

                // Disable calibration mode - calibration complete
                gesturesSetCalibrationMode(false);

                Serial.println("Calibration: Complete!");
            }
            break;

        case CAL_CONFIRM_FULL:
            // DEPRECATED - No longer used (skipped after MEASURE_FULL)
            // Kept for backward compatibility with state enum
            Serial.println("Calibration: Unexpected CONFIRM_FULL state");
            g_state = CAL_COMPLETE;
            break;

        case CAL_COMPLETE:
            // Calibration complete - stay in this state
            // Main.cpp will handle timeout and return to IDLE
            break;

        case CAL_ERROR:
            // Error occurred - stay in this state until reset
            break;
    }

    return g_state;
}

CalibrationState calibrationGetState() {
    return g_state;
}

bool calibrationIsActive() {
    return (g_state != CAL_IDLE && g_state != CAL_COMPLETE);
}

CalibrationResult calibrationGetResult() {
    g_result.final_state = g_state;
    return g_result;
}

void calibrationCancel() {
    Serial.println("Calibration: Cancelled");

    // Disable calibration mode - return to normal weight stability detection
    gesturesSetCalibrationMode(false);

    g_state = CAL_IDLE;
    g_empty_adc = 0;
    g_full_adc = 0;
    g_weight_stable_start = 0;
    g_last_stable_weight = 0;
    g_weight_is_stable = false;
    g_wait_empty_start = 0;
    g_wait_full_start = 0;
}

const char* calibrationGetStateName(CalibrationState state) {
    switch (state) {
        case CAL_IDLE: return "IDLE";
        case CAL_TRIGGERED: return "TRIGGERED";
        case CAL_STARTED: return "STARTED";
        case CAL_WAIT_EMPTY: return "WAIT_EMPTY";
        case CAL_MEASURE_EMPTY: return "MEASURE_EMPTY";
        case CAL_CONFIRM_EMPTY: return "CONFIRM_EMPTY";
        case CAL_WAIT_FULL: return "WAIT_FULL";
        case CAL_MEASURE_FULL: return "MEASURE_FULL";
        case CAL_CONFIRM_FULL: return "CONFIRM_FULL";
        case CAL_COMPLETE: return "COMPLETE";
        case CAL_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

#endif // ENABLE_STANDALONE_CALIBRATION

// ==================== Core Calibration Functions ====================
// These functions are ALWAYS compiled - needed for weight calculations and BLE.

float calibrationCalculateScaleFactor(int32_t empty_adc, int32_t full_adc, float water_volume_ml) {
    // Bottle weight cancels out in the difference:
    // - empty_adc = bottle_weight * scale + offset
    // - full_adc = (bottle_weight + water_weight) * scale + offset
    // - full_adc - empty_adc = water_weight * scale
    // - scale_factor = (full_adc - empty_adc) / water_weight
    //
    // For water: 1ml = 1g, so water_volume_ml = water_weight_grams

    int32_t adc_diff = full_adc - empty_adc;

    if (adc_diff <= 0) {
        Serial.println("Calibration: Error - Full ADC must be greater than empty ADC");
        return 0.0f;
    }

    float water_weight_grams = water_volume_ml * CALIBRATION_WATER_DENSITY;
    float scale_factor = (float)adc_diff / water_weight_grams;

    return scale_factor;
}

float calibrationGetWaterWeight(int32_t current_adc, const CalibrationData& cal) {
    if (cal.calibration_valid != 1 || cal.scale_factor <= 0.0f) {
        return 0.0f;
    }

    // Calculate water weight from current ADC reading
    int32_t adc_from_empty = current_adc - cal.empty_bottle_adc;
    float water_grams = (float)adc_from_empty / cal.scale_factor;

    // For water, grams = ml
    return water_grams;
}
