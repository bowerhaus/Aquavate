/**
 * Aquavate - NVS Storage Module
 * Implementation
 */

#include "storage.h"
#include "config.h"
#include <Preferences.h>

// Static variables
static Preferences g_preferences;
static bool g_initialized = false;

// NVS keys
static const char* KEY_SCALE_FACTOR = "scale_factor";
static const char* KEY_EMPTY_ADC = "empty_adc";
static const char* KEY_FULL_ADC = "full_adc";
static const char* KEY_TIMESTAMP = "cal_timestamp";
static const char* KEY_VALID = "cal_valid";

bool storageInit() {
    if (g_initialized) {
        return true; // Already initialized
    }

    // Open NVS namespace in read-write mode
    bool success = g_preferences.begin(NVS_NAMESPACE, false);
    if (success) {
        g_initialized = true;
        Serial.println("Storage: NVS initialized");
    } else {
        Serial.println("Storage: Failed to initialize NVS");
    }

    return success;
}

CalibrationData storageGetEmptyCalibration() {
    CalibrationData cal;
    cal.scale_factor = 0.0f;
    cal.empty_bottle_adc = 0;
    cal.full_bottle_adc = 0;
    cal.calibration_timestamp = 0;
    cal.calibration_valid = 0;
    return cal;
}

bool storageSaveCalibration(const CalibrationData& cal) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    Serial.println("Storage: Saving calibration...");

    // Save all fields
    g_preferences.putFloat(KEY_SCALE_FACTOR, cal.scale_factor);
    g_preferences.putInt(KEY_EMPTY_ADC, cal.empty_bottle_adc);
    g_preferences.putInt(KEY_FULL_ADC, cal.full_bottle_adc);
    g_preferences.putUInt(KEY_TIMESTAMP, cal.calibration_timestamp);
    g_preferences.putUChar(KEY_VALID, cal.calibration_valid);

    Serial.print("Storage: Saved scale_factor = ");
    Serial.println(cal.scale_factor);
    Serial.print("Storage: Saved empty_adc = ");
    Serial.println(cal.empty_bottle_adc);
    Serial.print("Storage: Saved full_adc = ");
    Serial.println(cal.full_bottle_adc);
    Serial.print("Storage: Valid = ");
    Serial.println(cal.calibration_valid);

    return true;
}

bool storageLoadCalibration(CalibrationData& cal) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        cal = storageGetEmptyCalibration();
        return false;
    }

    Serial.println("Storage: Loading calibration...");

    // Load all fields (with defaults if not found)
    cal.scale_factor = g_preferences.getFloat(KEY_SCALE_FACTOR, 0.0f);
    cal.empty_bottle_adc = g_preferences.getInt(KEY_EMPTY_ADC, 0);
    cal.full_bottle_adc = g_preferences.getInt(KEY_FULL_ADC, 0);
    cal.calibration_timestamp = g_preferences.getUInt(KEY_TIMESTAMP, 0);
    cal.calibration_valid = g_preferences.getUChar(KEY_VALID, 0);

    Serial.print("Storage: Loaded scale_factor = ");
    Serial.println(cal.scale_factor);
    Serial.print("Storage: Loaded empty_adc = ");
    Serial.println(cal.empty_bottle_adc);
    Serial.print("Storage: Loaded full_adc = ");
    Serial.println(cal.full_bottle_adc);
    Serial.print("Storage: Valid = ");
    Serial.println(cal.calibration_valid);

    return (cal.calibration_valid == 1);
}

bool storageResetCalibration() {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    Serial.println("Storage: Resetting calibration...");

    // Set valid flag to 0
    g_preferences.putUChar(KEY_VALID, 0);

    // Optionally clear all values
    g_preferences.putFloat(KEY_SCALE_FACTOR, 0.0f);
    g_preferences.putInt(KEY_EMPTY_ADC, 0);
    g_preferences.putInt(KEY_FULL_ADC, 0);
    g_preferences.putUInt(KEY_TIMESTAMP, 0);

    Serial.println("Storage: Calibration reset");
    return true;
}

bool storageHasValidCalibration() {
    if (!g_initialized) {
        return false;
    }

    uint8_t valid = g_preferences.getUChar(KEY_VALID, 0);
    return (valid == 1);
}
