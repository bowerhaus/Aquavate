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
static const char* KEY_TIMEZONE = "timezone";
static const char* KEY_TIME_VALID = "time_valid";
static const char* KEY_LAST_BOOT_TIME = "last_boot_time";
static const char* KEY_DISPLAY_MODE = "display_mode";
static const char* KEY_SLEEP_TIMEOUT = "sleep_timeout";
static const char* KEY_EXT_SLEEP_TMR = "ext_sleep_tmr";
static const char* KEY_EXT_SLEEP_THR = "ext_sleep_thr";
static const char* KEY_SHAKE_EMPTY_EN = "shake_empty_en";
static const char* KEY_DAILY_GOAL = "daily_goal_ml";

bool storageInit() {
    if (g_initialized) {
        return true; // Already initialized
    }

    // Open NVS namespace in read-write mode
    bool success = g_preferences.begin(NVS_NAMESPACE, false);
    if (success) {
        g_initialized = true;
        DEBUG_PRINTLN(g_debug_calibration, "Storage: NVS initialized");
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

    DEBUG_PRINTLN(g_debug_calibration, "Storage: Saving calibration...");

    // Save all fields
    g_preferences.putFloat(KEY_SCALE_FACTOR, cal.scale_factor);
    g_preferences.putInt(KEY_EMPTY_ADC, cal.empty_bottle_adc);
    g_preferences.putInt(KEY_FULL_ADC, cal.full_bottle_adc);
    g_preferences.putUInt(KEY_TIMESTAMP, cal.calibration_timestamp);
    g_preferences.putUChar(KEY_VALID, cal.calibration_valid);

    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved scale_factor = %.2f\n", cal.scale_factor);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved empty_adc = %d\n", cal.empty_bottle_adc);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved full_adc = %d\n", cal.full_bottle_adc);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Valid = %d\n", cal.calibration_valid);

    return true;
}

bool storageLoadCalibration(CalibrationData& cal) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        cal = storageGetEmptyCalibration();
        return false;
    }

    DEBUG_PRINTLN(g_debug_calibration, "Storage: Loading calibration...");

    // Load all fields (with defaults if not found)
    cal.scale_factor = g_preferences.getFloat(KEY_SCALE_FACTOR, 0.0f);
    cal.empty_bottle_adc = g_preferences.getInt(KEY_EMPTY_ADC, 0);
    cal.full_bottle_adc = g_preferences.getInt(KEY_FULL_ADC, 0);
    cal.calibration_timestamp = g_preferences.getUInt(KEY_TIMESTAMP, 0);
    cal.calibration_valid = g_preferences.getUChar(KEY_VALID, 0);

    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded scale_factor = %.2f\n", cal.scale_factor);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded empty_adc = %d\n", cal.empty_bottle_adc);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded full_adc = %d\n", cal.full_bottle_adc);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Valid = %d\n", cal.calibration_valid);

    // Sanity check: validate scale factor is within reasonable bounds
    // This catches corrupt calibration data that could cause circular dependency issues (Issue #84)
    if (cal.calibration_valid == 1) {
        if (cal.scale_factor < CALIBRATION_SCALE_FACTOR_MIN ||
            cal.scale_factor > CALIBRATION_SCALE_FACTOR_MAX) {
            Serial.printf("Storage: WARNING - scale_factor %.2f out of range [%.0f-%.0f], marking invalid\n",
                         cal.scale_factor, CALIBRATION_SCALE_FACTOR_MIN, CALIBRATION_SCALE_FACTOR_MAX);
            cal.calibration_valid = 0;
        }
    }

    return (cal.calibration_valid == 1);
}

bool storageResetCalibration() {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    DEBUG_PRINTLN(g_debug_calibration, "Storage: Resetting calibration...");

    // Set valid flag to 0
    g_preferences.putUChar(KEY_VALID, 0);

    // Optionally clear all values
    g_preferences.putFloat(KEY_SCALE_FACTOR, 0.0f);
    g_preferences.putInt(KEY_EMPTY_ADC, 0);
    g_preferences.putInt(KEY_FULL_ADC, 0);
    g_preferences.putUInt(KEY_TIMESTAMP, 0);

    DEBUG_PRINTLN(g_debug_calibration, "Storage: Calibration reset");
    return true;
}

bool storageHasValidCalibration() {
    if (!g_initialized) {
        return false;
    }

    uint8_t valid = g_preferences.getUChar(KEY_VALID, 0);
    return (valid == 1);
}

bool storageSaveTimezone(int8_t utc_offset) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    g_preferences.putChar(KEY_TIMEZONE, utc_offset);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved timezone = %d\n", utc_offset);
    return true;
}

int8_t storageLoadTimezone() {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized, using default timezone 0");
        return 0; // Default UTC
    }

    int8_t timezone = g_preferences.getChar(KEY_TIMEZONE, 0);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded timezone = %d\n", timezone);
    return timezone;
}

bool storageSaveTimeValid(bool valid) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    g_preferences.putBool(KEY_TIME_VALID, valid);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved time_valid = %s\n", valid ? "true" : "false");
    return true;
}

bool storageLoadTimeValid() {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized, using default time_valid false");
        return false;
    }

    bool valid = g_preferences.getBool(KEY_TIME_VALID, false);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded time_valid = %s\n", valid ? "true" : "false");
    return valid;
}

bool storageSaveLastBootTime(uint32_t timestamp) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    g_preferences.putUInt(KEY_LAST_BOOT_TIME, timestamp);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved last_boot_time = %u\n", timestamp);
    return true;
}

