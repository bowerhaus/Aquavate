//
//  BLEManager.swift
//  Aquavate
//
//  CoreBluetooth central manager for communicating with Aquavate firmware.
//  Phase 4.1: Scanning, connection, and service/characteristic discovery.
//  Phase 4.2: Current State and battery notifications.
//  Phase 4.4: Drink sync protocol (chunked transfer).
//

import CoreBluetooth
import CoreData
import Combine
import os.log

/// Connection state for BLE device
enum BLEConnectionState: String {
    case disconnected = "Disconnected"
    case scanning = "Scanning..."
    case connecting = "Connecting..."
    case discovering = "Discovering services..."
    case connected = "Connected"

    var isConnected: Bool {
        self == .connected
    }

    var isActive: Bool {
        self != .disconnected
    }
}

/// Sync state for drink history transfer
enum BLESyncState: String {
    case idle = "Idle"
    case requesting = "Requesting..."
    case receiving = "Receiving..."
    case complete = "Sync Complete"
    case failed = "Sync Failed"

    var isActive: Bool {
        self == .requesting || self == .receiving
    }
}

/// BLE Manager for Aquavate device communication
@MainActor
class BLEManager: NSObject, ObservableObject {

    // MARK: - Published Properties (Connection)

    /// Current connection state
    @Published private(set) var connectionState: BLEConnectionState = .disconnected

    /// Discovered Aquavate devices (name: peripheral)
    @Published private(set) var discoveredDevices: [String: CBPeripheral] = [:]

    /// Name of currently connected device
    @Published private(set) var connectedDeviceName: String?

    /// Error message for display
    @Published var errorMessage: String?

    /// Whether Bluetooth is available and powered on
    @Published private(set) var isBluetoothReady = false

    // MARK: - Published Properties (Current State - Phase 4.2)

    /// Current bottle weight in grams (from load cell)
    @Published private(set) var currentWeightG: Int = 0

    /// Current water level in bottle (ml)
    @Published private(set) var bottleLevelMl: Int = 0

    /// Today's cumulative water intake (ml)
    @Published private(set) var dailyTotalMl: Int = 0

    /// Battery percentage (0-100)
    @Published private(set) var batteryPercent: Int = 0

    /// Whether device time has been set
    @Published private(set) var isTimeValid: Bool = false

    /// Whether device is calibrated
    @Published private(set) var isCalibrated: Bool = false

    /// Whether weight reading is stable
    @Published private(set) var isStable: Bool = false

    /// Number of drink records pending sync
    @Published private(set) var unsyncedCount: Int = 0

    /// Timestamp of last Current State update
    @Published private(set) var lastStateUpdateTime: Date?

    // MARK: - Published Properties (Bottle Config)

    /// Bottle capacity in ml (from config)
    @Published private(set) var bottleCapacityMl: Int = 830

    /// Daily goal in ml (from config)
    @Published private(set) var dailyGoalMl: Int = 2000

    // MARK: - Published Properties (Sync - Phase 4.4)

    /// Current sync state
    @Published private(set) var syncState: BLESyncState = .idle

    /// Sync progress (0.0 to 1.0)
    @Published private(set) var syncProgress: Double = 0.0

    /// Number of records synced in current transfer
    @Published private(set) var syncedRecordCount: Int = 0

    /// Total records to sync in current transfer
    @Published private(set) var totalRecordsToSync: Int = 0

    /// Timestamp of last successful sync
    @Published private(set) var lastSyncTime: Date? {
        didSet {
            // Persist to UserDefaults
            if let time = lastSyncTime {
                UserDefaults.standard.set(time, forKey: "lastSyncTime")
            }
        }
    }

    // MARK: - Private Properties

    /// Buffer for drink records during sync
    private var syncBuffer: [BLEDrinkRecord] = []

    /// Expected total chunks in current sync
    private var expectedTotalChunks: UInt16 = 0

    /// Last received chunk index
    private var lastChunkIndex: UInt16 = 0

    /// Bottle ID for current sync (from CoreData)
    private var currentBottleId: UUID?

    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?
    private var scanTimer: Timer?
    private var connectionTimer: Timer?

    /// Discovered characteristics keyed by UUID
    private var characteristics: [CBUUID: CBCharacteristic] = [:]

    /// Logger for BLE events
    private let logger = Logger(subsystem: "com.aquavate", category: "BLE")

    // MARK: - Initialization

