/**
 * Aquavate - BLE Service Module
 * NimBLE GATT server for iOS app communication
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "config.h"

#if ENABLE_BLE

#include <Arduino.h>
#include "drinks.h"
#include "storage.h"

// BLE UUIDs
#define AQUAVATE_SERVICE_UUID           "6F75616B-7661-7465-2D00-000000000000"
#define AQUAVATE_CURRENT_STATE_UUID     "6F75616B-7661-7465-2D00-000000000001"
#define AQUAVATE_BOTTLE_CONFIG_UUID     "6F75616B-7661-7465-2D00-000000000002"
#define AQUAVATE_SYNC_CONTROL_UUID      "6F75616B-7661-7465-2D00-000000000003"
#define AQUAVATE_DRINK_DATA_UUID        "6F75616B-7661-7465-2D00-000000000004"
#define AQUAVATE_COMMAND_UUID           "6F75616B-7661-7465-2D00-000000000005"

// BLE advertising parameters
#define BLE_ADV_INTERVAL_MS             1000    // 1 second (power-optimized)
#define BLE_ADV_TIMEOUT_SEC             30      // Auto-stop after 30s
#define BLE_TX_POWER_DBM                0       // 0dBm (reduce from +4dBm)

// BLE connection parameters
#define BLE_CONN_INTERVAL_MIN_MS        15      // Min 15ms (fast sync)
#define BLE_CONN_INTERVAL_MAX_MS        30      // Max 30ms
#define BLE_CONN_SLAVE_LATENCY          0       // No latency during sync
#define BLE_CONN_TIMEOUT_MS             6000    // 6s supervision timeout

// BLE MTU configuration
#define BLE_MTU_SIZE                    247     // Maximum MTU for iOS

// Current State Characteristic (14 bytes)
struct __attribute__((packed)) BLE_CurrentState {
    uint32_t timestamp;          // Unix time (seconds)
    int16_t  current_weight_g;   // Current bottle weight in grams
    uint16_t bottle_level_ml;    // Water level after last event
    uint16_t daily_total_ml;     // Today's cumulative intake
    uint8_t  battery_percent;    // 0-100
    uint8_t  flags;              // Bit 0: time_valid, Bit 1: calibrated, Bit 2: stable
    uint16_t unsynced_count;     // Records pending sync
};

// Bottle Config Characteristic (12 bytes)
struct __attribute__((packed)) BLE_BottleConfig {
    float    scale_factor;        // ADC counts per gram
    int32_t  tare_weight_grams;   // Empty bottle weight
    uint16_t bottle_capacity_ml;  // Max capacity (default 830ml)
    uint16_t daily_goal_ml;       // User's daily target (default 2000ml)
};

// Sync Control Characteristic (8 bytes)
struct __attribute__((packed)) BLE_SyncControl {
    uint16_t start_index;     // Circular buffer index to start
    uint16_t count;           // Number of records to transfer
    uint8_t  command;         // 0=query, 1=start, 2=ack
    uint8_t  status;          // 0=idle, 1=in_progress, 2=complete
    uint16_t chunk_size;      // Records per chunk (default 20)
};

// Drink Record for BLE transfer (14 bytes)
struct __attribute__((packed)) BLE_DrinkRecord {
    uint32_t record_id;         // Unique ID for deletion
    uint32_t timestamp;         // Unix time
    int16_t  amount_ml;         // Consumed (+) or refilled (-)
    uint16_t bottle_level_ml;   // Level after event
    uint8_t  type;              // 0=gulp, 1=pour
    uint8_t  flags;             // 0x01=synced, 0x04=deleted
};

// Drink Data Characteristic (variable, max 206 bytes)
struct __attribute__((packed)) BLE_DrinkDataChunk {
    uint16_t chunk_index;       // Current chunk number
    uint16_t total_chunks;      // Total chunks in sync
    uint8_t  record_count;      // Records in this chunk (1-20)
    uint8_t  _reserved;         // Padding
    BLE_DrinkRecord records[20]; // Up to 20 records
};

// Command Characteristic (4 bytes)
struct __attribute__((packed)) BLE_Command {
    uint8_t  command;       // Command type
    uint8_t  param1;        // Parameter 1
    uint16_t param2;        // Parameter 2
};

// Command types
#define BLE_CMD_TARE_NOW            0x01
// 0x02-0x04 reserved for future calibration commands (iOS app will handle calibration)
#define BLE_CMD_RESET_DAILY         0x05
#define BLE_CMD_CLEAR_HISTORY       0x06
#define BLE_CMD_SET_TIME            0x10  // Set device time (5 bytes: cmd + 4-byte Unix timestamp)
#define BLE_CMD_SET_DAILY_TOTAL     0x11  // DEPRECATED: Set daily total (use DELETE_DRINK_RECORD instead)
#define BLE_CMD_DELETE_DRINK_RECORD 0x12  // Delete drink record (5 bytes: cmd + 4-byte record_id)

// Set Time Command (5 bytes) - different from standard 4-byte command
struct __attribute__((packed)) BLE_SetTimeCommand {
    uint8_t  command;       // Always 0x10
    uint32_t timestamp;     // Unix timestamp (seconds since 1970)
};

// Public API

/**
 * Initialize BLE service
 * Sets up NimBLE server, GATT services, and starts advertising
 * @return true if initialization successful
 */
bool bleInit();

/**
 * Update BLE service (call from main loop)
 * Handles periodic tasks, connection management
 */
void bleUpdate();

/**
 * Check if BLE is connected
 * @return true if iOS device is connected
 */
bool bleIsConnected();

/**
 * Start BLE advertising
 * Advertises for BLE_ADV_TIMEOUT_SEC seconds or until connected
 */
void bleStartAdvertising();

/**
 * Stop BLE advertising
 */
void bleStopAdvertising();

/**
 * Update Current State characteristic
 * Triggers notification if subscribed and value changed
 * @param state DailyState from drink tracking
 * @param current_adc Current load cell ADC reading
 * @param cal Calibration data for ADC to weight conversion
 * @param battery_percent Battery level 0-100
 * @param calibrated Calibration status
 * @param time_valid Time set status
 * @param stable Weight stability status
 */
void bleUpdateCurrentState(uint16_t daily_total_ml, int32_t current_adc,
                           const CalibrationData& cal, uint8_t battery_percent,
                           bool calibrated, bool time_valid, bool stable);

/**
 * Notify battery level change
 * Updates Battery Service characteristic
 * @param percent Battery level 0-100
 */
void bleUpdateBatteryLevel(uint8_t percent);

/**
 * Get device MAC address suffix for advertising name
 * @return Last 4 hex digits of MAC (e.g., "A3F2")
 */
String bleGetDeviceSuffix();

/**
 * Check and clear command flags (Phase 3C)
 * Main loop should call these to handle BLE commands
 */
bool bleCheckTareRequested();
bool bleCheckResetDailyRequested();
bool bleCheckClearHistoryRequested();
bool bleCheckSetDailyTotalRequested(uint16_t& value);
bool bleCheckForceDisplayRefresh();

#endif // ENABLE_BLE

#endif // BLE_SERVICE_H
