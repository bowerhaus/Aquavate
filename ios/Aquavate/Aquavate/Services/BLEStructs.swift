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

    /// Whether a calibration measurement is in progress (bit 3)
    var isCalMeasuring: Bool {
        (flags & 0x08) != 0
    }

    /// Whether calibration ADC result is ready to read (bit 4)
    /// When true, the raw ADC value is encoded in currentWeightG (lower 16 bits) and bottleLevelMl (upper 16 bits)
    var isCalResultReady: Bool {
        (flags & 0x10) != 0
    }

    /// Extract raw ADC value when calibration result is ready
    /// The 32-bit ADC value is split across currentWeightG (lower 16 bits) and bottleLevelMl (upper 16 bits)
    var calibrationRawADC: Int32? {
        guard isCalResultReady else { return nil }
        // Reconstruct 32-bit value: bottleLevelMl is upper 16 bits, currentWeightG is lower 16 bits (as unsigned)
        let lower = UInt16(bitPattern: currentWeightG)
        let upper = bottleLevelMl
        return Int32(bitPattern: (UInt32(upper) << 16) | UInt32(lower))
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

// MARK: - Drink Record (14 bytes)

/// Individual drink record for sync transfer
/// Matches firmware's BLE_DrinkRecord struct exactly
struct BLEDrinkRecord {
    let recordId: UInt32            // Unique ID for deletion
    let timestamp: UInt32           // Unix time
    let amountMl: Int16             // Consumed (+) or refilled (-)
    let bottleLevelMl: UInt16       // Level after event
    let type: UInt8                 // 0=gulp, 1=pour
    let flags: UInt8                // 0x01=synced, 0x04=deleted

    /// Size of a single record (must be 14 bytes)
    static let size = 14

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

    /// Check if record has been deleted (soft delete)
    var isDeleted: Bool {
        (flags & 0x04) != 0
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

            // Validate expected size: header (6 bytes) + records (14 bytes each)
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
                    recordId: baseAddress.loadUnaligned(fromByteOffset: offset, as: UInt32.self),
                    timestamp: baseAddress.loadUnaligned(fromByteOffset: offset + 4, as: UInt32.self),
                    amountMl: baseAddress.loadUnaligned(fromByteOffset: offset + 8, as: Int16.self),
                    bottleLevelMl: baseAddress.loadUnaligned(fromByteOffset: offset + 10, as: UInt16.self),
                    type: baseAddress.load(fromByteOffset: offset + 12, as: UInt8.self),
                    flags: baseAddress.load(fromByteOffset: offset + 13, as: UInt8.self)
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

    /// Create PING command (0x02) - keep-alive to reset activity timeout
    static func ping() -> BLECommand {
        BLECommand(command: 0x02, param1: 0, param2: 0)
    }

    /// Create RESET_DAILY command (0x05)
    static func resetDaily() -> BLECommand {
        BLECommand(command: 0x05, param1: 0, param2: 0)
    }

    /// Create CLEAR_HISTORY command (0x06)
    static func clearHistory() -> BLECommand {
        BLECommand(command: 0x06, param1: 0, param2: 0)
    }

    /// Calibration point types for iOS-driven calibration
    enum CalibrationPointType: UInt8 {
        case empty = 0  // Empty bottle measurement
        case full = 1   // Full bottle measurement
    }

    /// Create CAL_MEASURE_POINT command (0x03) - Request stable ADC measurement
    /// The firmware will take a 10-second stable measurement and return the raw ADC in the Current State notification
    static func calMeasurePoint(pointType: CalibrationPointType) -> BLECommand {
        BLECommand(command: 0x03, param1: pointType.rawValue, param2: 0)
    }

    /// Create CAL_SET_DATA command (0x04) - Save calibration to firmware NVS
    /// Sends 13 bytes: command (1) + emptyADC (4) + fullADC (4) + scaleFactor (4)
    /// All values are little-endian
    static func calSetData(emptyADC: Int32, fullADC: Int32, scaleFactor: Float) -> Data {
        var data = Data(count: 13)
        data.withUnsafeMutableBytes { ptr in
            guard let baseAddress = ptr.baseAddress else { return }
            baseAddress.storeBytes(of: UInt8(0x04), as: UInt8.self)
            baseAddress.storeBytes(of: emptyADC, toByteOffset: 1, as: Int32.self)
            baseAddress.storeBytes(of: fullADC, toByteOffset: 5, as: Int32.self)
            baseAddress.storeBytes(of: scaleFactor, toByteOffset: 9, as: Float.self)
        }
        return data
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

    /// Create DELETE_DRINK_RECORD command (0x12) with record ID
    /// Sends 5 bytes: command + 4-byte little-endian record_id
    /// The firmware will soft-delete the record and recalculate the daily total
    static func deleteDrinkRecord(recordId: UInt32) -> Data {
        var data = Data(count: 5)
        data.withUnsafeMutableBytes { ptr in
            guard let baseAddress = ptr.baseAddress else { return }
            baseAddress.storeBytes(of: UInt8(0x12), as: UInt8.self)
            baseAddress.storeBytes(of: recordId, toByteOffset: 1, as: UInt32.self)
        }
        return data
    }

    // MARK: - Calibration Commands (Plan 060 - Bottle-Driven iOS Calibration)

    /// Create START_CALIBRATION command (0x20) - triggers bottle calibration state machine
    static func startCalibration() -> BLECommand {
        BLECommand(command: 0x20, param1: 0, param2: 0)
    }

    /// Create CANCEL_CALIBRATION command (0x21) - cancels calibration and returns to idle
    static func cancelCalibration() -> BLECommand {
        BLECommand(command: 0x21, param1: 0, param2: 0)
    }

    // MARK: - Activity Stats Commands

    /// Create GET_ACTIVITY_SUMMARY command (0x30)
    static func getActivitySummary() -> BLECommand {
        BLECommand(command: 0x30, param1: 0, param2: 0)
    }

    /// Create GET_MOTION_CHUNK command (0x31) with chunk index
    static func getMotionChunk(index: UInt8) -> BLECommand {
        BLECommand(command: 0x31, param1: index, param2: 0)
    }

    /// Create GET_BACKPACK_CHUNK command (0x32) with chunk index
    static func getBackpackChunk(index: UInt8) -> BLECommand {
        BLECommand(command: 0x32, param1: index, param2: 0)
    }
}

// MARK: - Device Settings Characteristic (4 bytes)

/// Device settings data (READ/WRITE)
/// Matches firmware's BLE_DeviceSettings struct exactly
struct BLEDeviceSettings {
    let flags: UInt8                // Device settings flags (bit 0: shake_to_empty_enabled)
    let reserved1: UInt8            // Reserved for future use
    let reserved2: UInt16           // Reserved for future use

    /// Size of the binary data (must be 4 bytes)
    static let size = 4

    // MARK: - Flag constants (matches firmware DEVICE_SETTINGS_FLAG_*)

    /// Shake-to-empty enabled flag (bit 0)
    static let flagShakeToEmptyEnabled: UInt8 = 0x01

    // MARK: - Flag accessors

    /// Check if shake-to-empty gesture is enabled
    var isShakeToEmptyEnabled: Bool {
        (flags & Self.flagShakeToEmptyEnabled) != 0
    }

    /// Parse from raw BLE data
    static func parse(from data: Data) -> BLEDeviceSettings? {
        guard data.count == size else {
            return nil
        }

        return data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> BLEDeviceSettings? in
            guard let baseAddress = ptr.baseAddress else { return nil }

            return BLEDeviceSettings(
                flags: baseAddress.load(as: UInt8.self),
                reserved1: baseAddress.load(fromByteOffset: 1, as: UInt8.self),
                reserved2: baseAddress.loadUnaligned(fromByteOffset: 2, as: UInt16.self)
            )
        }
    }

    /// Create with specific settings
    static func create(shakeToEmptyEnabled: Bool) -> BLEDeviceSettings {
        var flags: UInt8 = 0
        if shakeToEmptyEnabled {
            flags |= flagShakeToEmptyEnabled
        }
        return BLEDeviceSettings(flags: flags, reserved1: 0, reserved2: 0)
    }

    /// Serialize to binary data for BLE write
    func toData() -> Data {
        var data = Data(count: Self.size)
        data.withUnsafeMutableBytes { ptr in
            guard let baseAddress = ptr.baseAddress else { return }
            baseAddress.storeBytes(of: flags, as: UInt8.self)
            baseAddress.storeBytes(of: reserved1, toByteOffset: 1, as: UInt8.self)
            baseAddress.storeBytes(of: reserved2, toByteOffset: 2, as: UInt16.self)
        }
        return data
    }
}

// MARK: - Activity Stats Structures (Issue #36)

/// Activity Summary (12 bytes) - first response to GET_ACTIVITY_SUMMARY
struct BLEActivitySummary {
    let motionEventCount: UInt8         // Number of motion wake events stored
    let backpackSessionCount: UInt8     // Number of backpack sessions stored
    let inBackpackMode: UInt8           // 1 if currently in backpack mode
    let flags: UInt8                    // Bit 0: time_valid
    let currentSessionStart: UInt32     // If in backpack mode, when it started
    let currentTimerWakes: UInt16       // Timer wakes in current session
    let reserved: UInt16

    static let size = 12

    static func parse(from data: Data) -> BLEActivitySummary? {
        guard data.count >= size else { return nil }

        return data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> BLEActivitySummary? in
            guard let baseAddress = ptr.baseAddress else { return nil }

            return BLEActivitySummary(
                motionEventCount: baseAddress.load(as: UInt8.self),
                backpackSessionCount: baseAddress.load(fromByteOffset: 1, as: UInt8.self),
                inBackpackMode: baseAddress.load(fromByteOffset: 2, as: UInt8.self),
                flags: baseAddress.load(fromByteOffset: 3, as: UInt8.self),
                currentSessionStart: baseAddress.loadUnaligned(fromByteOffset: 4, as: UInt32.self),
                currentTimerWakes: baseAddress.loadUnaligned(fromByteOffset: 8, as: UInt16.self),
                reserved: baseAddress.loadUnaligned(fromByteOffset: 10, as: UInt16.self)
            )
        }
    }

    var isInBackpackMode: Bool { inBackpackMode != 0 }
    var isTimeValid: Bool { (flags & 0x01) != 0 }

    var currentSessionStartDate: Date? {
        guard isInBackpackMode && currentSessionStart > 0 else { return nil }
        return Date(timeIntervalSince1970: TimeInterval(currentSessionStart))
    }
}

/// Motion Wake Event (8 bytes)
struct BLEMotionWakeEvent {
    let timestamp: UInt32              // Unix timestamp when wake occurred
    let durationSec: UInt16            // How long device stayed awake
    let wakeReason: UInt8              // 0=motion, 1=timer, 2=power_on
    let sleepType: UInt8               // Bits 0-6: sleep type, Bit 7: drink taken flag

    static let size = 8

    // Bit masks for sleepType field (matches firmware SLEEP_TYPE_* constants)
    private static let drinkTakenFlag: UInt8 = 0x80  // Bit 7
    private static let sleepTypeMask: UInt8 = 0x7F   // Bits 0-6

    enum WakeReason: UInt8 {
        case motion = 0
        case timer = 1
        case powerOn = 2
        case other = 3
    }

    enum SleepType: UInt8 {
        case normal = 0
        case extended = 1
    }

    var date: Date { Date(timeIntervalSince1970: TimeInterval(timestamp)) }
    var reason: WakeReason { WakeReason(rawValue: wakeReason) ?? .other }
    var enteredSleepType: SleepType { SleepType(rawValue: sleepType & Self.sleepTypeMask) ?? .normal }
    var drinkTaken: Bool { (sleepType & Self.drinkTakenFlag) != 0 }
}

/// Backpack Session (12 bytes)
struct BLEBackpackSession {
    let startTimestamp: UInt32         // Unix timestamp when session started
    let durationSec: UInt32            // Total time in backpack mode
    let timerWakeCount: UInt16         // Number of timer wakes during session
    let exitReason: UInt8              // 0=motion_detected, 1=still_active
    let flags: UInt8                   // Reserved

    static let size = 12

    enum ExitReason: UInt8 {
        case motionDetected = 0
        case stillActive = 1
        case powerCycle = 2
    }

    var startDate: Date { Date(timeIntervalSince1970: TimeInterval(startTimestamp)) }
    var exit: ExitReason { ExitReason(rawValue: exitReason) ?? .motionDetected }
}

/// Motion Event Chunk (variable size, max 84 bytes)
struct BLEMotionEventChunk {
    let chunkIndex: UInt8              // Current chunk (0-9 for 100 events)
    let totalChunks: UInt8             // Total chunks available
    let eventCount: UInt8              // Events in this chunk (1-10)
    let events: [BLEMotionWakeEvent]

    static let headerSize = 4
    static let maxEventsPerChunk = 10

    static func parse(from data: Data) -> BLEMotionEventChunk? {
        guard data.count >= headerSize else { return nil }

        return data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> BLEMotionEventChunk? in
            guard let baseAddress = ptr.baseAddress else { return nil }

            let chunkIndex = baseAddress.load(as: UInt8.self)
            let totalChunks = baseAddress.load(fromByteOffset: 1, as: UInt8.self)
            let eventCount = baseAddress.load(fromByteOffset: 2, as: UInt8.self)

            let expectedSize = headerSize + (Int(eventCount) * BLEMotionWakeEvent.size)
            guard data.count >= expectedSize else { return nil }

            var events: [BLEMotionWakeEvent] = []
            for i in 0..<Int(eventCount) {
                let offset = headerSize + (i * BLEMotionWakeEvent.size)
                let event = BLEMotionWakeEvent(
                    timestamp: baseAddress.loadUnaligned(fromByteOffset: offset, as: UInt32.self),
                    durationSec: baseAddress.loadUnaligned(fromByteOffset: offset + 4, as: UInt16.self),
                    wakeReason: baseAddress.load(fromByteOffset: offset + 6, as: UInt8.self),
                    sleepType: baseAddress.load(fromByteOffset: offset + 7, as: UInt8.self)
                )
                events.append(event)
            }

            return BLEMotionEventChunk(
                chunkIndex: chunkIndex,
                totalChunks: totalChunks,
                eventCount: eventCount,
                events: events
            )
        }
    }

    var isLastChunk: Bool { chunkIndex + 1 >= totalChunks }
}

