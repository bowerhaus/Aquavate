/**
 * Aquavate - BLE Service Implementation
 * NimBLE GATT server for iOS app communication
 */

#include "config.h"

#if ENABLE_BLE

#include "ble_service.h"
#include <NimBLEDevice.h>
#include <esp_mac.h>
#include "aquavate.h"
#include "config.h"
#include "storage_drinks.h"

// NimBLE objects
static NimBLEServer* pServer = nullptr;
static NimBLEService* pDeviceInfoService = nullptr;
static NimBLEService* pBatteryService = nullptr;
static NimBLEService* pAquavateService = nullptr;
static NimBLEAdvertising* pAdvertising = nullptr;

// Characteristics - Standard Services
static NimBLECharacteristic* pManufacturerChar = nullptr;
static NimBLECharacteristic* pModelChar = nullptr;
static NimBLECharacteristic* pFirmwareChar = nullptr;
static NimBLECharacteristic* pSerialChar = nullptr;
static NimBLECharacteristic* pBatteryLevelChar = nullptr;

// Characteristics - Aquavate Service
static NimBLECharacteristic* pCurrentStateChar = nullptr;
static NimBLECharacteristic* pBottleConfigChar = nullptr;
static NimBLECharacteristic* pSyncControlChar = nullptr;
static NimBLECharacteristic* pDrinkDataChar = nullptr;
static NimBLECharacteristic* pCommandChar = nullptr;

// Connection state
static bool isConnected = false;
static uint32_t advertisingStartTime = 0;
static bool isAdvertising = false;

// Current state cache
static BLE_CurrentState currentState = {0};
static uint8_t lastBatteryPercent = 0;

// Bottle config cache (defaults used if no calibration data)
static BLE_BottleConfig bottleConfig = {
    .scale_factor = 1.0f,
    .tare_weight_grams = 0,
    .bottle_capacity_ml = 830,
    .daily_goal_ml = DRINK_DAILY_GOAL_ML
};

// Sync Control state
static BLE_SyncControl syncControl = {0};
static DrinkRecord* syncBuffer = nullptr;  // Buffer for unsynced records
static uint16_t syncBufferSize = 0;        // Number of records in buffer
static uint16_t syncCurrentChunk = 0;      // Current chunk being sent

// Command execution flags (main loop will handle these)
static volatile bool g_ble_tare_requested = false;
static volatile bool g_ble_reset_daily_requested = false;
static volatile bool g_ble_clear_history_requested = false;
static volatile bool g_ble_set_daily_total_requested = false;
static volatile uint16_t g_ble_set_daily_total_value = 0;
static volatile bool g_ble_force_display_refresh = false;

// Forward declaration for sync complete notification
static void bleNotifyCurrentStateUpdate();

// Debug helpers
extern bool g_debug_enabled;
extern bool g_debug_ble;

#define BLE_DEBUG(x) if (g_debug_enabled && g_debug_ble) { Serial.print("[BLE] "); Serial.println(x); }
#define BLE_DEBUG_F(fmt, ...) if (g_debug_enabled && g_debug_ble) { Serial.printf("[BLE] " fmt "\n", ##__VA_ARGS__); }

// Forward declarations
void bleLoadBottleConfig();
void bleSaveBottleConfig();
void bleSyncSendNextChunk();

// Bottle Config characteristic callbacks
class BottleConfigCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic) {
        BLE_DEBUG("Bottle Config read");
        // Reload from NVS to ensure fresh data
        bleLoadBottleConfig();
    }

    void onWrite(NimBLECharacteristic* pCharacteristic) {
        BLE_DEBUG("Bottle Config write");

        // Get written data
        std::string value = pCharacteristic->getValue();
        if (value.length() == sizeof(BLE_BottleConfig)) {
            memcpy(&bottleConfig, value.data(), sizeof(BLE_BottleConfig));

            BLE_DEBUG_F("Config updated: scale=%.2f, tare=%d, capacity=%d, goal=%d",
                       bottleConfig.scale_factor, bottleConfig.tare_weight_grams,
                       bottleConfig.bottle_capacity_ml, bottleConfig.daily_goal_ml);

            // Save to NVS
            bleSaveBottleConfig();

            // Trigger Current State update to reflect config change
            // (Main loop will call bleUpdateCurrentState)
        } else {
            BLE_DEBUG_F("Invalid config size: %d bytes (expected %d)",
                       value.length(), sizeof(BLE_BottleConfig));
        }
    }
};