    override init() {
        super.init()

        // Load persisted last sync time
        lastSyncTime = UserDefaults.standard.object(forKey: "lastSyncTime") as? Date

        let options: [String: Any] = [
            CBCentralManagerOptionRestoreIdentifierKey: BLEConstants.centralManagerRestoreIdentifier,
            CBCentralManagerOptionShowPowerAlertKey: true
        ]

        centralManager = CBCentralManager(delegate: self, queue: nil, options: options)
        logger.info("BLEManager initialized")
    }

    // MARK: - Public API

    /// Start scanning for Aquavate devices
    func startScanning() {
        guard isBluetoothReady else {
            errorMessage = "Bluetooth is not available"
            logger.warning("Cannot scan: Bluetooth not ready")
            return
        }

        guard connectionState == .disconnected else {
            logger.info("Already scanning or connected, ignoring scan request")
            return
        }

        logger.info("Starting BLE scan for Aquavate devices")
        connectionState = .scanning
        discoveredDevices.removeAll()
        errorMessage = nil

        // Scan for devices advertising the Aquavate service
        centralManager.scanForPeripherals(
            withServices: [BLEConstants.aquavateServiceUUID],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )

        // Set scan timeout
        scanTimer?.invalidate()
        scanTimer = Timer.scheduledTimer(withTimeInterval: BLEConstants.scanTimeout, repeats: false) { [weak self] _ in
            Task { @MainActor in
                self?.handleScanTimeout()
            }
        }
    }

    /// Stop scanning for devices
    func stopScanning() {
        guard centralManager.isScanning else { return }

        logger.info("Stopping BLE scan")
        centralManager.stopScan()
        scanTimer?.invalidate()
        scanTimer = nil

        if connectionState == .scanning {
            connectionState = .disconnected
        }
    }

    /// Connect to a specific peripheral
    func connect(to peripheral: CBPeripheral) {
        stopScanning()

        logger.info("Connecting to peripheral: \(peripheral.name ?? "Unknown")")
        connectionState = .connecting
        connectedPeripheral = peripheral
        connectedDeviceName = peripheral.name

        centralManager.connect(peripheral, options: nil)

        // Set connection timeout
        connectionTimer?.invalidate()
        connectionTimer = Timer.scheduledTimer(withTimeInterval: BLEConstants.connectionTimeout, repeats: false) { [weak self] _ in
            Task { @MainActor in
                self?.handleConnectionTimeout()
            }
        }
    }

    /// Connect to device by name (convenience method)
    func connect(toDeviceNamed name: String) {
        guard let peripheral = discoveredDevices[name] else {
            errorMessage = "Device not found: \(name)"
            return
        }
        connect(to: peripheral)
    }

    /// Attempt to reconnect to last known device
    func reconnectToLastDevice() {
        guard let identifierString = UserDefaults.standard.string(forKey: BLEConstants.lastConnectedPeripheralKey),
              let identifier = UUID(uuidString: identifierString) else {
            logger.info("No last connected device to reconnect to")
            return
        }

        let peripherals = centralManager.retrievePeripherals(withIdentifiers: [identifier])
        if let peripheral = peripherals.first {
            logger.info("Attempting to reconnect to last device: \(peripheral.identifier)")
            connect(to: peripheral)
        } else {
            logger.info("Last connected peripheral not found, starting scan")
            startScanning()
        }
    }

    /// Disconnect from current device
    func disconnect() {
        guard let peripheral = connectedPeripheral else { return }

        logger.info("Disconnecting from peripheral")
        centralManager.cancelPeripheralConnection(peripheral)
    }

    /// Check if a specific characteristic is available
    func hasCharacteristic(_ uuid: CBUUID) -> Bool {
        characteristics[uuid] != nil
    }

    // MARK: - Private Methods

    private func handleScanTimeout() {
        if connectionState == .scanning {
            stopScanning()
            if discoveredDevices.isEmpty {
                errorMessage = "No Aquavate devices found"
                logger.warning("Scan timeout: no devices found")
            }
        }
    }

    private func handleConnectionTimeout() {
        if connectionState == .connecting || connectionState == .discovering {
            logger.warning("Connection timeout")
            if let peripheral = connectedPeripheral {
                centralManager.cancelPeripheralConnection(peripheral)
            }
            connectionState = .disconnected
            errorMessage = "Connection timeout"
            connectedPeripheral = nil
            connectedDeviceName = nil
        }
    }

