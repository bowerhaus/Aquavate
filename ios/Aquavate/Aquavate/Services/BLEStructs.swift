//
//  BLEStructs.swift
//  Aquavate
//
//  Binary struct definitions matching firmware BLE characteristic data.
//  All structs use little-endian byte order (matches ESP32 and iOS).
//

import Foundation

// MARK: - Current State Characteristic (14 bytes)

/// Current State data received from firmware (READ/NOTIFY)
/// Matches firmware's BLE_CurrentState struct exactly
struct BLECurrentState {
    let timestamp: UInt32           // Unix time (seconds)
    let currentWeightG: Int16       // Current bottle weight in grams
    let bottleLevelMl: UInt16       // Water level after last event
    let dailyTotalMl: UInt16        // Today's cumulative intake
    let batteryPercent: UInt8       // 0-100
    let flags: UInt8                // Bit 0: time_valid, Bit 1: calibrated, Bit 2: stable
    let unsyncedCount: UInt16       // Records pending sync

    /// Size of the binary data (must be 14 bytes)
    static let size = 14

    /// Parse from raw BLE data
    static func parse(from data: Data) -> BLECurrentState? {
        guard data.count == size else {
            return nil
        }

        return data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> BLECurrentState? in
            guard let baseAddress = ptr.baseAddress else { return nil }

            // Use loadUnaligned for packed struct data to avoid alignment crashes
            return BLECurrentState(
                timestamp: baseAddress.loadUnaligned(as: UInt32.self),
                currentWeightG: baseAddress.loadUnaligned(fromByteOffset: 4, as: Int16.self),
                bottleLevelMl: baseAddress.loadUnaligned(fromByteOffset: 6, as: UInt16.self),
                dailyTotalMl: baseAddress.loadUnaligned(fromByteOffset: 8, as: UInt16.self),
                batteryPercent: baseAddress.load(fromByteOffset: 10, as: UInt8.self),
                flags: baseAddress.load(fromByteOffset: 11, as: UInt8.self),
                unsyncedCount: baseAddress.loadUnaligned(fromByteOffset: 12, as: UInt16.self)
            )
        }
    }

    // MARK: - Flag accessors

    var isTimeValid: Bool {
        (flags & 0x01) != 0
    }

    var isCalibrated: Bool {
        (flags & 0x02) != 0
    }

    var isStable: Bool {
        (flags & 0x04) != 0
    }

    /// Convert timestamp to Date
    var date: Date {
        Date(timeIntervalSince1970: TimeInterval(timestamp))
    }
}

// MARK: - Bottle Config Characteristic (12 bytes)

/// Bottle configuration data (READ/WRITE)
/// Matches firmware's BLE_BottleConfig struct exactly
struct BLEBottleConfig {
    let scaleFactor: Float          // ADC counts per gram
    let tareWeightGrams: Int32      // Empty bottle weight
    let bottleCapacityMl: UInt16    // Max capacity (default 830ml)
    let dailyGoalMl: UInt16         // User's daily target (default 2000ml)

    /// Size of the binary data (must be 12 bytes)
    static let size = 12

    /// Parse from raw BLE data
    static func parse(from data: Data) -> BLEBottleConfig? {
        guard data.count == size else {
            return nil
        }

        return data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> BLEBottleConfig? in
            guard let baseAddress = ptr.baseAddress else { return nil }

            // Use loadUnaligned for packed struct data to avoid alignment crashes
            return BLEBottleConfig(
                scaleFactor: baseAddress.loadUnaligned(as: Float.self),
                tareWeightGrams: baseAddress.loadUnaligned(fromByteOffset: 4, as: Int32.self),
                bottleCapacityMl: baseAddress.loadUnaligned(fromByteOffset: 8, as: UInt16.self),
                dailyGoalMl: baseAddress.loadUnaligned(fromByteOffset: 10, as: UInt16.self)
            )
        }
    }

    /// Serialize to binary data for BLE write
    func toData() -> Data {
        var data = Data(count: Self.size)
        data.withUnsafeMutableBytes { ptr in
            guard let baseAddress = ptr.baseAddress else { return }
            baseAddress.storeBytes(of: scaleFactor, as: Float.self)
            baseAddress.storeBytes(of: tareWeightGrams, toByteOffset: 4, as: Int32.self)
            baseAddress.storeBytes(of: bottleCapacityMl, toByteOffset: 8, as: UInt16.self)
            baseAddress.storeBytes(of: dailyGoalMl, toByteOffset: 10, as: UInt16.self)
        }
        return data
    }
}

// MARK: - Sync Control Characteristic (8 bytes)