// Forward declarations for time handling
extern bool g_time_valid;
void drinksInit();  // Re-initialize drink tracking when time becomes valid

// Command characteristic callbacks
class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        BLE_DEBUG("Command received");

        // Get written data
        std::string value = pCharacteristic->getValue();

        // Check for SET_TIME command (5 bytes)
        if (value.length() == sizeof(BLE_SetTimeCommand) && value[0] == BLE_CMD_SET_TIME) {
            BLE_SetTimeCommand timeCmd;
            memcpy(&timeCmd, value.data(), sizeof(BLE_SetTimeCommand));

            BLE_DEBUG_F("Command: SET_TIME, timestamp=%u", timeCmd.timestamp);

            // Set the ESP32 RTC
            struct timeval tv;
            tv.tv_sec = timeCmd.timestamp;
            tv.tv_usec = 0;

            if (settimeofday(&tv, NULL) == 0) {
                BLE_DEBUG("Time set successfully");

                // Mark time as valid
                g_time_valid = true;
                storageSaveTimeValid(true);

                // Re-initialize drink tracking now that time is valid
                drinksInit();

                // Log the time we set (using gmtime_r to avoid IRAM overhead)
                time_t now = time(NULL);
                struct tm timeinfo;
                gmtime_r(&now, &timeinfo);
                char timeStr[32];
                snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
                         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                BLE_DEBUG_F("Device time set to: %s", timeStr);
            } else {
                BLE_DEBUG("ERROR: Failed to set time");
            }
            return;
        }

        // Check for SET_DAILY_TOTAL command (3 bytes: cmd + 2-byte little-endian value)
        // DEPRECATED: Kept for backwards compatibility, but apps should use DELETE_DRINK_RECORD
        if (value.length() == 3 && value[0] == BLE_CMD_SET_DAILY_TOTAL) {
            uint16_t dailyTotal = (uint8_t)value[1] | ((uint8_t)value[2] << 8);
            BLE_DEBUG_F("Command: SET_DAILY_TOTAL to %dml (DEPRECATED)", dailyTotal);
            g_ble_set_daily_total_value = dailyTotal;
            g_ble_set_daily_total_requested = true;
            g_ble_force_display_refresh = true;  // Force display update
            return;
        }

        // Check for DELETE_DRINK_RECORD command (5 bytes: cmd + 4-byte record_id)
        if (value.length() == 5 && value[0] == BLE_CMD_DELETE_DRINK_RECORD) {
            uint32_t recordId = (uint8_t)value[1] | ((uint8_t)value[2] << 8) |
                               ((uint8_t)value[3] << 16) | ((uint8_t)value[4] << 24);

            BLE_DEBUG_F("Command: DELETE_DRINK_RECORD id=%lu", recordId);

            bool found = storageMarkDeleted(recordId);
            if (found) {
                drinksRecalculateTotals();
                BLE_DEBUG_F("DELETE_DRINK_RECORD: %lu deleted, total recalculated", recordId);
            } else {
                BLE_DEBUG_F("DELETE_DRINK_RECORD: %lu not found (rolled off)", recordId);
            }

            // Always notify - either way, the delete is "successful" from the app's perspective
            g_ble_force_display_refresh = true;
            bleNotifyCurrentStateUpdate();
            return;
        }

        // Standard 4-byte command
        if (value.length() == sizeof(BLE_Command)) {
            BLE_Command cmd;
            memcpy(&cmd, value.data(), sizeof(BLE_Command));

            BLE_DEBUG_F("Command: 0x%02X, param1=%d, param2=%d",
                       cmd.command, cmd.param1, cmd.param2);

            // Set flags for main loop to handle
            switch (cmd.command) {
                case BLE_CMD_TARE_NOW:
                    BLE_DEBUG("Command: TARE_NOW");
                    g_ble_tare_requested = true;
                    break;

                case BLE_CMD_RESET_DAILY:
                    BLE_DEBUG("Command: RESET_DAILY");
                    g_ble_reset_daily_requested = true;
                    break;

                case BLE_CMD_CLEAR_HISTORY:
                    BLE_DEBUG("Command: CLEAR_HISTORY (WARNING)");
                    g_ble_clear_history_requested = true;
                    break;

                default:
                    BLE_DEBUG_F("Unknown command: 0x%02X", cmd.command);
                    break;
            }
        } else {
            BLE_DEBUG_F("Invalid command size: %d bytes (expected 4 or 5)",
                       value.length());
        }
    }
};

