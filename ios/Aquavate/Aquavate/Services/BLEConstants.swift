//
//  BLEConstants.swift
//  Aquavate
//
//  BLE service and characteristic UUIDs matching firmware definitions.
//

import CoreBluetooth

enum BLEConstants {
    // MARK: - Device Discovery

    /// Device name prefix for scanning filter
    static let deviceNamePrefix = "Aquavate-"

    /// Scan timeout in seconds
    static let scanTimeout: TimeInterval = 10.0

    /// Connection timeout in seconds
    static let connectionTimeout: TimeInterval = 10.0

    // MARK: - Standard Services

    /// Device Information Service (0x180A)
    static let deviceInfoServiceUUID = CBUUID(string: "180A")

    /// Battery Service (0x180F)
    static let batteryServiceUUID = CBUUID(string: "180F")

    /// Battery Level Characteristic (0x2A19)
    static let batteryLevelUUID = CBUUID(string: "2A19")

    // MARK: - Aquavate Custom Service

    /// Aquavate service UUID (matches firmware BLE_SERVICE.h)
    static let aquavateServiceUUID = CBUUID(string: "6F75616B-7661-7465-2D00-000000000000")

    // MARK: - Aquavate Characteristics

    /// Current State (14 bytes) - READ/NOTIFY
    /// Contains: timestamp, weight, bottle level, daily total, battery, flags, unsynced count
    static let currentStateUUID = CBUUID(string: "6F75616B-7661-7465-2D00-000000000001")

    /// Bottle Config (12 bytes) - READ/WRITE
    /// Contains: scale_factor, tare_weight, capacity, goal
    static let bottleConfigUUID = CBUUID(string: "6F75616B-7661-7465-2D00-000000000002")

    /// Sync Control (8 bytes) - READ/WRITE
    /// Contains: start_index, count, command, status, chunk_size
    static let syncControlUUID = CBUUID(string: "6F75616B-7661-7465-2D00-000000000003")

    /// Drink Data (variable, max 206 bytes) - READ/NOTIFY
    /// Contains: chunk header + up to 20 drink records
    static let drinkDataUUID = CBUUID(string: "6F75616B-7661-7465-2D00-000000000004")

    /// Command (4 bytes) - WRITE
    /// Contains: command, param1, param2
    static let commandUUID = CBUUID(string: "6F75616B-7661-7465-2D00-000000000005")

    // MARK: - All Services to Discover

    /// Services to discover on connection
    static let servicesToDiscover: [CBUUID] = [
        deviceInfoServiceUUID,
        batteryServiceUUID,
        aquavateServiceUUID
    ]

    // MARK: - Characteristics to Discover

    /// Aquavate characteristics to discover
    static let aquavateCharacteristics: [CBUUID] = [
        currentStateUUID,
        bottleConfigUUID,
        syncControlUUID,
        drinkDataUUID,
        commandUUID
    ]

    /// Battery characteristics to discover
    static let batteryCharacteristics: [CBUUID] = [
        batteryLevelUUID
    ]

    // MARK: - Command Types (matches firmware BLE_CMD_*)

    enum Command: UInt8 {
        case tareNow = 0x01
        case startCalibration = 0x02
        case calibratePoint = 0x03
        case cancelCalibration = 0x04
        case resetDaily = 0x05
        case clearHistory = 0x06
        case setTime = 0x10  // Time sync command
    }

    // MARK: - Sync Control Commands

    enum SyncCommand: UInt8 {
        case query = 0
        case start = 1
        case ack = 2
    }

    // MARK: - Current State Flags

    struct StateFlags: OptionSet {
        let rawValue: UInt8

        static let timeValid = StateFlags(rawValue: 0x01)
        static let calibrated = StateFlags(rawValue: 0x02)
        static let stable = StateFlags(rawValue: 0x04)
    }

    // MARK: - State Restoration

    /// Key for CBCentralManager state restoration
    static let centralManagerRestoreIdentifier = "com.aquavate.centralmanager"

    /// Key for storing last connected peripheral identifier
    static let lastConnectedPeripheralKey = "lastConnectedPeripheralIdentifier"
}
