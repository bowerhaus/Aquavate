/**
 * Aquavate - NVS Storage Module
 * Persistent storage for calibration data
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

// Calibration data structure
struct CalibrationData {
    float scale_factor;         // ADC counts per gram
    int32_t empty_bottle_adc;   // ADC reading of empty bottle
    int32_t full_bottle_adc;    // ADC reading of full bottle (830ml)
    uint32_t calibration_timestamp; // Unix timestamp (or millis() at calibration)
    uint8_t calibration_valid;  // 0=invalid, 1=valid
};

// Initialize storage module (opens NVS namespace)
bool storageInit();

// Save calibration data to NVS
bool storageSaveCalibration(const CalibrationData& cal);

// Load calibration data from NVS
bool storageLoadCalibration(CalibrationData& cal);

// Reset calibration (mark as invalid)
bool storageResetCalibration();

// Check if valid calibration exists
bool storageHasValidCalibration();

// Get empty CalibrationData structure
CalibrationData storageGetEmptyCalibration();

// Save timezone offset to NVS (-12 to +14)
bool storageSaveTimezone(int8_t utc_offset);

// Load timezone offset from NVS (default: 0 UTC)
int8_t storageLoadTimezone();

// Save time valid flag to NVS
bool storageSaveTimeValid(bool valid);

// Load time valid flag from NVS (default: false)
bool storageLoadTimeValid();

// Save last boot timestamp to NVS (for time persistence across resets)
bool storageSaveLastBootTime(uint32_t timestamp);

// Load last boot timestamp from NVS (default: 0)
uint32_t storageLoadLastBootTime();

// Save daily intake display mode to NVS (0=human figure, 1=tumbler grid)
bool storageSaveDisplayMode(uint8_t mode);

// Load daily intake display mode from NVS (default: 0=human figure)
uint8_t storageLoadDisplayMode();

// Save sleep timeout to NVS in seconds (0=disabled, 1-300 seconds)
bool storageSaveSleepTimeout(uint32_t seconds);

// Load sleep timeout from NVS in seconds (default: 30 seconds from AWAKE_DURATION_MS)
uint32_t storageLoadSleepTimeout();

// Save extended sleep timer duration to NVS in seconds (default: 60 seconds)
bool storageSaveExtendedSleepTimer(uint32_t seconds);

// Load extended sleep timer duration from NVS in seconds (default: 60 seconds)
uint32_t storageLoadExtendedSleepTimer();

// Save extended sleep threshold to NVS in seconds (default: 120 seconds)
bool storageSaveExtendedSleepThreshold(uint32_t seconds);

// Load extended sleep threshold from NVS in seconds (default: 120 seconds)
uint32_t storageLoadExtendedSleepThreshold();

// Save shake-to-empty enabled setting to NVS
bool storageSaveShakeToEmptyEnabled(bool enabled);

// Load shake-to-empty enabled setting from NVS (default: true = enabled)
bool storageLoadShakeToEmptyEnabled();

// Save daily hydration goal to NVS in ml (1000-4000ml)
bool storageSaveDailyGoal(uint16_t goal_ml);

// Load daily hydration goal from NVS in ml (default: 2500ml)
uint16_t storageLoadDailyGoal();

// Save low battery lockout threshold to NVS (5-95%)
bool storageSaveLowBatteryThreshold(uint8_t percent);

// Load low battery lockout threshold from NVS (default: 20%)
uint8_t storageLoadLowBatteryThreshold();

#endif // STORAGE_H
