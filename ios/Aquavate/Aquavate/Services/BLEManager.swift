//
//  BLEManager.swift
//  Aquavate
//
//  CoreBluetooth central manager for communicating with Aquavate firmware.
//  Phase 4.1: Scanning, connection, and service/characteristic discovery.
//

import CoreBluetooth
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

/// BLE Manager for Aquavate device communication
@MainActor
class BLEManager: NSObject, ObservableObject {

    // MARK: - Published Properties

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

    // MARK: - Private Properties

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

            // Phase 4.2 will add parsing for each characteristic type
            // For now, just log that we received data
            switch characteristic.uuid {
            case BLEConstants.currentStateUUID:
                logger.info("Received Current State update (\(data.count) bytes)")

            case BLEConstants.batteryLevelUUID:
                if let level = data.first {
                    logger.info("Battery level: \(level)%")
                }

            case BLEConstants.drinkDataUUID:
                logger.info("Received Drink Data chunk (\(data.count) bytes)")

            case BLEConstants.bottleConfigUUID:
                logger.info("Received Bottle Config (\(data.count) bytes)")

            case BLEConstants.syncControlUUID:
                logger.info("Received Sync Control (\(data.count) bytes)")

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
        }
    }
}
