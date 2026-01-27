// storage_drinks.h - LittleFS storage for drink records, NVS for daily state
// Part of the Aquavate smart water bottle firmware
//
// Drink records use LittleFS file with fixed-size slots (true in-place overwrites).
// Daily state uses NVS with retry logic (small, rarely changes).

#ifndef STORAGE_DRINKS_H
#define STORAGE_DRINKS_H

#include <Arduino.h>
#include "drinks.h"

// CircularBufferMetadata: Tracks circular buffer state in LittleFS (14 bytes)
struct CircularBufferMetadata {
    uint16_t write_index;      // Next write position (0-599)
    uint16_t record_count;     // Number of records stored (0-600)
    uint32_t total_writes;     // Total lifetime writes (for debugging)
    uint32_t next_record_id;   // Next ID to assign (incrementing counter)
    uint16_t _reserved;        // Padding for future use
};

// ============================================================================
// LittleFS Initialization
// ============================================================================

/**
 * Initialize LittleFS for drink record storage
 * Must be called before any drink storage functions
 * Formats the filesystem on first boot after partition change
 *
 * @return true if mounted successfully
 */
bool storageInitDrinkFS();

// ============================================================================
// Drink Storage API (LittleFS)
// ============================================================================

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

/**
 * Get drink record at specific circular buffer index
 * Index 0 is the oldest record, record_count-1 is the newest
 *
 * @param index Logical index (0 = oldest, count-1 = newest)
 * @param record Output parameter for loaded record
 * @return true if record loaded successfully
 */
bool storageGetDrinkRecord(uint16_t index, DrinkRecord& record);

/**
 * Mark records as synced (set flags |= 0x01) for range
 *
 * @param start_index Logical start index (0 = oldest)
 * @param count Number of records to mark
 * @return true if marked successfully
 */
bool storageMarkSynced(uint16_t start_index, uint16_t count);

/**
 * Count unsynced records (where flags & 0x01 == 0)
 *
 * @return Number of unsynced records
 */
uint16_t storageGetUnsyncedCount();

/**
 * Get all unsynced records for sync protocol
 * Returns records in chronological order (oldest first)
 * Skips deleted records (flags & 0x04)
 *
 * @param buffer Output buffer for records
 * @param max_count Maximum records to retrieve
 * @param out_count Output parameter for actual count retrieved
 * @return true if successful
 */
bool storageGetUnsyncedRecords(DrinkRecord* buffer, uint16_t max_count, uint16_t& out_count);

/**
 * Mark a drink record as deleted by record_id (soft delete)
 * Searches circular buffer for record with matching ID and sets deleted flag
 *
 * @param record_id The unique record ID to mark as deleted
 * @return true if record found and marked, false if not found (may have rolled off)
 */
bool storageMarkDeleted(uint32_t record_id);

#endif // STORAGE_DRINKS_H