// Sync Control characteristic callbacks (Phase 3D)
class SyncControlCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic) {
        BLE_DEBUG("Sync Control read");
        // Update status with current unsynced count
        syncControl.count = storageGetUnsyncedCount();
        pCharacteristic->setValue((uint8_t*)&syncControl, sizeof(syncControl));
    }

    void onWrite(NimBLECharacteristic* pCharacteristic) {
        BLE_DEBUG("Sync Control write");

        // Get written data
        std::string value = pCharacteristic->getValue();
        if (value.length() != sizeof(BLE_SyncControl)) {
            BLE_DEBUG_F("Invalid sync control size: %d bytes (expected %d)",
                       value.length(), sizeof(BLE_SyncControl));
            return;
        }

        BLE_SyncControl request;
        memcpy(&request, value.data(), sizeof(BLE_SyncControl));

        BLE_DEBUG_F("Sync command: %d, start=%d, count=%d, chunk_size=%d",
                   request.command, request.start_index, request.count, request.chunk_size);

        // Handle sync commands
        switch (request.command) {
            case 0: // QUERY
                BLE_DEBUG("Sync: QUERY");
                syncControl.count = storageGetUnsyncedCount();
                syncControl.status = 0; // IDLE
                pCharacteristic->setValue((uint8_t*)&syncControl, sizeof(syncControl));
                break;

            case 1: // START
                BLE_DEBUG_F("Sync: START (count=%d)", request.count);

                // Free previous buffer if exists
                if (syncBuffer != nullptr) {
                    delete[] syncBuffer;
                    syncBuffer = nullptr;
                }

                // Allocate buffer for unsynced records
                syncBufferSize = 0;
                syncBuffer = new DrinkRecord[request.count];

                if (syncBuffer == nullptr) {
                    BLE_DEBUG("ERROR: Failed to allocate sync buffer");
                    syncControl.status = 0; // IDLE
                    syncControl.count = 0;
                    pCharacteristic->setValue((uint8_t*)&syncControl, sizeof(syncControl));
                    return;
                }

                // Load unsynced records
                if (!storageGetUnsyncedRecords(syncBuffer, request.count, syncBufferSize)) {
                    BLE_DEBUG("ERROR: Failed to load unsynced records");
                    delete[] syncBuffer;
                    syncBuffer = nullptr;
                    syncControl.status = 0; // IDLE
                    syncControl.count = 0;
                    pCharacteristic->setValue((uint8_t*)&syncControl, sizeof(syncControl));
                    return;
                }

                // Setup sync state
                syncControl.start_index = 0;
                syncControl.count = syncBufferSize;
                syncControl.chunk_size = (request.chunk_size > 0 && request.chunk_size <= 20)
                                        ? request.chunk_size : 20;
                syncControl.status = 1; // IN_PROGRESS
                syncCurrentChunk = 0;

                BLE_DEBUG_F("Sync started: %d records, chunk_size=%d",
                           syncBufferSize, syncControl.chunk_size);

                // Update characteristic and send first chunk
                pCharacteristic->setValue((uint8_t*)&syncControl, sizeof(syncControl));
                bleSyncSendNextChunk();
                break;

            case 2: { // ACK
                BLE_DEBUG_F("Sync: ACK chunk %d", syncCurrentChunk);

                if (syncControl.status != 1) {
                    BLE_DEBUG("ERROR: Received ACK but sync not in progress");
                    return;
                }

                // Move to next chunk
                syncCurrentChunk++;

                // Calculate total chunks
                uint16_t total_chunks = (syncBufferSize + syncControl.chunk_size - 1) / syncControl.chunk_size;

                if (syncCurrentChunk >= total_chunks) {
                    // Sync complete - mark all records as synced
                    BLE_DEBUG("Sync: COMPLETE");

                    // Mark records as synced in NVS
                    storageMarkSynced(0, syncBufferSize);

                    // Free buffer
                    delete[] syncBuffer;
                    syncBuffer = nullptr;
                    syncBufferSize = 0;

                    // Update state
                    syncControl.status = 2; // COMPLETE
                    pCharacteristic->setValue((uint8_t*)&syncControl, sizeof(syncControl));

                    // Reset to IDLE after brief delay
                    syncControl.status = 0; // IDLE
                    syncControl.count = 0;

                    // Notify app of updated unsynced count (should now be 0)
                    bleNotifyCurrentStateUpdate();
                } else {
                    // Send next chunk
                    bleSyncSendNextChunk();
                }
                break;
            }

            default:
                BLE_DEBUG_F("Unknown sync command: %d", request.command);
                break;
        }
    }
};