/// Backpack Session Chunk (variable size, max 64 bytes)
struct BLEBackpackSessionChunk {
    let chunkIndex: UInt8              // Current chunk (0-3 for 20 sessions)
    let totalChunks: UInt8             // Total chunks available
    let sessionCount: UInt8            // Sessions in this chunk (1-5)
    let sessions: [BLEBackpackSession]

    static let headerSize = 4
    static let maxSessionsPerChunk = 5

    static func parse(from data: Data) -> BLEBackpackSessionChunk? {
        guard data.count >= headerSize else { return nil }

        return data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> BLEBackpackSessionChunk? in
            guard let baseAddress = ptr.baseAddress else { return nil }

            let chunkIndex = baseAddress.load(as: UInt8.self)
            let totalChunks = baseAddress.load(fromByteOffset: 1, as: UInt8.self)
            let sessionCount = baseAddress.load(fromByteOffset: 2, as: UInt8.self)

            let expectedSize = headerSize + (Int(sessionCount) * BLEBackpackSession.size)
            guard data.count >= expectedSize else { return nil }

            var sessions: [BLEBackpackSession] = []
            for i in 0..<Int(sessionCount) {
                let offset = headerSize + (i * BLEBackpackSession.size)
                let session = BLEBackpackSession(
                    startTimestamp: baseAddress.loadUnaligned(fromByteOffset: offset, as: UInt32.self),
                    durationSec: baseAddress.loadUnaligned(fromByteOffset: offset + 4, as: UInt32.self),
                    timerWakeCount: baseAddress.loadUnaligned(fromByteOffset: offset + 8, as: UInt16.self),
                    exitReason: baseAddress.load(fromByteOffset: offset + 10, as: UInt8.self),
                    flags: baseAddress.load(fromByteOffset: offset + 11, as: UInt8.self)
                )
                sessions.append(session)
            }

            return BLEBackpackSessionChunk(
                chunkIndex: chunkIndex,
                totalChunks: totalChunks,
                sessionCount: sessionCount,
                sessions: sessions
            )
        }
    }

    var isLastChunk: Bool { chunkIndex + 1 >= totalChunks }
}

