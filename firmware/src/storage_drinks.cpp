// storage_drinks.cpp - NVS storage implementation for drink tracking
// Part of the Aquavate smart water bottle firmware

#include <Preferences.h>
#include "storage_drinks.h"
#include "config.h"

// Helper function to generate NVS key for drink record at given index
static void getDrinkRecordKey(uint16_t index, char* key_buf, size_t buf_size) {
    snprintf(key_buf, buf_size, "drink_%03d", index % DRINK_MAX_RECORDS);
}

bool storageSaveDrinkRecord(const DrinkRecord& record) {
    // Load or initialize metadata
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta)) {
        // Initialize new buffer
        meta.write_index = 0;
        meta.record_count = 0;
        meta.total_writes = 0;
        meta._reserved = 0;
    }

    // Generate key for current write position
    char key[16];
    getDrinkRecordKey(meta.write_index, key, sizeof(key));

    // Save drink record
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("ERROR: Failed to open NVS for drink record write");
        return false;
    }

    size_t written = prefs.putBytes(key, &record, sizeof(DrinkRecord));
    prefs.end();

    if (written != sizeof(DrinkRecord)) {
        Serial.println("ERROR: Failed to write drink record to NVS");
        return false;
    }

    // Update metadata
    meta.write_index = (meta.write_index + 1) % DRINK_MAX_RECORDS;
    if (meta.record_count < DRINK_MAX_RECORDS) {
        meta.record_count++;
    }
    meta.total_writes++;

    if (!storageSaveBufferMetadata(meta)) {
        Serial.println("WARNING: Drink record saved but metadata update failed");
        return false;
    }

    Serial.print("Drink record saved to ");
    Serial.print(key);
    Serial.print(" (total: ");
    Serial.print(meta.record_count);
    Serial.println(")");

    return true;
}

bool storageLoadLastDrinkRecord(DrinkRecord& record) {
    // Load metadata to find last written index
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        Serial.println("No drink records in storage");
        return false;
    }

    // Calculate last written index (write_index points to NEXT position)
    uint16_t last_index = (meta.write_index == 0)
        ? (DRINK_MAX_RECORDS - 1)
        : (meta.write_index - 1);

    // Generate key and load record
    char key[16];
    getDrinkRecordKey(last_index, key, sizeof(key));

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        Serial.println("ERROR: Failed to open NVS for drink record read");
        return false;
    }

    size_t read_size = prefs.getBytes(key, &record, sizeof(DrinkRecord));
    prefs.end();

    if (read_size != sizeof(DrinkRecord)) {
        Serial.println("ERROR: Failed to read last drink record");
        return false;
    }

    return true;
}

bool storageLoadDailyState(DailyState& state) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        Serial.println("ERROR: Failed to open NVS for daily state read");
        return false;
    }

    size_t read_size = prefs.getBytes("daily_state", &state, sizeof(DailyState));
    prefs.end();

    if (read_size != sizeof(DailyState)) {
        Serial.println("Daily state not found (first run)");
        return false;
    }

    return true;
}

bool storageSaveDailyState(const DailyState& state) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("ERROR: Failed to open NVS for daily state write");
        return false;
    }

    size_t written = prefs.putBytes("daily_state", &state, sizeof(DailyState));
    prefs.end();

    if (written != sizeof(DailyState)) {
        Serial.println("ERROR: Failed to write daily state to NVS");
        return false;
    }

    return true;
}

bool storageLoadBufferMetadata(CircularBufferMetadata& meta) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        Serial.println("ERROR: Failed to open NVS for buffer metadata read");
        return false;
    }

    size_t read_size = prefs.getBytes("drink_meta", &meta, sizeof(CircularBufferMetadata));
    prefs.end();

    if (read_size != sizeof(CircularBufferMetadata)) {
        Serial.println("Buffer metadata not found (first run)");
        return false;
    }

    return true;
}

bool storageSaveBufferMetadata(const CircularBufferMetadata& meta) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("ERROR: Failed to open NVS for buffer metadata write");
        return false;
    }

    size_t written = prefs.putBytes("drink_meta", &meta, sizeof(CircularBufferMetadata));
    prefs.end();

    if (written != sizeof(CircularBufferMetadata)) {
        Serial.println("ERROR: Failed to write buffer metadata to NVS");
        return false;
    }

    return true;
}