// Server callbacks
class AquavateServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        BLE_DEBUG("Client connected");
        isConnected = true;
        isAdvertising = false;

        // Update connection parameters for fast sync
        NimBLEDevice::getClientList();
        // Connection params are set in advertising data
    }

    void onDisconnect(NimBLEServer* pServer) {
        BLE_DEBUG("Client disconnected");
        isConnected = false;

        // Restart advertising for next connection
        // Note: Main loop will handle advertising timeout logic
        bleStartAdvertising();
    }
};

// Load bottle config from NVS
void bleLoadBottleConfig() {
    CalibrationData cal;
    if (storageLoadCalibration(cal)) {
        bottleConfig.scale_factor = cal.scale_factor;
        bottleConfig.tare_weight_grams = (int32_t)(cal.empty_bottle_adc / cal.scale_factor);
        bottleConfig.bottle_capacity_ml = 830; // Default capacity (could be configurable later)
        bottleConfig.daily_goal_ml = DRINK_DAILY_GOAL_ML;

        // Update characteristic value
        if (pBottleConfigChar) {
            pBottleConfigChar->setValue((uint8_t*)&bottleConfig, sizeof(bottleConfig));
        }

        BLE_DEBUG_F("Loaded config: scale=%.2f, tare=%d, capacity=%d, goal=%d",
                   bottleConfig.scale_factor, bottleConfig.tare_weight_grams,
                   bottleConfig.bottle_capacity_ml, bottleConfig.daily_goal_ml);
    } else {
        BLE_DEBUG("No calibration data found in NVS");
    }
}

// Save bottle config to NVS
void bleSaveBottleConfig() {
    CalibrationData cal;

    // Load existing calibration to preserve ADC values
    if (storageLoadCalibration(cal)) {
        // Update scale factor
        cal.scale_factor = bottleConfig.scale_factor;

        // Update empty bottle ADC from tare weight
        // empty_bottle_adc = tare_weight * scale_factor
        cal.empty_bottle_adc = (int32_t)(bottleConfig.tare_weight_grams * bottleConfig.scale_factor);

        // Recalculate full bottle ADC
        // full_bottle_adc = empty_bottle_adc + (830ml * scale_factor)
        cal.full_bottle_adc = cal.empty_bottle_adc + (int32_t)(830.0f * cal.scale_factor);

        // Save to NVS
        if (storageSaveCalibration(cal)) {
            BLE_DEBUG("Config saved to NVS");
        } else {
            BLE_DEBUG("Failed to save config to NVS");
        }
    } else {
        BLE_DEBUG("Cannot save config: no existing calibration");
    }
}

// Get device MAC address for unique naming
String bleGetDeviceSuffix() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    return String(suffix);
}

