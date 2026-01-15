// storage_drinks.h - NVS storage for drink records and daily state
// Part of the Aquavate smart water bottle firmware

#ifndef STORAGE_DRINKS_H
#define STORAGE_DRINKS_H

#include <Arduino.h>
#include "drinks.h"

// CircularBufferMetadata: Tracks circular buffer state in NVS (10 bytes)
struct CircularBufferMetadata {
    uint16_t write_index;      // Next write position (0-599)
    uint16_t record_count;     // Number of records stored (0-600)
    uint32_t total_writes;     // Total lifetime writes (for debugging)
    uint16_t _reserved;        // Padding for future use
};

// NVS Storage API

/**
 * Save a drink record to the circular buffer
 * Automatically wraps around after 600 records
 *
 * @param record DrinkRecord to save
 * @return true if saved successfully
 */
bool storageSaveDrinkRecord(const DrinkRecord& record);

/**
 * Load the most recently saved drink record
 *
 * @param record Output parameter for loaded record
 * @return true if record loaded successfully
 */
bool storageLoadLastDrinkRecord(DrinkRecord& record);

/**
 * Load daily state from NVS
 *
 * @param state Output parameter for loaded state
 * @return true if state loaded successfully, false if not initialized
 */
bool storageLoadDailyState(DailyState& state);

/**
 * Save daily state to NVS
 *
 * @param state DailyState to save
 * @return true if saved successfully
 */
bool storageSaveDailyState(const DailyState& state);

/**
 * Load circular buffer metadata
 *
 * @param meta Output parameter for metadata
 * @return true if loaded successfully
 */
bool storageLoadBufferMetadata(CircularBufferMetadata& meta);

/**
 * Save circular buffer metadata
 *
 * @param meta Metadata to save
 * @return true if saved successfully
 */
bool storageSaveBufferMetadata(const CircularBufferMetadata& meta);

#endif // STORAGE_DRINKS_H
