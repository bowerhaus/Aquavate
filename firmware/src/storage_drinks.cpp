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

bool storageUpdateLastDrinkRecord(const DrinkRecord& record) {
    // Load metadata to find last written index
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        Serial.println("ERROR: Cannot update - no records exist");
        return false;
    }

    // Calculate last written index
    uint16_t last_index = (meta.write_index == 0)
        ? (DRINK_MAX_RECORDS - 1)
        : (meta.write_index - 1);

    // Generate key and update record
    char key[16];
    getDrinkRecordKey(last_index, key, sizeof(key));

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("ERROR: Failed to open NVS for drink record update");
        return false;
    }

    size_t written = prefs.putBytes(key, &record, sizeof(DrinkRecord));
    prefs.end();

    if (written != sizeof(DrinkRecord)) {
        Serial.println("ERROR: Failed to update last drink record");
        return false;
    }

    Serial.print("Updated last drink record: ");
    Serial.println(key);

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