uint32_t storageLoadLastBootTime() {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized, using default last_boot_time 0");
        return 0;
    }

    uint32_t timestamp = g_preferences.getUInt(KEY_LAST_BOOT_TIME, 0);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded last_boot_time = %u\n", timestamp);
    return timestamp;
}

bool storageSaveDisplayMode(uint8_t mode) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    g_preferences.putUChar(KEY_DISPLAY_MODE, mode);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved display_mode = %d\n", mode);
    return true;
}

uint8_t storageLoadDisplayMode() {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized, using default display_mode 0");
        return 0;
    }

    uint8_t mode = g_preferences.getUChar(KEY_DISPLAY_MODE, 0);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded display_mode = %d\n", mode);
    return mode;
}

bool storageSaveSleepTimeout(uint32_t seconds) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    g_preferences.putUInt(KEY_SLEEP_TIMEOUT, seconds);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved sleep_timeout = %u seconds\n", seconds);
    return true;
}

uint32_t storageLoadSleepTimeout() {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized, using default sleep_timeout 30");
        return 30; // Default 30 seconds
    }

    uint32_t seconds = g_preferences.getUInt(KEY_SLEEP_TIMEOUT, 30); // Default 30 seconds

    // Sanity check: sleep timeout of 0 (disabled) should only be used for debugging
    // In IOS_MODE, always enforce a minimum sleep timeout to prevent battery drain
#if IOS_MODE
    if (seconds == 0) {
        Serial.println("Storage: WARNING - sleep_timeout was 0 (disabled), resetting to 30s");
        seconds = 30;
        g_preferences.putUInt(KEY_SLEEP_TIMEOUT, seconds);  // Fix the stored value
    }
#endif

    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded sleep_timeout = %u seconds\n", seconds);
    return seconds;
}

bool storageSaveExtendedSleepTimer(uint32_t seconds) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    g_preferences.putUInt(KEY_EXT_SLEEP_TMR, seconds);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved extended_sleep_timer = %u seconds\n", seconds);
    return true;
}

uint32_t storageLoadExtendedSleepTimer() {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized, using default extended_sleep_timer 60");
        return 60; // Default 60 seconds
    }

    uint32_t seconds = g_preferences.getUInt(KEY_EXT_SLEEP_TMR, 60); // Default 60 seconds
    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded extended_sleep_timer = %u seconds\n", seconds);
    return seconds;
}

bool storageSaveExtendedSleepThreshold(uint32_t seconds) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    g_preferences.putUInt(KEY_EXT_SLEEP_THR, seconds);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved extended_sleep_threshold = %u seconds\n", seconds);
    return true;
}

uint32_t storageLoadExtendedSleepThreshold() {
    if (!g_initialized) {
        Serial.printf("Storage: Not initialized, using default extended_sleep_threshold %d\n", TIME_SINCE_STABLE_THRESHOLD_SEC);
        return TIME_SINCE_STABLE_THRESHOLD_SEC;
    }

    uint32_t seconds = g_preferences.getUInt(KEY_EXT_SLEEP_THR, TIME_SINCE_STABLE_THRESHOLD_SEC);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded extended_sleep_threshold = %u seconds\n", seconds);
    return seconds;
}

bool storageSaveShakeToEmptyEnabled(bool enabled) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    g_preferences.putBool(KEY_SHAKE_EMPTY_EN, enabled);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved shake_to_empty_enabled = %s\n", enabled ? "true" : "false");
    return true;
}

bool storageLoadShakeToEmptyEnabled() {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized, using default shake_to_empty_enabled false");
        return false; // Default: disabled
    }

    bool enabled = g_preferences.getBool(KEY_SHAKE_EMPTY_EN, false); // Default: disabled
    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded shake_to_empty_enabled = %s\n", enabled ? "true" : "false");
    return enabled;
}

bool storageSaveDailyGoal(uint16_t goal_ml) {
    if (!g_initialized) {
        Serial.println("Storage: Not initialized");
        return false;
    }

    // Clamp to valid range
    if (goal_ml < DRINK_DAILY_GOAL_MIN_ML) {
        goal_ml = DRINK_DAILY_GOAL_MIN_ML;
    } else if (goal_ml > DRINK_DAILY_GOAL_MAX_ML) {
        goal_ml = DRINK_DAILY_GOAL_MAX_ML;
    }

    g_preferences.putUShort(KEY_DAILY_GOAL, goal_ml);
    DEBUG_PRINTF(g_debug_calibration, "Storage: Saved daily_goal = %dml\n", goal_ml);
    return true;
}

uint16_t storageLoadDailyGoal() {
    if (!g_initialized) {
        Serial.printf("Storage: Not initialized, using default daily_goal %dml\n", DRINK_DAILY_GOAL_DEFAULT_ML);
        return DRINK_DAILY_GOAL_DEFAULT_ML;
    }

    uint16_t goal_ml = g_preferences.getUShort(KEY_DAILY_GOAL, DRINK_DAILY_GOAL_DEFAULT_ML);

    // Validate range and fix if corrupted
    if (goal_ml < DRINK_DAILY_GOAL_MIN_ML || goal_ml > DRINK_DAILY_GOAL_MAX_ML) {
        Serial.printf("Storage: WARNING - daily_goal %dml out of range, resetting to %dml\n", goal_ml, DRINK_DAILY_GOAL_DEFAULT_ML);
        goal_ml = DRINK_DAILY_GOAL_DEFAULT_ML;
        g_preferences.putUShort(KEY_DAILY_GOAL, goal_ml);  // Fix the stored value
    }

    DEBUG_PRINTF(g_debug_calibration, "Storage: Loaded daily_goal = %dml\n", goal_ml);
    return goal_ml;
}