/// Sync control for drink history transfer (READ/WRITE)
/// Matches firmware's BLE_SyncControl struct exactly
struct BLESyncControl {
    let startIndex: UInt16          // Circular buffer index to start
    let count: UInt16               // Number of records to transfer
    let command: UInt8              // 0=query, 1=start, 2=ack
    let status: UInt8               // 0=idle, 1=in_progress, 2=complete
    let chunkSize: UInt16           // Records per chunk (default 20)

    /// Size of the binary data (must be 8 bytes)
    static let size = 8

    /// Sync commands
    enum Command: UInt8 {
        case query = 0
        case start = 1
        case ack = 2
    }

    /// Sync status
    enum Status: UInt8 {
        case idle = 0
        case inProgress = 1
        case complete = 2
    }

    /// Parse from raw BLE data
    static func parse(from data: Data) -> BLESyncControl? {
        guard data.count == size else {
            return nil
        }

        return data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> BLESyncControl? in
            guard let baseAddress = ptr.baseAddress else { return nil }

            // Use loadUnaligned for packed struct data to avoid alignment crashes
            return BLESyncControl(
                startIndex: baseAddress.loadUnaligned(as: UInt16.self),
                count: baseAddress.loadUnaligned(fromByteOffset: 2, as: UInt16.self),
                command: baseAddress.load(fromByteOffset: 4, as: UInt8.self),
                status: baseAddress.load(fromByteOffset: 5, as: UInt8.self),
                chunkSize: baseAddress.loadUnaligned(fromByteOffset: 6, as: UInt16.self)
            )
        }
    }

    /// Serialize to binary data for BLE write
    func toData() -> Data {
        var data = Data(count: Self.size)
        data.withUnsafeMutableBytes { ptr in
            guard let baseAddress = ptr.baseAddress else { return }
            baseAddress.storeBytes(of: startIndex, as: UInt16.self)
            baseAddress.storeBytes(of: count, toByteOffset: 2, as: UInt16.self)
            baseAddress.storeBytes(of: command, toByteOffset: 4, as: UInt8.self)
            baseAddress.storeBytes(of: status, toByteOffset: 5, as: UInt8.self)
            baseAddress.storeBytes(of: chunkSize, toByteOffset: 6, as: UInt16.self)
        }
        return data
    }

    /// Create a START command
    static func startCommand(count: UInt16, chunkSize: UInt16 = 20) -> BLESyncControl {
        BLESyncControl(
            startIndex: 0,
            count: count,
            command: Command.start.rawValue,
            status: Status.idle.rawValue,
            chunkSize: chunkSize
        )
    }

    /// Create an ACK command
    static func ackCommand() -> BLESyncControl {
        BLESyncControl(
            startIndex: 0,
            count: 0,
            command: Command.ack.rawValue,
            status: Status.idle.rawValue,
            chunkSize: 0
        )
    }
}

// MARK: - Drink Record (10 bytes)

/// Individual drink record for sync transfer
/// Matches firmware's BLE_DrinkRecord struct exactly
struct BLEDrinkRecord {
    let timestamp: UInt32           // Unix time
    let amountMl: Int16             // Consumed (+) or refilled (-)
    let bottleLevelMl: UInt16       // Level after event
    let type: UInt8                 // 0=gulp, 1=pour
    let flags: UInt8                // 0x01=synced

    /// Size of a single record (must be 10 bytes)
    static let size = 10

    /// Drink types
    enum DrinkType: UInt8 {
        case gulp = 0
        case pour = 1

        var description: String {
            switch self {
            case .gulp: return "Gulp"
            case .pour: return "Pour"
            }
        }
    }

    /// Get the drink type enum
    var drinkType: DrinkType {
        DrinkType(rawValue: type) ?? .gulp
    }

    /// Check if record has been synced
    var isSynced: Bool {
        (flags & 0x01) != 0
    }

    /// Convert timestamp to Date
    var date: Date {
        Date(timeIntervalSince1970: TimeInterval(timestamp))
    }
}

// MARK: - Drink Data Chunk (variable size, max 206 bytes)

/// Chunk of drink records for bulk transfer
/// Matches firmware's BLE_DrinkDataChunk struct
struct BLEDrinkDataChunk {
    let chunkIndex: UInt16          // Current chunk number
    let totalChunks: UInt16         // Total chunks in sync
    let recordCount: UInt8          // Records in this chunk (1-20)
    let records: [BLEDrinkRecord]   // Parsed drink records

    /// Header size (6 bytes: chunk_index + total_chunks + record_count + reserved)
    static let headerSize = 6

    /// Maximum records per chunk
    static let maxRecordsPerChunk = 20

