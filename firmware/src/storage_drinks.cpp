// storage_drinks.cpp - LittleFS storage for drink records, NVS for daily state
// Part of the Aquavate smart water bottle firmware
//
// Drink records use LittleFS file with fixed-size slots for true in-place overwrites.
// This eliminates NVS fragmentation that caused ESP_ERR_NVS_NOT_ENOUGH_SPACE errors.
// Daily state remains in NVS (small, rarely changes) with retry logic.

#include <LittleFS.h>
#include <Preferences.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "storage_drinks.h"
#include "config.h"

// External debug flag from main.cpp
extern bool g_debug_drink_tracking;

// LittleFS file paths
#define DRINK_FILE "/drinks.bin"
#define META_FILE "/meta.bin"

// NVS retry configuration (for daily state only)
#define NVS_MAX_RETRIES 3
#define NVS_RETRY_DELAY_MS 10

// LittleFS initialization state
static bool g_littlefs_mounted = false;

// ============================================================================
// LittleFS Initialization
// ============================================================================

bool storageInitDrinkFS() {
    if (g_littlefs_mounted) {
        return true;  // Already mounted
    }

    // Mount LittleFS, format if needed (first boot after partition change)
    if (!LittleFS.begin(true)) {
        Serial.println("ERROR: LittleFS mount failed");
        return false;
    }

    g_littlefs_mounted = true;
    Serial.println("LittleFS mounted for drink storage");

    // Print filesystem info
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    Serial.printf("LittleFS: %u bytes used / %u bytes total\n", used, total);

    return true;
}

// ============================================================================
// Buffer Metadata (LittleFS)
// ============================================================================

bool storageLoadBufferMetadata(CircularBufferMetadata& meta) {
    if (!g_littlefs_mounted) {
        Serial.println("ERROR: LittleFS not mounted");
        return false;
    }

    File file = LittleFS.open(META_FILE, "r");
    if (!file) {
        DEBUG_PRINTF(g_debug_drink_tracking, "Metadata file not found (first run)\n");
        return false;
    }

    size_t read_size = file.read((uint8_t*)&meta, sizeof(CircularBufferMetadata));
    file.close();

    if (read_size != sizeof(CircularBufferMetadata)) {
        Serial.println("ERROR: Failed to read buffer metadata");
        return false;
    }

    return true;
}

bool storageSaveBufferMetadata(const CircularBufferMetadata& meta) {
    if (!g_littlefs_mounted) {
        Serial.println("ERROR: LittleFS not mounted");
        return false;
    }

    File file = LittleFS.open(META_FILE, "w");
    if (!file) {
        Serial.println("ERROR: Failed to open metadata file for writing");
        return false;
    }

    size_t written = file.write((const uint8_t*)&meta, sizeof(CircularBufferMetadata));
    file.close();

    if (written != sizeof(CircularBufferMetadata)) {
        Serial.println("ERROR: Failed to write buffer metadata");
        return false;
    }

    return true;
}

// ============================================================================
// Drink Records (LittleFS - fixed-size slots, true in-place overwrites)
// ============================================================================

// Helper: Calculate file offset for a given slot index
static size_t getDrinkRecordOffset(uint16_t index) {
    return (size_t)index * sizeof(DrinkRecord);
}

bool storageSaveDrinkRecord(const DrinkRecord& record) {
    if (!g_littlefs_mounted) {
        Serial.println("ERROR: LittleFS not mounted");
        return false;
    }

    // Load or initialize metadata
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta)) {
        // Initialize new buffer
        meta.write_index = 0;
        meta.record_count = 0;
        meta.total_writes = 0;
        meta.next_record_id = 1;  // Start IDs at 1 (0 = invalid/unassigned)
        meta._reserved = 0;
    }

    // Create a copy of the record to assign the ID
    DrinkRecord record_with_id = record;
    record_with_id.record_id = meta.next_record_id;

    // Open drinks file for read+write (create if doesn't exist)
    File file = LittleFS.open(DRINK_FILE, "r+");
    if (!file) {
        // File doesn't exist, create it
        file = LittleFS.open(DRINK_FILE, "w");
        if (!file) {
            Serial.println("ERROR: Failed to create drinks file");
            return false;
        }
        file.close();
        file = LittleFS.open(DRINK_FILE, "r+");
        if (!file) {
            Serial.println("ERROR: Failed to open drinks file for r+");
            return false;
        }
    }

    // Seek to the slot position and write (true in-place overwrite)
    size_t offset = getDrinkRecordOffset(meta.write_index);
    if (!file.seek(offset)) {
        Serial.printf("ERROR: Failed to seek to offset %u\n", offset);
        file.close();
        return false;
    }

    size_t written = file.write((const uint8_t*)&record_with_id, sizeof(DrinkRecord));
    file.close();

    if (written != sizeof(DrinkRecord)) {
        Serial.println("ERROR: Failed to write drink record");
        return false;
    }

    // Update metadata
    meta.write_index = (meta.write_index + 1) % DRINK_MAX_RECORDS;
    if (meta.record_count < DRINK_MAX_RECORDS) {
        meta.record_count++;
    }
    meta.total_writes++;
    meta.next_record_id++;

    if (!storageSaveBufferMetadata(meta)) {
        Serial.println("WARNING: Drink record saved but metadata update failed");
        return false;
    }

    Serial.printf("Drink record saved to slot %u, id=%u (total: %u)\n",
                  (meta.write_index == 0 ? DRINK_MAX_RECORDS - 1 : meta.write_index - 1),
                  record_with_id.record_id, meta.record_count);

    return true;
}