bool storageGetDrinkRecord(uint16_t index, DrinkRecord& record) {
    // Load metadata to validate index
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        Serial.println("No drink records in storage");
        return false;
    }

    // Validate logical index
    if (index >= meta.record_count) {
        Serial.print("ERROR: Invalid record index: ");
        Serial.print(index);
        Serial.print(" (max: ");
        Serial.print(meta.record_count - 1);
        Serial.println(")");
        return false;
    }

    // Calculate physical NVS index
    // If buffer is full (record_count == 600), oldest record is at write_index
    // If buffer is partial, oldest record is at index 0
    uint16_t physical_index;
    if (meta.record_count < DRINK_MAX_RECORDS) {
        // Buffer not full yet, records start at 0
        physical_index = index;
    } else {
        // Buffer full, oldest record is at write_index
        physical_index = (meta.write_index + index) % DRINK_MAX_RECORDS;
    }

    // Generate key and load record
    char key[16];
    getDrinkRecordKey(physical_index, key, sizeof(key));

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        Serial.println("ERROR: Failed to open NVS for drink record read");
        return false;
    }

    size_t read_size = prefs.getBytes(key, &record, sizeof(DrinkRecord));
    prefs.end();

    if (read_size != sizeof(DrinkRecord)) {
        Serial.print("ERROR: Failed to read drink record at index ");
        Serial.println(index);
        return false;
    }

    return true;
}

bool storageMarkSynced(uint16_t start_index, uint16_t count) {
    (void)start_index; // Not used - we mark the first N unsynced records

    // Load metadata
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        Serial.println("No drink records to mark synced");
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("ERROR: Failed to open NVS for mark synced");
        return false;
    }

    // Iterate through all records and mark the first 'count' unsynced ones as synced
    // This matches the logic in storageGetUnsyncedRecords
    uint16_t marked = 0;
    for (uint16_t i = 0; i < meta.record_count && marked < count; i++) {
        // Calculate physical index (same logic as storageGetUnsyncedRecords)
        uint16_t physical_index;
        if (meta.record_count < DRINK_MAX_RECORDS) {
            physical_index = i;
        } else {
            physical_index = (meta.write_index + i) % DRINK_MAX_RECORDS;
        }

        // Load record
        char key[16];
        getDrinkRecordKey(physical_index, key, sizeof(key));

        DrinkRecord record;
        size_t read_size = prefs.getBytes(key, &record, sizeof(DrinkRecord));
        if (read_size != sizeof(DrinkRecord)) {
            Serial.print("WARNING: Failed to read record at physical index ");
            Serial.println(physical_index);
            continue;
        }

        // Only mark if currently unsynced (bit 0 not set)
        if ((record.flags & 0x01) == 0) {
            // Set synced flag
            record.flags |= 0x01;

            // Write back
            size_t written = prefs.putBytes(key, &record, sizeof(DrinkRecord));
            if (written != sizeof(DrinkRecord)) {
                Serial.print("WARNING: Failed to write synced flag at physical index ");
                Serial.println(physical_index);
            } else {
                marked++;
            }
        }
    }

    prefs.end();

    Serial.print("Marked ");
    Serial.print(marked);
    Serial.println(" records as synced");

    return true;
}

uint16_t storageGetUnsyncedCount() {
    // Load metadata
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        return 0;
    }

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        Serial.println("ERROR: Failed to open NVS for unsynced count");
        return 0;
    }

    uint16_t unsynced_count = 0;

    // Iterate through all records
    for (uint16_t i = 0; i < meta.record_count; i++) {
        // Calculate physical index
        uint16_t physical_index;
        if (meta.record_count < DRINK_MAX_RECORDS) {
            physical_index = i;
        } else {
            physical_index = (meta.write_index + i) % DRINK_MAX_RECORDS;
        }

        // Load record
        char key[16];
        getDrinkRecordKey(physical_index, key, sizeof(key));

        DrinkRecord record;
        size_t read_size = prefs.getBytes(key, &record, sizeof(DrinkRecord));
        if (read_size == sizeof(DrinkRecord)) {
            // Check if unsynced (bit 0 not set)
            if ((record.flags & 0x01) == 0) {
                unsynced_count++;
            }
        }
    }

    prefs.end();

    return unsynced_count;
}

bool storageGetUnsyncedRecords(DrinkRecord* buffer, uint16_t max_count, uint16_t& out_count) {
    out_count = 0;

    // Load metadata
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        Serial.println("No drink records in storage");
        return true; // Not an error, just empty
    }

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        Serial.println("ERROR: Failed to open NVS for unsynced records");
        return false;
    }

    // Iterate through all records in chronological order
    for (uint16_t i = 0; i < meta.record_count && out_count < max_count; i++) {
        // Calculate physical index
        uint16_t physical_index;
        if (meta.record_count < DRINK_MAX_RECORDS) {
            physical_index = i;
        } else {
            physical_index = (meta.write_index + i) % DRINK_MAX_RECORDS;
        }

        // Load record
        char key[16];
        getDrinkRecordKey(physical_index, key, sizeof(key));

        DrinkRecord record;
        size_t read_size = prefs.getBytes(key, &record, sizeof(DrinkRecord));
        if (read_size == sizeof(DrinkRecord)) {
            // Check if unsynced (bit 0 not set)
            if ((record.flags & 0x01) == 0) {
                buffer[out_count] = record;
                out_count++;
            }
        }
    }

    prefs.end();

    Serial.print("Retrieved ");
    Serial.print(out_count);
    Serial.println(" unsynced records");

    return true;
}