    /// Parse from raw BLE data
    static func parse(from data: Data) -> BLEDrinkDataChunk? {
        guard data.count >= headerSize else {
            return nil
        }

        return data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> BLEDrinkDataChunk? in
            guard let baseAddress = ptr.baseAddress else { return nil }

            // Use loadUnaligned for packed struct data to avoid alignment crashes
            let chunkIndex = baseAddress.loadUnaligned(as: UInt16.self)
            let totalChunks = baseAddress.loadUnaligned(fromByteOffset: 2, as: UInt16.self)
            let recordCount = baseAddress.load(fromByteOffset: 4, as: UInt8.self)

            // Validate expected size: header (6 bytes) + records (10 bytes each)
            let expectedSize = headerSize + (Int(recordCount) * BLEDrinkRecord.size)
            guard data.count >= expectedSize else {
                // Data too short for declared record count
                return nil
            }

            // Parse individual records (use loadUnaligned for multi-byte types)
            var records: [BLEDrinkRecord] = []
            for i in 0..<Int(recordCount) {
                let offset = headerSize + (i * BLEDrinkRecord.size)

                let record = BLEDrinkRecord(
                    timestamp: baseAddress.loadUnaligned(fromByteOffset: offset, as: UInt32.self),
                    amountMl: baseAddress.loadUnaligned(fromByteOffset: offset + 4, as: Int16.self),
                    bottleLevelMl: baseAddress.loadUnaligned(fromByteOffset: offset + 6, as: UInt16.self),
                    type: baseAddress.load(fromByteOffset: offset + 8, as: UInt8.self),
                    flags: baseAddress.load(fromByteOffset: offset + 9, as: UInt8.self)
                )
                records.append(record)
            }

            return BLEDrinkDataChunk(
                chunkIndex: chunkIndex,
                totalChunks: totalChunks,
                recordCount: recordCount,
                records: records
            )
        }
    }

    /// Check if this is the last chunk
    var isLastChunk: Bool {
        chunkIndex + 1 >= totalChunks
    }
}

// MARK: - Command Characteristic (4 bytes)

/// Command to send to firmware (WRITE)
/// Matches firmware's BLE_Command struct exactly
struct BLECommand {
    let command: UInt8              // Command type
    let param1: UInt8               // Parameter 1
    let param2: UInt16              // Parameter 2

    /// Size of the binary data (must be 4 bytes)
    static let size = 4

    /// Serialize to binary data for BLE write
    func toData() -> Data {
        var data = Data(count: Self.size)
        data.withUnsafeMutableBytes { ptr in
            guard let baseAddress = ptr.baseAddress else { return }
            baseAddress.storeBytes(of: command, as: UInt8.self)
            baseAddress.storeBytes(of: param1, toByteOffset: 1, as: UInt8.self)
            baseAddress.storeBytes(of: param2, toByteOffset: 2, as: UInt16.self)
        }
        return data
    }

    // MARK: - Command Factories

    /// Create TARE_NOW command (0x01)
    static func tareNow() -> BLECommand {
        BLECommand(command: 0x01, param1: 0, param2: 0)
    }

    /// Create RESET_DAILY command (0x05)
    static func resetDaily() -> BLECommand {
        BLECommand(command: 0x05, param1: 0, param2: 0)
    }

    /// Create CLEAR_HISTORY command (0x06)
    static func clearHistory() -> BLECommand {
        BLECommand(command: 0x06, param1: 0, param2: 0)
    }

    /// Create START_CALIBRATION command (0x02)
    static func startCalibration() -> BLECommand {
        BLECommand(command: 0x02, param1: 0, param2: 0)
    }

    /// Create CANCEL_CALIBRATION command (0x04)
    static func cancelCalibration() -> BLECommand {
        BLECommand(command: 0x04, param1: 0, param2: 0)
    }

    /// Create SET_TIME command (0x10) with Unix timestamp
    /// Note: param2 is only 16 bits, so we need to pack timestamp differently
    /// The firmware expects the full 32-bit timestamp, so we'll use a different approach
    static func setTime(timestamp: UInt32) -> Data {
        // For time sync, we send 5 bytes: command + 4-byte timestamp
        var data = Data(count: 5)
        data.withUnsafeMutableBytes { ptr in
            guard let baseAddress = ptr.baseAddress else { return }
            baseAddress.storeBytes(of: UInt8(0x10), as: UInt8.self)
            baseAddress.storeBytes(of: timestamp, toByteOffset: 1, as: UInt32.self)
        }
        return data
    }

    /// Create SET_DAILY_TOTAL command (0x11) with daily total in ml
    /// Sends 3 bytes: command + 2-byte little-endian value
    static func setDailyTotal(ml: UInt16) -> Data {
        var data = Data(count: 3)
        data.withUnsafeMutableBytes { ptr in
            guard let baseAddress = ptr.baseAddress else { return }
            baseAddress.storeBytes(of: UInt8(0x11), as: UInt8.self)
            baseAddress.storeBytes(of: ml, toByteOffset: 1, as: UInt16.self)
        }
        return data
    }
}