bool storageLoadLastDrinkRecord(DrinkRecord& record) {
    if (!g_littlefs_mounted) {
        Serial.println("ERROR: LittleFS not mounted");
        return false;
    }

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

    File file = LittleFS.open(DRINK_FILE, "r");
    if (!file) {
        Serial.println("ERROR: Failed to open drinks file for reading");
        return false;
    }

    size_t offset = getDrinkRecordOffset(last_index);
    if (!file.seek(offset)) {
        Serial.printf("ERROR: Failed to seek to offset %u\n", offset);
        file.close();
        return false;
    }

    size_t read_size = file.read((uint8_t*)&record, sizeof(DrinkRecord));
    file.close();

    if (read_size != sizeof(DrinkRecord)) {
        Serial.println("ERROR: Failed to read last drink record");
        return false;
    }

    return true;
}

bool storageGetDrinkRecord(uint16_t index, DrinkRecord& record) {
    if (!g_littlefs_mounted) {
        Serial.println("ERROR: LittleFS not mounted");
        return false;
    }

    // Load metadata to validate index
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        Serial.println("No drink records in storage");
        return false;
    }

    // Validate logical index
    if (index >= meta.record_count) {
        Serial.printf("ERROR: Invalid record index: %u (max: %u)\n", index, meta.record_count - 1);
        return false;
    }

    // Calculate physical index
    // If buffer is full (record_count == 600), oldest record is at write_index
    // If buffer is partial, oldest record is at index 0
    uint16_t physical_index;
    if (meta.record_count < DRINK_MAX_RECORDS) {
        physical_index = index;
    } else {
        physical_index = (meta.write_index + index) % DRINK_MAX_RECORDS;
    }

    File file = LittleFS.open(DRINK_FILE, "r");
    if (!file) {
        Serial.println("ERROR: Failed to open drinks file for reading");
        return false;
    }

    size_t offset = getDrinkRecordOffset(physical_index);
    if (!file.seek(offset)) {
        Serial.printf("ERROR: Failed to seek to offset %u\n", offset);
        file.close();
        return false;
    }

    size_t read_size = file.read((uint8_t*)&record, sizeof(DrinkRecord));
    file.close();

    if (read_size != sizeof(DrinkRecord)) {
        Serial.printf("ERROR: Failed to read drink record at index %u\n", index);
        return false;
    }

    return true;
}

// Helper: Read record at physical index
static bool readRecordAtPhysicalIndex(File& file, uint16_t physical_index, DrinkRecord& record) {
    size_t offset = getDrinkRecordOffset(physical_index);
    if (!file.seek(offset)) {
        return false;
    }
    return file.read((uint8_t*)&record, sizeof(DrinkRecord)) == sizeof(DrinkRecord);
}

// Helper: Write record at physical index
static bool writeRecordAtPhysicalIndex(File& file, uint16_t physical_index, const DrinkRecord& record) {
    size_t offset = getDrinkRecordOffset(physical_index);
    if (!file.seek(offset)) {
        return false;
    }
    return file.write((const uint8_t*)&record, sizeof(DrinkRecord)) == sizeof(DrinkRecord);
}

bool storageMarkSynced(uint16_t start_index, uint16_t count) {
    (void)start_index;  // Not used - we mark the first N unsynced records

    if (!g_littlefs_mounted) {
        Serial.println("ERROR: LittleFS not mounted");
        return false;
    }

    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        Serial.println("No drink records to mark synced");
        return false;
    }

    File file = LittleFS.open(DRINK_FILE, "r+");
    if (!file) {
        Serial.println("ERROR: Failed to open drinks file for mark synced");
        return false;
    }

    uint16_t marked = 0;
    for (uint16_t i = 0; i < meta.record_count && marked < count; i++) {
        uint16_t physical_index;
        if (meta.record_count < DRINK_MAX_RECORDS) {
            physical_index = i;
        } else {
            physical_index = (meta.write_index + i) % DRINK_MAX_RECORDS;
        }

        DrinkRecord record;
        if (!readRecordAtPhysicalIndex(file, physical_index, record)) {
            Serial.printf("WARNING: Failed to read record at physical index %u\n", physical_index);
            continue;
        }

        // Only mark if currently unsynced (bit 0 not set)
        if ((record.flags & 0x01) == 0) {
            record.flags |= 0x01;
            if (writeRecordAtPhysicalIndex(file, physical_index, record)) {
                marked++;
            } else {
                Serial.printf("WARNING: Failed to write synced flag at physical index %u\n", physical_index);
            }
        }
    }

    file.close();

    Serial.printf("Marked %u records as synced\n", marked);
    return true;
}