    private func discoverServices() {
        guard let peripheral = connectedPeripheral else { return }

        logger.info("Discovering services")
        connectionState = .discovering
        peripheral.delegate = self
        peripheral.discoverServices(BLEConstants.servicesToDiscover)
    }

    private func savePeripheralIdentifier(_ peripheral: CBPeripheral) {
        UserDefaults.standard.set(peripheral.identifier.uuidString, forKey: BLEConstants.lastConnectedPeripheralKey)
        logger.info("Saved peripheral identifier for reconnection")
    }

    private func clearSavedPeripheral() {
        UserDefaults.standard.removeObject(forKey: BLEConstants.lastConnectedPeripheralKey)
    }
}

// MARK: - CBCentralManagerDelegate

extension BLEManager: CBCentralManagerDelegate {

    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Task { @MainActor in
            switch central.state {
            case .poweredOn:
                logger.info("Bluetooth powered on")
                isBluetoothReady = true
                // Attempt to reconnect to last device on startup
                reconnectToLastDevice()

            case .poweredOff:
                logger.warning("Bluetooth powered off")
                isBluetoothReady = false
                connectionState = .disconnected
                errorMessage = "Bluetooth is turned off"

            case .unauthorized:
                logger.error("Bluetooth unauthorized")
                isBluetoothReady = false
                errorMessage = "Bluetooth permission denied"

            case .unsupported:
                logger.error("Bluetooth unsupported")
                isBluetoothReady = false
                errorMessage = "Bluetooth not supported on this device"

            case .resetting:
                logger.warning("Bluetooth resetting")
                isBluetoothReady = false

            case .unknown:
                logger.info("Bluetooth state unknown")
                isBluetoothReady = false

            @unknown default:
                logger.warning("Unknown Bluetooth state")
                isBluetoothReady = false
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                                    advertisementData: [String: Any], rssi RSSI: NSNumber) {
        Task { @MainActor in
            let deviceName = peripheral.name ?? "Unknown Aquavate"

            // Filter for Aquavate devices
            guard deviceName.hasPrefix(BLEConstants.deviceNamePrefix) else {
                logger.debug("Ignoring non-Aquavate device: \(deviceName)")
                return
            }

            logger.info("Discovered Aquavate device: \(deviceName) (RSSI: \(RSSI))")
            discoveredDevices[deviceName] = peripheral

            // Auto-connect if only one device found (common case)
            if discoveredDevices.count == 1 {
                connect(to: peripheral)
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        Task { @MainActor in
            logger.info("Connected to peripheral: \(peripheral.name ?? "Unknown")")
            connectionTimer?.invalidate()
            connectionTimer = nil
            savePeripheralIdentifier(peripheral)
            discoverServices()
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        Task { @MainActor in
            logger.error("Failed to connect: \(error?.localizedDescription ?? "Unknown error")")
            connectionTimer?.invalidate()
            connectionTimer = nil
            connectionState = .disconnected
            errorMessage = "Failed to connect: \(error?.localizedDescription ?? "Unknown error")"
            connectedPeripheral = nil
            connectedDeviceName = nil
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        Task { @MainActor in
            if let error = error {
                logger.warning("Disconnected with error: \(error.localizedDescription)")
                errorMessage = "Disconnected: \(error.localizedDescription)"
            } else {
                logger.info("Disconnected from peripheral")
            }

            connectionState = .disconnected
            connectedPeripheral = nil
            connectedDeviceName = nil
            characteristics.removeAll()
        }
    }

    // MARK: - State Restoration

    nonisolated func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        Task { @MainActor in
            logger.info("Restoring central manager state")

            if let peripherals = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral] {
                for peripheral in peripherals {
                    logger.info("Restored peripheral: \(peripheral.name ?? "Unknown")")
                    if peripheral.state == .connected {
                        connectedPeripheral = peripheral
                        connectedDeviceName = peripheral.name
                        peripheral.delegate = self
                        connectionState = .connected
                    }
                }
            }
        }
    }
}

// MARK: - CBPeripheralDelegate

extension BLEManager: CBPeripheralDelegate {

    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        Task { @MainActor in
            if let error = error {
                logger.error("Service discovery error: \(error.localizedDescription)")
                errorMessage = "Service discovery failed"
                return
            }

            guard let services = peripheral.services else {
                logger.warning("No services discovered")
                return
            }

            logger.info("Discovered \(services.count) services")

            for service in services {
                logger.debug("Service: \(service.uuid)")

                // Discover characteristics for each service
                if service.uuid == BLEConstants.aquavateServiceUUID {
                    peripheral.discoverCharacteristics(BLEConstants.aquavateCharacteristics, for: service)
                } else if service.uuid == BLEConstants.batteryServiceUUID {
                    peripheral.discoverCharacteristics(BLEConstants.batteryCharacteristics, for: service)
                } else if service.uuid == BLEConstants.deviceInfoServiceUUID {
                    peripheral.discoverCharacteristics(nil, for: service)
                }
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        Task { @MainActor in
            if let error = error {
                logger.error("Characteristic discovery error: \(error.localizedDescription)")
                return
            }

            guard let characteristics = service.characteristics else {
                logger.warning("No characteristics for service: \(service.uuid)")
                return
            }

            logger.info("Discovered \(characteristics.count) characteristics for service \(service.uuid)")

            for characteristic in characteristics {
                logger.debug("  Characteristic: \(characteristic.uuid), properties: \(characteristic.properties.rawValue)")

                // Store characteristic reference
                self.characteristics[characteristic.uuid] = characteristic

                // Subscribe to notifications for relevant characteristics
                if characteristic.properties.contains(.notify) {
                    if characteristic.uuid == BLEConstants.currentStateUUID ||
                       characteristic.uuid == BLEConstants.drinkDataUUID ||
                       characteristic.uuid == BLEConstants.batteryLevelUUID {
                        peripheral.setNotifyValue(true, for: characteristic)
                        logger.info("Subscribed to notifications for \(characteristic.uuid)")
                    }
                }

                // Read initial values
                if characteristic.properties.contains(.read) {
                    peripheral.readValue(for: characteristic)
                }
            }

            // Check if we have all required Aquavate characteristics
            checkDiscoveryComplete()
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        Task { @MainActor in
            if let error = error {
                logger.error("Value update error for \(characteristic.uuid): \(error.localizedDescription)")
                return
            }

            guard let data = characteristic.value else {
                logger.warning("No data for characteristic: \(characteristic.uuid)")
                return
            }

            logger.debug("Received \(data.count) bytes from \(characteristic.uuid)")

            // Parse characteristic data based on UUID
            switch characteristic.uuid {
            case BLEConstants.currentStateUUID:
                handleCurrentStateUpdate(data)

            case BLEConstants.batteryLevelUUID:
                handleBatteryLevelUpdate(data)

            case BLEConstants.drinkDataUUID:
                handleDrinkDataUpdate(data)

            case BLEConstants.bottleConfigUUID:
                handleBottleConfigUpdate(data)

            case BLEConstants.syncControlUUID:
                handleSyncControlUpdate(data)

            default:
                logger.debug("Received data from unknown characteristic: \(characteristic.uuid)")
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        Task { @MainActor in
            if let error = error {
                logger.error("Notification state error for \(characteristic.uuid): \(error.localizedDescription)")
                return
            }

            let status = characteristic.isNotifying ? "enabled" : "disabled"
            logger.info("Notifications \(status) for \(characteristic.uuid)")
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        Task { @MainActor in
            if let error = error {
                logger.error("Write error for \(characteristic.uuid): \(error.localizedDescription)")
                return
            }
            logger.debug("Successfully wrote to \(characteristic.uuid)")
        }
    }

    // MARK: - Private Helpers

    private func checkDiscoveryComplete() {
        // Check if we have all required Aquavate characteristics
        let required: [CBUUID] = [
            BLEConstants.currentStateUUID,
            BLEConstants.bottleConfigUUID,
            BLEConstants.syncControlUUID,
            BLEConstants.drinkDataUUID,
            BLEConstants.commandUUID
        ]

        let hasAll = required.allSatisfy { characteristics[$0] != nil }

        if hasAll {
            logger.info("All Aquavate characteristics discovered - connection complete")
            connectionState = .connected
            connectionTimer?.invalidate()
            connectionTimer = nil
            errorMessage = nil

            // Get bottle ID for sync
            currentBottleId = PersistenceController.shared.getOrCreateDefaultBottle().id

            // Sync device time on every connection
            syncDeviceTime()

            // Auto-sync will be triggered by handleCurrentStateUpdate when we receive unsyncedCount
        }
    }

    // MARK: - Characteristic Parsing (Phase 4.2)

    /// Parse and update Current State from BLE notification
    private func handleCurrentStateUpdate(_ data: Data) {
        guard let state = BLECurrentState.parse(from: data) else {
            logger.error("Failed to parse Current State (expected 14 bytes, got \(data.count))")
            return
        }

        let previousUnsyncedCount = unsyncedCount

        // Update published properties
        currentWeightG = Int(state.currentWeightG)
        bottleLevelMl = Int(state.bottleLevelMl)
        dailyTotalMl = Int(state.dailyTotalMl)
        batteryPercent = Int(state.batteryPercent)
        isTimeValid = state.isTimeValid
        isCalibrated = state.isCalibrated
        isStable = state.isStable
        unsyncedCount = Int(state.unsyncedCount)
        lastStateUpdateTime = Date()

        logger.info("Current State: weight=\(state.currentWeightG)g, level=\(state.bottleLevelMl)ml, daily=\(state.dailyTotalMl)ml, battery=\(state.batteryPercent)%, unsynced=\(state.unsyncedCount)")
        logger.debug("  Flags: timeValid=\(state.isTimeValid), calibrated=\(state.isCalibrated), stable=\(state.isStable)")

        // Auto-sync if we have unsynced records and sync is not already in progress
        // Only trigger on first state update with unsynced records (previousUnsyncedCount == 0)
        if unsyncedCount > 0 && previousUnsyncedCount == 0 && !syncState.isActive && connectionState.isConnected {
            logger.info("Detected \(self.unsyncedCount) unsynced records, starting auto-sync")
            startDrinkSync()
        }
    }

    /// Parse and update Battery Level from BLE notification
    private func handleBatteryLevelUpdate(_ data: Data) {
        guard let level = data.first else {
            logger.error("Failed to parse Battery Level (no data)")
            return
        }

        batteryPercent = Int(level)
        logger.info("Battery level updated: \(level)%")
    }

    /// Parse and update Bottle Config from BLE read
    private func handleBottleConfigUpdate(_ data: Data) {
        guard let config = BLEBottleConfig.parse(from: data) else {
            logger.error("Failed to parse Bottle Config (expected 12 bytes, got \(data.count))")
            return
        }

        bottleCapacityMl = Int(config.bottleCapacityMl)
        dailyGoalMl = Int(config.dailyGoalMl)

        logger.info("Bottle Config: capacity=\(config.bottleCapacityMl)ml, goal=\(config.dailyGoalMl)ml, tare=\(config.tareWeightGrams)g, scale=\(config.scaleFactor)")
    }

    // MARK: - Sync Protocol (Phase 4.4)

    /// Handle Sync Control characteristic update
    private func handleSyncControlUpdate(_ data: Data) {
        guard let syncControl = BLESyncControl.parse(from: data) else {
            logger.error("Failed to parse Sync Control (expected 8 bytes, got \(data.count))")
            return
        }

        logger.info("Sync Control: status=\(syncControl.status), command=\(syncControl.command), count=\(syncControl.count)")

        // Update sync state based on firmware response
        switch BLESyncControl.Status(rawValue: syncControl.status) {
        case .idle:
            if syncState == .complete {
                // Sync finished successfully
            } else if syncState.isActive {
                // Sync was cancelled or reset
                syncState = .idle
            }
        case .inProgress:
            // Firmware is sending data
            syncState = .receiving
        case .complete:
            // Firmware reports sync complete
            completeSyncTransfer()
        case .none:
            logger.warning("Unknown sync status: \(syncControl.status)")
        }
    }

    /// Handle Drink Data chunk notification
    private func handleDrinkDataUpdate(_ data: Data) {
        // Debug: log raw data for troubleshooting
        let hexString = data.map { String(format: "%02X", $0) }.joined(separator: " ")
        logger.info("Drink Data raw (\(data.count) bytes): \(hexString)")

        // Minimum size check with better error message
        guard data.count >= BLEDrinkDataChunk.headerSize else {
            logger.warning("Drink Data too short (\(data.count) bytes, need \(BLEDrinkDataChunk.headerSize) for header) - ignoring")
            return  // Don't fail sync, firmware may send empty/status notifications
        }

        guard let chunk = BLEDrinkDataChunk.parse(from: data) else {
            logger.error("Failed to parse Drink Data chunk (got \(data.count) bytes)")
            // Log more detail about what we received
            if data.count >= 6 {
                let chunkIdx = data.prefix(2).withUnsafeBytes { $0.load(as: UInt16.self) }
                let totalChunks = data.dropFirst(2).prefix(2).withUnsafeBytes { $0.load(as: UInt16.self) }
                let recordCount = data[4]
                logger.error("Partial parse: chunkIdx=\(chunkIdx), totalChunks=\(totalChunks), recordCount=\(recordCount)")
                logger.error("Expected data size: \(6 + Int(recordCount) * 10) bytes, got \(data.count)")
            }
            handleSyncFailure("Invalid drink data chunk")
            return
        }

        logger.info("Received chunk \(chunk.chunkIndex + 1)/\(chunk.totalChunks) with \(chunk.recordCount) records")

        // Validate chunk sequence
        if chunk.chunkIndex == 0 {
            // First chunk - initialize sync
            syncBuffer.removeAll()
            expectedTotalChunks = chunk.totalChunks
            totalRecordsToSync = Int(chunk.totalChunks) * Int(BLEDrinkDataChunk.maxRecordsPerChunk)
        } else if chunk.chunkIndex != lastChunkIndex + 1 {
            // Out of order chunk
            logger.warning("Out of order chunk: expected \(self.lastChunkIndex + 1), got \(chunk.chunkIndex)")
        }

        lastChunkIndex = chunk.chunkIndex

        // Append records to buffer
        syncBuffer.append(contentsOf: chunk.records)
        syncedRecordCount = syncBuffer.count

        // Update progress
        if chunk.totalChunks > 0 {
            syncProgress = Double(chunk.chunkIndex + 1) / Double(chunk.totalChunks)
        }

        logger.debug("Sync progress: \(self.syncBuffer.count) records, \(Int(self.syncProgress * 100))%")

        // Send ACK for this chunk
        sendSyncAck()

        // Check if this was the last chunk
        if chunk.isLastChunk {
            logger.info("Last chunk received, completing sync")
            completeSyncTransfer()
        }
    }

    /// Start drink history sync
    func startDrinkSync() {
        guard connectionState.isConnected else {
            logger.warning("Cannot start sync: not connected")
            return
        }

        guard !syncState.isActive else {
            logger.warning("Sync already in progress")
            return
        }

        guard unsyncedCount > 0 else {
            logger.info("No records to sync")
            syncState = .complete
            return
        }

        logger.info("Starting drink sync for \(self.unsyncedCount) records")

        // Reset sync state
        syncState = .requesting
        syncProgress = 0.0
        syncedRecordCount = 0
        totalRecordsToSync = unsyncedCount
        syncBuffer.removeAll()
        lastChunkIndex = 0
        expectedTotalChunks = 0

        // Get or create bottle ID for CoreData
        currentBottleId = PersistenceController.shared.getOrCreateDefaultBottle().id

        // Send START command to firmware
        let startControl = BLESyncControl.startCommand(count: UInt16(unsyncedCount))
        writeSyncControl(startControl)
    }

    /// Send ACK for received chunk
    private func sendSyncAck() {
        let ackControl = BLESyncControl.ackCommand()
        writeSyncControl(ackControl)
    }

    /// Write to Sync Control characteristic
    private func writeSyncControl(_ control: BLESyncControl) {
        guard let peripheral = connectedPeripheral,
              let characteristic = characteristics[BLEConstants.syncControlUUID] else {
            logger.error("Cannot write sync control: peripheral or characteristic not available")
            return
        }

        let data = control.toData()
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
        logger.debug("Wrote sync control: command=\(control.command)")
    }

    /// Complete the sync transfer and save to CoreData
    private func completeSyncTransfer() {
        guard !syncBuffer.isEmpty else {
            logger.info("Sync complete with no records")
            syncState = .complete
            lastSyncTime = Date()
            return
        }

        logger.info("Completing sync with \(self.syncBuffer.count) records")

        // Save to CoreData
        if let bottleId = currentBottleId {
            PersistenceController.shared.saveDrinkRecords(syncBuffer, for: bottleId)
            logger.info("Saved \(self.syncBuffer.count) records to CoreData")
        } else {
            logger.error("No bottle ID for saving records")
        }

        // Update state
        syncState = .complete
        syncProgress = 1.0
        lastSyncTime = Date()

        // Clear buffer
        let recordCount = syncBuffer.count
        syncBuffer.removeAll()

        logger.info("Drink sync complete: \(recordCount) records synced")
    }

    /// Handle sync failure
    private func handleSyncFailure(_ reason: String) {
        logger.error("Sync failed: \(reason)")
        syncState = .failed
        syncBuffer.removeAll()
        errorMessage = "Sync failed: \(reason)"
    }

    /// Check if sync should start automatically (called after connection complete)
    func checkAutoSync() {
        // Auto-sync when there are unsynced records
        if unsyncedCount > 0 && !syncState.isActive {
            logger.info("Auto-starting sync for \(self.unsyncedCount) unsynced records")
            startDrinkSync()
        }
    }

    // MARK: - Commands (Phase 4.5)

    /// Send TARE command to reset empty bottle baseline
    func sendTareCommand() {
        writeCommand(BLECommand.tareNow())
        logger.info("Sent TARE_NOW command")
    }

    /// Send RESET_DAILY command to reset daily intake counter
    func sendResetDailyCommand() {
        writeCommand(BLECommand.resetDaily())
        logger.info("Sent RESET_DAILY command")
    }

    /// Send CLEAR_HISTORY command to clear all drink records
    func sendClearHistoryCommand() {
        writeCommand(BLECommand.clearHistory())
        logger.info("Sent CLEAR_HISTORY command")
    }

    /// Send START_CALIBRATION command to begin calibration flow
    func sendStartCalibrationCommand() {
        writeCommand(BLECommand.startCalibration())
        logger.info("Sent START_CALIBRATION command")
    }

    /// Send CANCEL_CALIBRATION command to abort calibration
    func sendCancelCalibrationCommand() {
        writeCommand(BLECommand.cancelCalibration())
        logger.info("Sent CANCEL_CALIBRATION command")
    }

    /// Write command to Command characteristic
    private func writeCommand(_ command: BLECommand) {
        guard let peripheral = connectedPeripheral,
              let characteristic = characteristics[BLEConstants.commandUUID] else {
            logger.error("Cannot write command: peripheral or characteristic not available")
            errorMessage = "Not connected to device"
            return
        }

        let data = command.toData()
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
        logger.debug("Wrote command: \(command.command)")
    }

    // MARK: - Time Sync (Phase 4.5)

    /// Sync device time to current iOS time
    func syncDeviceTime() {
        guard let peripheral = connectedPeripheral,
              let characteristic = characteristics[BLEConstants.commandUUID] else {
            logger.error("Cannot sync time: peripheral or characteristic not available")
            return
        }

        let currentTimestamp = UInt32(Date().timeIntervalSince1970)
        let data = BLECommand.setTime(timestamp: currentTimestamp)
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
        logger.info("Sent SET_TIME command with timestamp: \(currentTimestamp)")
    }

    /// Auto-sync time on connection if device time is invalid
    private func checkTimeSync() {
        // Always sync time on connection to handle drift and ensure accuracy
        if connectionState.isConnected {
            logger.info("Syncing device time on connection")
            syncDeviceTime()
        }
    }

    // MARK: - Bottle Config (Phase 4.5)

    /// Write new bottle configuration to device
    func writeBottleConfig(capacity: UInt16, goal: UInt16) {
        guard let peripheral = connectedPeripheral,
              let characteristic = characteristics[BLEConstants.bottleConfigUUID] else {
            logger.error("Cannot write bottle config: peripheral or characteristic not available")
            errorMessage = "Not connected to device"
            return
        }

        // Create config with current values (we only update capacity and goal)
        // Note: scaleFactor and tareWeight should be preserved from calibration
        let config = BLEBottleConfig(
            scaleFactor: 1.0,  // Will be preserved by firmware if not recalibrating
            tareWeightGrams: 0, // Will be preserved by firmware
            bottleCapacityMl: capacity,
            dailyGoalMl: goal
        )

        let data = config.toData()
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
        logger.info("Wrote bottle config: capacity=\(capacity)ml, goal=\(goal)ml")

        // Update local state
        bottleCapacityMl = Int(capacity)
        dailyGoalMl = Int(goal)
    }

    /// Read bottle configuration from device
    func readBottleConfig() {
        guard let peripheral = connectedPeripheral,
              let characteristic = characteristics[BLEConstants.bottleConfigUUID] else {
            logger.error("Cannot read bottle config: peripheral or characteristic not available")
            return
        }

        peripheral.readValue(for: characteristic)
        logger.info("Requested bottle config read")
    }
}
