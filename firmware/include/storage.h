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

#endif // STORAGE_H