// MARK: - Calibration State Characteristic (12 bytes) - Plan 060

/// Calibration state notification from bottle-driven calibration
/// Matches firmware's BLE_CalibrationState struct exactly
struct BLECalibrationState {
    let state: UInt8                // CalibrationState enum value
    let flags: UInt8                // Bit 0: error occurred
    let emptyADC: Int32             // Captured empty ADC (0 if not yet measured)
    let fullADC: Int32              // Captured full ADC (0 if not yet measured)
    let reserved: UInt16            // Padding/future use

    /// Size of the binary data (must be 12 bytes)
    static let size = 12

    // State value constants (matches firmware CalibrationState enum)
    static let stateIdle: UInt8 = 0
    static let stateTriggered: UInt8 = 1
    static let stateStarted: UInt8 = 2
    static let stateWaitEmpty: UInt8 = 3
    static let stateMeasureEmpty: UInt8 = 4
    static let stateConfirmEmpty: UInt8 = 5
    static let stateWaitFull: UInt8 = 6
    static let stateMeasureFull: UInt8 = 7
    static let stateConfirmFull: UInt8 = 8
    static let stateComplete: UInt8 = 9
    static let stateError: UInt8 = 10

    /// Parse from raw BLE data
    static func parse(from data: Data) -> BLECalibrationState? {
        guard data.count == size else {
            return nil
        }

        return data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> BLECalibrationState? in
            guard let baseAddress = ptr.baseAddress else { return nil }

            return BLECalibrationState(
                state: baseAddress.load(as: UInt8.self),
                flags: baseAddress.load(fromByteOffset: 1, as: UInt8.self),
                emptyADC: baseAddress.loadUnaligned(fromByteOffset: 2, as: Int32.self),
                fullADC: baseAddress.loadUnaligned(fromByteOffset: 6, as: Int32.self),
                reserved: baseAddress.loadUnaligned(fromByteOffset: 10, as: UInt16.self)
            )
        }
    }

    // MARK: - State accessors

    /// Check if an error occurred
    var hasError: Bool {
        (flags & 0x01) != 0
    }

    /// Check if calibration is idle
    var isIdle: Bool {
        state == Self.stateIdle
    }

    /// Check if calibration is active (not idle, not complete, not error)
    var isActive: Bool {
        state != Self.stateIdle && state != Self.stateComplete && state != Self.stateError
    }

    /// Check if calibration is complete (success)
    var isComplete: Bool {
        state == Self.stateComplete
    }

    /// Check if calibration is in error state
    var isError: Bool {
        state == Self.stateError || hasError
    }

    /// Check if waiting for empty bottle placement
    var isWaitingForEmpty: Bool {
        state == Self.stateWaitEmpty
    }

    /// Check if measuring empty bottle
    var isMeasuringEmpty: Bool {
        state == Self.stateMeasureEmpty
    }

    /// Check if waiting for full bottle placement
    var isWaitingForFull: Bool {
        state == Self.stateWaitFull
    }

    /// Check if measuring full bottle
    var isMeasuringFull: Bool {
        state == Self.stateMeasureFull
    }

    /// Human-readable state description
    var stateDescription: String {
        switch state {
        case Self.stateIdle: return "Idle"
        case Self.stateTriggered: return "Triggered"
        case Self.stateStarted: return "Starting"
        case Self.stateWaitEmpty: return "Place Empty Bottle"
        case Self.stateMeasureEmpty: return "Measuring Empty"
        case Self.stateConfirmEmpty: return "Confirm Empty"
        case Self.stateWaitFull: return "Fill and Place Bottle"
        case Self.stateMeasureFull: return "Measuring Full"
        case Self.stateConfirmFull: return "Confirm Full"
        case Self.stateComplete: return "Complete"
        case Self.stateError: return "Error"
        default: return "Unknown"
        }
    }
}