// Send next chunk of drink data (Phase 3D)
void bleSyncSendNextChunk() {
    if (syncBuffer == nullptr || syncBufferSize == 0) {
        BLE_DEBUG("ERROR: No sync buffer to send");
        return;
    }

    if (!isConnected || pDrinkDataChar == nullptr) {
        BLE_DEBUG("ERROR: Not connected or Drink Data char not initialized");
        return;
    }

    // Calculate chunk boundaries
    uint16_t chunk_start = syncCurrentChunk * syncControl.chunk_size;
    uint16_t chunk_end = chunk_start + syncControl.chunk_size;
    if (chunk_end > syncBufferSize) {
        chunk_end = syncBufferSize;
    }
    uint16_t records_in_chunk = chunk_end - chunk_start;

    // Calculate total chunks
    uint16_t total_chunks = (syncBufferSize + syncControl.chunk_size - 1) / syncControl.chunk_size;

    BLE_DEBUG_F("Sending chunk %d/%d: records %d-%d (%d records)",
               syncCurrentChunk, total_chunks - 1,
               chunk_start, chunk_end - 1, records_in_chunk);

    // Build Drink Data Chunk
    BLE_DrinkDataChunk chunk;
    chunk.chunk_index = syncCurrentChunk;
    chunk.total_chunks = total_chunks;
    chunk.record_count = records_in_chunk;
    chunk._reserved = 0;

    // Copy records from sync buffer to chunk, converting format
    for (uint16_t i = 0; i < records_in_chunk; i++) {
        DrinkRecord& src = syncBuffer[chunk_start + i];
        BLE_DrinkRecord& dst = chunk.records[i];

        dst.record_id = src.record_id;
        dst.timestamp = src.timestamp;
        dst.amount_ml = src.amount_ml;
        dst.bottle_level_ml = src.bottle_level_ml;
        dst.type = src.type;
        dst.flags = src.flags;
    }

    // Calculate chunk size: header (6 bytes) + records (14 bytes each)
    size_t chunk_size = 6 + (records_in_chunk * sizeof(BLE_DrinkRecord));

    // Send notification
    pDrinkDataChar->setValue((uint8_t*)&chunk, chunk_size);
    pDrinkDataChar->notify();

    BLE_DEBUG_F("Chunk %d sent: %d bytes", syncCurrentChunk, chunk_size);
}