uint16_t storageGetUnsyncedCount() {
    if (!g_littlefs_mounted) {
        return 0;
    }

    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        return 0;
    }

    File file = LittleFS.open(DRINK_FILE, "r");
    if (!file) {
        Serial.println("ERROR: Failed to open drinks file for unsynced count");
        return 0;
    }

    uint16_t unsynced_count = 0;
    for (uint16_t i = 0; i < meta.record_count; i++) {
        uint16_t physical_index;
        if (meta.record_count < DRINK_MAX_RECORDS) {
            physical_index = i;
        } else {
            physical_index = (meta.write_index + i) % DRINK_MAX_RECORDS;
        }

        DrinkRecord record;
        if (readRecordAtPhysicalIndex(file, physical_index, record)) {
            if ((record.flags & 0x01) == 0) {
                unsynced_count++;
            }
        }
    }

    file.close();
    return unsynced_count;
}

bool storageGetUnsyncedRecords(DrinkRecord* buffer, uint16_t max_count, uint16_t& out_count) {
    out_count = 0;

    if (!g_littlefs_mounted) {
        Serial.println("ERROR: LittleFS not mounted");
        return false;
    }

    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        Serial.println("No drink records in storage");
        return true;  // Not an error, just empty
    }

    File file = LittleFS.open(DRINK_FILE, "r");
    if (!file) {
        Serial.println("ERROR: Failed to open drinks file for unsynced records");
        return false;
    }

    for (uint16_t i = 0; i < meta.record_count && out_count < max_count; i++) {
        uint16_t physical_index;
        if (meta.record_count < DRINK_MAX_RECORDS) {
            physical_index = i;
        } else {
            physical_index = (meta.write_index + i) % DRINK_MAX_RECORDS;
        }

        DrinkRecord record;
        if (readRecordAtPhysicalIndex(file, physical_index, record)) {
            // Check if unsynced (bit 0 not set) AND not deleted (bit 2 not set)
            if ((record.flags & 0x01) == 0 && (record.flags & 0x04) == 0) {
                buffer[out_count] = record;
                out_count++;
            }
        }
    }

    file.close();

    Serial.printf("Retrieved %u unsynced records\n", out_count);
    return true;
}

bool storageMarkDeleted(uint32_t record_id) {
    if (!g_littlefs_mounted) {
        Serial.println("ERROR: LittleFS not mounted");
        return false;
    }

    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        Serial.println("No drink records in storage");
        return false;
    }

    File file = LittleFS.open(DRINK_FILE, "r+");
    if (!file) {
        Serial.println("ERROR: Failed to open drinks file for mark deleted");
        return false;
    }

    bool found = false;
    for (uint16_t i = 0; i < meta.record_count; i++) {
        uint16_t physical_index;
        if (meta.record_count < DRINK_MAX_RECORDS) {
            physical_index = i;
        } else {
            physical_index = (meta.write_index + i) % DRINK_MAX_RECORDS;
        }

        DrinkRecord record;
        if (readRecordAtPhysicalIndex(file, physical_index, record)) {
            if (record.record_id == record_id) {
                record.flags |= 0x04;  // Set deleted flag (bit 2)
                if (writeRecordAtPhysicalIndex(file, physical_index, record)) {
                    found = true;
                    Serial.printf("Marked record %u as deleted\n", record_id);
                } else {
                    Serial.printf("ERROR: Failed to write deleted flag for record %u\n", record_id);
                }
                break;
            }
        }
    }

    file.close();

    if (!found) {
        Serial.printf("Record %u not found (may have rolled off)\n", record_id);
    }

    return found;
}

// ============================================================================
// Daily State (NVS with retry logic - unchanged from before)
// ============================================================================

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
    // Use ESP-IDF NVS API directly for error codes and retry logic
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        Serial.printf("ERROR: nvs_open failed for daily state: 0x%x\n", err);
        return false;
    }

    // Try up to 3 times with ESP-IDF API for error codes
    bool write_success = false;
    esp_err_t last_err = ESP_OK;
    for (int retry = 0; retry < NVS_MAX_RETRIES; retry++) {
        if (retry > 0) {
            DEBUG_PRINTF(g_debug_drink_tracking, "NVS daily state write retry %d/%d (last error: 0x%x)...\n",
                         retry + 1, NVS_MAX_RETRIES, last_err);
            delay(NVS_RETRY_DELAY_MS);
        }
        last_err = nvs_set_blob(nvs_handle, "daily_state", &state, sizeof(DailyState));
        if (last_err == ESP_OK) {
            last_err = nvs_commit(nvs_handle);
            if (last_err == ESP_OK) {
                write_success = true;
                break;
            }
        }
    }
    nvs_close(nvs_handle);

    if (!write_success) {
        Serial.printf("ERROR: NVS daily state write failed after %d retries (error: 0x%x)\n",
                      NVS_MAX_RETRIES, last_err);
        return false;
    }

    return true;
}