// Initialize BLE service
bool bleInit() {
    BLE_DEBUG("Initializing BLE service...");

    // Initialize NimBLE
    String deviceName = "Aquavate-" + bleGetDeviceSuffix();
    BLE_DEBUG_F("Device name: %s", deviceName.c_str());

    NimBLEDevice::init(deviceName.c_str());
    NimBLEDevice::setPower(ESP_PWR_LVL_N0); // 0dBm (ESP_PWR_LVL_N0 = 0dBm)
    NimBLEDevice::setMTU(BLE_MTU_SIZE);

    // Create server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new AquavateServerCallbacks());

    // Device Information Service (0x180A)
    pDeviceInfoService = pServer->createService(NimBLEUUID((uint16_t)0x180A));

    pManufacturerChar = pDeviceInfoService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A29),
        NIMBLE_PROPERTY::READ
    );
    pManufacturerChar->setValue("Aquavate");

    pModelChar = pDeviceInfoService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A24),
        NIMBLE_PROPERTY::READ
    );
    pModelChar->setValue("Puck v1.0");

    char firmware[16];
    snprintf(firmware, sizeof(firmware), "%d.%d.%d",
             AQUAVATE_VERSION_MAJOR, AQUAVATE_VERSION_MINOR, AQUAVATE_VERSION_PATCH);
    pFirmwareChar = pDeviceInfoService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A26),
        NIMBLE_PROPERTY::READ
    );
    pFirmwareChar->setValue(firmware);

    // Serial number from MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char serial[18];
    snprintf(serial, sizeof(serial), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    pSerialChar = pDeviceInfoService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A25),
        NIMBLE_PROPERTY::READ
    );
    pSerialChar->setValue(serial);

    pDeviceInfoService->start();
    BLE_DEBUG("Device Information Service started");

    // Battery Service (0x180F)
    pBatteryService = pServer->createService(NimBLEUUID((uint16_t)0x180F));

    pBatteryLevelChar = pBatteryService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A19),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    pBatteryLevelChar->setValue(&lastBatteryPercent, 1);

    pBatteryService->start();
    BLE_DEBUG("Battery Service started");

    // Aquavate Custom Service
    pAquavateService = pServer->createService(AQUAVATE_SERVICE_UUID);

    // Current State characteristic (Phase 3B)
    pCurrentStateChar = pAquavateService->createCharacteristic(
        AQUAVATE_CURRENT_STATE_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    pCurrentStateChar->setValue((uint8_t*)&currentState, sizeof(currentState));

    // Bottle Config characteristic (Phase 3C)
    pBottleConfigChar = pAquavateService->createCharacteristic(
        AQUAVATE_BOTTLE_CONFIG_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pBottleConfigChar->setCallbacks(new BottleConfigCallbacks());
    pBottleConfigChar->setValue((uint8_t*)&bottleConfig, sizeof(bottleConfig));
    bleLoadBottleConfig(); // Load initial config from NVS

    // Command characteristic (Phase 3C)
    pCommandChar = pAquavateService->createCharacteristic(
        AQUAVATE_COMMAND_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pCommandChar->setCallbacks(new CommandCallbacks());

    // Sync Control characteristic (Phase 3D)
    pSyncControlChar = pAquavateService->createCharacteristic(
        AQUAVATE_SYNC_CONTROL_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pSyncControlChar->setCallbacks(new SyncControlCallbacks());
    syncControl.start_index = 0;
    syncControl.count = 0;
    syncControl.command = 0;
    syncControl.status = 0; // IDLE
    syncControl.chunk_size = 20;
    pSyncControlChar->setValue((uint8_t*)&syncControl, sizeof(syncControl));

    // Drink Data characteristic (Phase 3D)
    pDrinkDataChar = pAquavateService->createCharacteristic(
        AQUAVATE_DRINK_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    // Set initial value with empty chunk header (prevents 0-byte notification on subscribe)
    static const uint8_t emptyChunk[6] = {0, 0, 0, 0, 0, 0}; // chunk_index=0, total_chunks=0, record_count=0, reserved=0
    pDrinkDataChar->setValue(emptyChunk, sizeof(emptyChunk));

    pAquavateService->start();
    BLE_DEBUG("Aquavate Service started (Current State + Config + Commands + Sync)");

    // Setup advertising
    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(AQUAVATE_SERVICE_UUID);
    pAdvertising->addServiceUUID(NimBLEUUID((uint16_t)0x180F)); // Battery Service
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // Connection interval

    // Set advertising interval (1000ms for power optimization)
    pAdvertising->setMinInterval(BLE_ADV_INTERVAL_MS * 1000 / 625);  // Convert ms to 0.625ms units
    pAdvertising->setMaxInterval(BLE_ADV_INTERVAL_MS * 1000 / 625);

    BLE_DEBUG("BLE initialization complete");
    return true;
}

// Start advertising
void bleStartAdvertising() {
    if (isAdvertising) {
        BLE_DEBUG("Already advertising, ignoring start request");
        return;
    }

    BLE_DEBUG("Starting advertising...");
    pAdvertising->start();
    advertisingStartTime = millis();
    isAdvertising = true;
}

// Stop advertising
void bleStopAdvertising() {
    if (!isAdvertising) {
        return;
    }

    BLE_DEBUG("Stopping advertising");
    pAdvertising->stop();
    isAdvertising = false;
}

// Check if connected
bool bleIsConnected() {
    return isConnected;
}

// Update BLE service (call from main loop)
void bleUpdate() {
    // Check advertising timeout
    // Use extended timeout (2 min) if there are unsynced records to give iOS background BLE time to connect
    // Otherwise use normal timeout (30 sec)
    if (isAdvertising && !isConnected) {
        unsigned long elapsed = millis() - advertisingStartTime;
        uint16_t unsyncedCount = storageGetUnsyncedCount();
        uint32_t timeout = (unsyncedCount > 0) ? BLE_ADV_TIMEOUT_EXTENDED_SEC : BLE_ADV_TIMEOUT_SEC;

        if (elapsed > (timeout * 1000)) {
            BLE_DEBUG_F("Advertising timeout (%d seconds, unsynced=%d)", timeout, unsyncedCount);
            bleStopAdvertising();
        }
    }
}

// Update battery level
void bleUpdateBatteryLevel(uint8_t percent) {
    if (percent != lastBatteryPercent) {
        lastBatteryPercent = percent;
        pBatteryLevelChar->setValue(&percent, 1);

        if (isConnected) {
            pBatteryLevelChar->notify();
            BLE_DEBUG_F("Battery level updated: %d%%", percent);
        }
    }
}

// Update Current State (Phase 3B - full implementation)
void bleUpdateCurrentState(uint16_t daily_total_ml, int32_t current_adc,
                           const CalibrationData& cal, uint8_t battery_percent,
                           bool calibrated, bool time_valid, bool stable) {
    // Save previous state for change detection
    BLE_CurrentState previousState = currentState;

    // Update timestamp (use current time)
    currentState.timestamp = getCurrentUnixTime();

    // Calculate current weight from ADC if calibrated
    if (calibrated && cal.scale_factor != 0.0f) {
        // Convert ADC to grams: (adc - empty_adc) / scale_factor
        int32_t adc_delta = current_adc - cal.empty_bottle_adc;
        float grams = (float)adc_delta / cal.scale_factor;
        currentState.current_weight_g = (int16_t)grams;

        // Calculate bottle level in ml (water: 1g = 1ml)
        float water_ml = grams;
        if (water_ml < 0) water_ml = 0;
        if (water_ml > 830) water_ml = 830;
        currentState.bottle_level_ml = (uint16_t)water_ml;
    } else {
        currentState.current_weight_g = 0;
        currentState.bottle_level_ml = 0;
    }

    // Update daily total
    currentState.daily_total_ml = daily_total_ml;

    // Update battery
    currentState.battery_percent = battery_percent;

    // Set flags
    currentState.flags = 0;
    if (time_valid) currentState.flags |= 0x01;
    if (calibrated) currentState.flags |= 0x02;
    if (stable) currentState.flags |= 0x04;

    // Unsynced count (Phase 3D)
    currentState.unsynced_count = storageGetUnsyncedCount();

    // Update characteristic value
    pCurrentStateChar->setValue((uint8_t*)&currentState, sizeof(currentState));

    // Send notification if connected and value changed significantly
    if (isConnected) {
        bool should_notify = false;

        // Notify on significant changes
        if (currentState.daily_total_ml != previousState.daily_total_ml) {
            should_notify = true;
            BLE_DEBUG("Current State: Daily total changed, sending notification");
        } else if (abs(currentState.bottle_level_ml - previousState.bottle_level_ml) >= 10) {
            should_notify = true;
            BLE_DEBUG("Current State: Bottle level changed >10ml, sending notification");
        } else if ((currentState.flags & 0x04) != (previousState.flags & 0x04)) {
            should_notify = true;
            BLE_DEBUG("Current State: Stability changed, sending notification");
        }

        if (should_notify) {
            pCurrentStateChar->notify();
            BLE_DEBUG_F("Current State notified: %dml bottle, %dml daily, battery %d%%",
                       currentState.bottle_level_ml, currentState.daily_total_ml,
                       currentState.battery_percent);
        }
    }
}

// Notify current state with updated unsynced count (called after sync completes)
static void bleNotifyCurrentStateUpdate() {
    if (!isConnected || pCurrentStateChar == nullptr) {
        return;
    }

    // Update unsynced count and notify
    currentState.unsynced_count = storageGetUnsyncedCount();
    pCurrentStateChar->setValue((uint8_t*)&currentState, sizeof(currentState));
    pCurrentStateChar->notify();
    BLE_DEBUG_F("Current State notified after sync: unsynced=%d", currentState.unsynced_count);
}

// Command check functions (Phase 3C)
// These return true once and clear the flag, so main loop handles each command once

bool bleCheckTareRequested() {
    if (g_ble_tare_requested) {
        g_ble_tare_requested = false;
        return true;
    }
    return false;
}

bool bleCheckResetDailyRequested() {
    if (g_ble_reset_daily_requested) {
        g_ble_reset_daily_requested = false;
        return true;
    }
    return false;
}

bool bleCheckClearHistoryRequested() {
    if (g_ble_clear_history_requested) {
        g_ble_clear_history_requested = false;
        return true;
    }
    return false;
}

bool bleCheckSetDailyTotalRequested(uint16_t& value) {
    if (g_ble_set_daily_total_requested) {
        g_ble_set_daily_total_requested = false;
        value = g_ble_set_daily_total_value;
        return true;
    }
    return false;
}

bool bleCheckForceDisplayRefresh() {
    if (g_ble_force_display_refresh) {
        g_ble_force_display_refresh = false;
        return true;
    }
    return false;
}

uint16_t bleGetDailyGoalMl() {
    return bottleConfig.daily_goal_ml;
}

#endif // ENABLE_BLE
