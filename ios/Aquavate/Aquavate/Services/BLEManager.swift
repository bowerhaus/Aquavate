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

/// Result of pull-to-refresh operation
enum RefreshResult {
    case synced(recordCount: Int)   // Successfully synced N records
    case alreadySynced              // Connected but no records to sync
    case bottleAsleep               // Scan timeout - no devices found
    case bluetoothOff               // Bluetooth not available
    case connectionFailed(String)   // Connection error with message
    case syncFailed(String)         // Sync error with message
}

/// State for activity stats fetch operation (Issue #36)
enum ActivityFetchState: Equatable {
    case idle
    case fetchingSummary
    case fetchingMotionEvents(currentChunk: Int, totalChunks: Int)
    case fetchingBackpackSessions(currentChunk: Int, totalChunks: Int)
    case complete
    case failed(String)

    var isLoading: Bool {
        switch self {
        case .fetchingSummary, .fetchingMotionEvents, .fetchingBackpackSessions:
            return true
        default:
            return false
        }
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
    @Published private(set) var bottleLevelMl: Int = 0 {
        didSet {
            // Persist to UserDefaults for offline display
            UserDefaults.standard.set(bottleLevelMl, forKey: "lastKnownBottleLevelMl")
        }
    }

    /// Whether we've ever received bottle data (for showing "(recent)" indicator)
    @Published private(set) var hasReceivedBottleData: Bool = false {
        didSet {
            UserDefaults.standard.set(hasReceivedBottleData, forKey: "hasReceivedBottleData")
        }
    }

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

    /// Whether a calibration measurement is in progress (firmware taking stable reading)
    @Published private(set) var isCalMeasuring: Bool = false

    /// Whether calibration ADC result is ready to read
    @Published private(set) var isCalResultReady: Bool = false

    /// Raw ADC value from calibration measurement (only valid when isCalResultReady is true)
    @Published private(set) var calibrationRawADC: Int32?

    // MARK: - Published Properties (Bottle-Driven Calibration - Plan 060)

    /// Current calibration state from bottle (bottle-driven calibration)
    @Published private(set) var calibrationState: BLECalibrationState?

    /// Number of drink records pending sync
    @Published private(set) var unsyncedCount: Int = 0

    /// Timestamp of last Current State update
    @Published private(set) var lastStateUpdateTime: Date?

    // MARK: - Published Properties (Bottle Config)

    /// Bottle capacity in ml (from config)
    @Published private(set) var bottleCapacityMl: Int = 830

    /// Daily goal in ml (from config)
    @Published private(set) var dailyGoalMl: Int = 2000 {
        didSet {
            // Persist to UserDefaults for offline display
            UserDefaults.standard.set(dailyGoalMl, forKey: "lastKnownDailyGoalMl")
        }
    }

    // MARK: - Published Properties (Device Settings)

    /// Whether shake-to-empty gesture is enabled (default: true)
    @Published private(set) var isShakeToEmptyEnabled: Bool = true

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

    // MARK: - Published Properties (Activity Stats - Issue #36)

    /// Current activity fetch state
    @Published private(set) var activityFetchState: ActivityFetchState = .idle

    /// Activity summary from device
    @Published private(set) var activitySummary: BLEActivitySummary?

    /// Motion wake events from device
    @Published private(set) var motionWakeEvents: [BLEMotionWakeEvent] = []

    /// Backpack sessions from device
    @Published private(set) var backpackSessions: [BLEBackpackSession] = []

    // MARK: - Private Properties

    /// Whether HealthKit sync is currently in progress (prevents concurrent execution)
    private var isHealthKitSyncInProgress = false

    /// Whether a HealthKit sync was requested while one was already in progress
    private var pendingHealthKitSync = false

    /// Buffer for drink records during sync
    private var syncBuffer: [BLEDrinkRecord] = []

    /// Expected total chunks in current sync
    private var expectedTotalChunks: UInt16 = 0

    /// Last received chunk index
    private var lastChunkIndex: UInt16 = 0

    /// Bottle ID for current sync (from CoreData)
    private var currentBottleId: UUID?

    // MARK: - Activity Stats Fetch Tracking

    /// Expected motion event count from summary
    private var expectedMotionEventCount: Int = 0

    /// Expected backpack session count from summary
    private var expectedBackpackSessionCount: Int = 0

    /// Current motion event chunk being fetched
    private var currentMotionChunkIndex: UInt8 = 0

    /// Total motion event chunks to fetch
    private var totalMotionChunks: UInt8 = 0

    /// Current backpack session chunk being fetched
    private var currentBackpackChunkIndex: UInt8 = 0

    /// Total backpack session chunks to fetch
    private var totalBackpackChunks: UInt8 = 0

    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?
    private var scanTimer: Timer?
    private var connectionTimer: Timer?
    private var idleDisconnectTimer: Timer?
    private var goalWriteTask: Task<Void, Never>?

    /// How long to stay connected after sync when app is in foreground (seconds)
    private let idleDisconnectInterval: TimeInterval = 60.0

    /// Discovered characteristics keyed by UUID
    private var characteristics: [CBUUID: CBCharacteristic] = [:]

    /// Logger for BLE events
    private let logger = Logger(subsystem: "com.aquavate", category: "BLE")

    /// Pending delete completion handler (for bidirectional sync)
    private var pendingDeleteCompletion: ((Bool) -> Void)?
    private var pendingDeleteRecordId: UInt32?
    private var pendingDeleteTask: Task<Void, Never>?

    // MARK: - Auto-Reconnection Properties

    /// Duration of foreground scan burst (seconds)
    private let foregroundScanBurstDuration: TimeInterval = 5.0

    /// Delay before scanning after disconnect (seconds)
    private let scanAfterDisconnectDelay: TimeInterval = 3.0

    /// Whether auto-reconnection is enabled
    private var autoReconnectEnabled: Bool = true

    /// Timer for foreground scan burst
    private var foregroundScanTimer: Timer?

    /// Track if we're in a scan burst (to distinguish from manual scanning)
    private var isAutoReconnectScan: Bool = false

    /// Peripheral we're waiting for iOS to reconnect to in background
    /// When set, iOS will auto-connect when the device advertises
    private var pendingBackgroundReconnectPeripheral: CBPeripheral?

    /// Reference to HealthKitManager for syncing drinks (set by AquavateApp)
    weak var healthKitManager: HealthKitManager?

    /// Reference to HydrationReminderService for triggering reminders (set by AquavateApp)
    weak var hydrationReminderService: HydrationReminderService?

    // MARK: - Initialization

    override init() {
        super.init()

        // Load persisted last sync time
        lastSyncTime = UserDefaults.standard.object(forKey: "lastSyncTime") as? Date

        // Load persisted daily goal (default 2000 if never synced)
        if UserDefaults.standard.object(forKey: "lastKnownDailyGoalMl") != nil {
            dailyGoalMl = UserDefaults.standard.integer(forKey: "lastKnownDailyGoalMl")
        }

        // Load persisted bottle level and connection history flag
        if UserDefaults.standard.bool(forKey: "hasReceivedBottleData") {
            hasReceivedBottleData = true
            bottleLevelMl = UserDefaults.standard.integer(forKey: "lastKnownBottleLevelMl")
        }

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

        // Defensive state recovery: if connectionState is .scanning but CoreBluetooth isn't,
        // we have corrupted state - reset it
        if connectionState == .scanning && !centralManager.isScanning {
            logger.warning("Detected corrupted scanning state - recovering")
            connectionState = .disconnected
            scanTimer?.invalidate()
            scanTimer = nil
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
        // Always clean up our state, even if CoreBluetooth stopped scanning externally
        scanTimer?.invalidate()
        scanTimer = nil

        // Only call stopScan if actually scanning (avoids CoreBluetooth warning)
        if centralManager.isScanning {
            logger.info("Stopping BLE scan")
            centralManager.stopScan()
        } else if connectionState == .scanning {
            logger.info("Cleaning up scanning state (CoreBluetooth already stopped)")
        }

        // Always reset connectionState if we were scanning
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

    // MARK: - Auto-Reconnection Methods

    /// Perform a brief scan burst to find known device
    /// Called when app becomes active or after unexpected disconnect
    func performForegroundScanBurst() {
        guard isBluetoothReady else {
            logger.info("Skipping scan burst: Bluetooth not ready")
            return
        }

        guard connectionState == .disconnected else {
            logger.info("Skipping scan burst: already \(self.connectionState.rawValue)")
            return
        }

        guard autoReconnectEnabled else {
            logger.info("Skipping scan burst: auto-reconnect disabled")
            return
        }

        // Check if we have a known device to reconnect to
        guard let identifierString = UserDefaults.standard.string(forKey: BLEConstants.lastConnectedPeripheralKey),
              UUID(uuidString: identifierString) != nil else {
            logger.info("Skipping scan burst: no known device")
            return
        }

        logger.info("Starting foreground scan burst (\(Int(self.foregroundScanBurstDuration))s)")
        isAutoReconnectScan = true
        connectionState = .scanning
        discoveredDevices.removeAll()

        // Scan for Aquavate service UUID
        centralManager.scanForPeripherals(
            withServices: [BLEConstants.aquavateServiceUUID],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )

        // Set burst timeout (shorter than normal scan)
        foregroundScanTimer?.invalidate()
        foregroundScanTimer = Timer.scheduledTimer(withTimeInterval: foregroundScanBurstDuration, repeats: false) { [weak self] _ in
            Task { @MainActor in
                self?.handleScanBurstTimeout()
            }
        }
    }

    /// Handle scan burst timeout - silent cleanup
    private func handleScanBurstTimeout() {
        guard isAutoReconnectScan else { return }

        logger.info("Scan burst timeout - no device found")
        isAutoReconnectScan = false
        centralManager.stopScan()
        foregroundScanTimer?.invalidate()
        foregroundScanTimer = nil
        connectionState = .disconnected
        // Silent failure - no error message for auto-reconnect attempts
    }

    /// Called when app becomes active (foreground)
    /// Attempts brief scan burst to reconnect to known device
    func appDidBecomeActive() {
        // Only attempt if we're not already connected or connecting
        guard connectionState == .disconnected else {
            logger.info("appDidBecomeActive: already \(self.connectionState.rawValue)")
            return
        }

        // Small delay to let Bluetooth stabilize after app activation
        Task {
            try? await Task.sleep(nanoseconds: 500_000_000) // 0.5s
            await MainActor.run {
                self.performForegroundScanBurst()
            }
        }
    }

    /// Called when app goes to background
    /// Disconnect after a short delay to allow bottle to sleep
    func appDidEnterBackground() {
        logger.info("App entered background")

        // Cancel any pending auto-reconnect attempts
        foregroundScanTimer?.invalidate()
        foregroundScanTimer = nil
        isAutoReconnectScan = false

        // If we're scanning (not connected), stop scanning
        if connectionState == .scanning {
            centralManager.stopScan()
            connectionState = .disconnected
        }

        // If connected, disconnect after a short delay to allow any in-progress sync to complete
        // Request background reconnection so iOS will auto-connect when bottle wakes
        if connectionState.isConnected {
            // Cancel any existing idle timer and set a shorter one for background
            idleDisconnectTimer?.invalidate()
            idleDisconnectTimer = Timer.scheduledTimer(withTimeInterval: 5.0, repeats: false) { [weak self] _ in
                Task { @MainActor in
                    guard let self = self else { return }
                    if self.connectionState.isConnected {
                        self.logger.info("Background timeout - disconnecting with background reconnect request")
                        self.disconnect(requestBackgroundReconnect: true)
                    }
                }
            }
        } else if autoReconnectEnabled {
            // Plan 035: Already disconnected but have a known device - still request background reconnect
            // This handles the case where bottle timed out while app was in foreground
            if let identifierString = UserDefaults.standard.string(forKey: BLEConstants.lastConnectedPeripheralKey),
               let identifier = UUID(uuidString: identifierString) {
                let peripherals = centralManager.retrievePeripherals(withIdentifiers: [identifier])
                if let peripheral = peripherals.first {
                    logger.info("Already disconnected - requesting background reconnect for known device")
                    requestBackgroundReconnection(to: peripheral)
                }
            }
        }
    }

    /// Enable/disable auto-reconnection
    func setAutoReconnect(enabled: Bool) {
        autoReconnectEnabled = enabled
        logger.info("Auto-reconnect \(enabled ? "enabled" : "disabled")")
    }

    /// Disconnect from current device
    /// If requestBackgroundReconnect is true, iOS will auto-reconnect when device advertises
    func disconnect(requestBackgroundReconnect: Bool = false) {
        guard let peripheral = connectedPeripheral else { return }

        // Cancel idle timer if active
        idleDisconnectTimer?.invalidate()
        idleDisconnectTimer = nil

        // Store peripheral for background reconnection if requested
        if requestBackgroundReconnect && autoReconnectEnabled {
            pendingBackgroundReconnectPeripheral = peripheral
            logger.info("Disconnecting from peripheral (will request background reconnect)")
        } else {
            pendingBackgroundReconnectPeripheral = nil
            logger.info("Disconnecting from peripheral")
        }

        centralManager.cancelPeripheralConnection(peripheral)
    }

    /// Request iOS to auto-connect to a peripheral when it becomes available
    /// This works even when the app is in the background
    private func requestBackgroundReconnection(to peripheral: CBPeripheral) {
        guard autoReconnectEnabled else {
            logger.info("Skipping background reconnect request: auto-reconnect disabled")
            return
        }

        logger.info("Requesting background reconnection to \(peripheral.name ?? "Unknown")")
        pendingBackgroundReconnectPeripheral = peripheral

        // This tells iOS to connect when the peripheral advertises
        // iOS handles this even when the app is suspended
        centralManager.connect(peripheral, options: [
            CBConnectPeripheralOptionNotifyOnConnectionKey: true,
            CBConnectPeripheralOptionNotifyOnDisconnectionKey: true,
            CBConnectPeripheralOptionNotifyOnNotificationKey: true
        ])

        // Update state to show we're waiting
        connectionState = .disconnected
    }

    /// Cancel any pending background reconnection request
    func cancelBackgroundReconnection() {
        if let peripheral = pendingBackgroundReconnectPeripheral {
            logger.info("Cancelling background reconnection request")
            centralManager.cancelPeripheralConnection(peripheral)
            pendingBackgroundReconnectPeripheral = nil
        }
    }

    /// Check if a specific characteristic is available
    func hasCharacteristic(_ uuid: CBUUID) -> Bool {
        characteristics[uuid] != nil
    }

    /// Start or reset the idle disconnect timer
    /// After the interval, automatically disconnects to save bottle battery
    func startIdleDisconnectTimer() {
        // Cancel existing timer
        idleDisconnectTimer?.invalidate()

        // Start new timer
        idleDisconnectTimer = Timer.scheduledTimer(withTimeInterval: idleDisconnectInterval, repeats: false) { [weak self] _ in
            Task { @MainActor in
                guard let self = self else { return }
                if self.connectionState.isConnected {
                    self.logger.info("Idle timeout - disconnecting to save battery")
                    self.disconnect()
                }
            }
        }
        logger.info("Idle disconnect timer started (\(Int(self.idleDisconnectInterval))s)")
    }

    /// Cancel the idle disconnect timer (e.g., when user interacts)
    func cancelIdleDisconnectTimer() {
        idleDisconnectTimer?.invalidate()
        idleDisconnectTimer = nil
    }

    // MARK: - Private Methods

    private func handleScanTimeout() {
        // Stop CoreBluetooth scan to save power
        // Note: "bottle asleep" detection is done by elapsed time in attemptConnection()
        if connectionState == .scanning || centralManager.isScanning {
            logger.info("Scan timeout - stopping CoreBluetooth scan")
            stopScanning()
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
                // Clean up timers during reset to prevent state corruption
                scanTimer?.invalidate()
                scanTimer = nil
                if connectionState == .scanning {
                    connectionState = .disconnected
                }

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

            // Check if this is our known device
            let knownIdentifier = UserDefaults.standard.string(forKey: BLEConstants.lastConnectedPeripheralKey)
            let isKnownDevice = knownIdentifier == peripheral.identifier.uuidString

            // Auto-connect if:
            // 1. This is a scan burst and we found our known device, OR
            // 2. This is a normal scan and only one device found (existing behavior)
            if isAutoReconnectScan && isKnownDevice {
                logger.info("Found known device during scan burst - auto-connecting")
                foregroundScanTimer?.invalidate()
                foregroundScanTimer = nil
                isAutoReconnectScan = false
                connect(to: peripheral)
            } else if !isAutoReconnectScan && discoveredDevices.count == 1 {
                // Existing behavior for manual scanning
                connect(to: peripheral)
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        Task { @MainActor in
            // Check if this is a background reconnection
            let isBackgroundReconnect = pendingBackgroundReconnectPeripheral?.identifier == peripheral.identifier
            if isBackgroundReconnect {
                logger.info("Background reconnection successful: \(peripheral.name ?? "Unknown")")
                pendingBackgroundReconnectPeripheral = nil
            } else {
                logger.info("Connected to peripheral: \(peripheral.name ?? "Unknown")")
            }

            connectionTimer?.invalidate()
            connectionTimer = nil
            connectedPeripheral = peripheral
            connectedDeviceName = peripheral.name
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
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        Task { @MainActor in
            let wasConnected = connectionState.isConnected

            if let error = error {
                logger.warning("Disconnected with error: \(error.localizedDescription)")
                // Only show error for unexpected disconnects when we were fully connected
                if wasConnected {
                    errorMessage = "Disconnected: \(error.localizedDescription)"
                }
            } else {
                logger.info("Disconnected from peripheral")
            }

            connectionState = .disconnected
            connectedPeripheral = nil
            characteristics.removeAll()

            // Check if we should request background reconnection
            // This is set when disconnect(requestBackgroundReconnect: true) was called
            if let pendingPeripheral = pendingBackgroundReconnectPeripheral {
                // Clear the pending flag before requesting (requestBackgroundReconnection will set it again)
                pendingBackgroundReconnectPeripheral = nil
                logger.info("Requesting background reconnection after disconnect")
                requestBackgroundReconnection(to: pendingPeripheral)
                return // Don't try foreground reconnect
            }

            // If unexpected disconnect while app is active, try to reconnect
            // Delay slightly to allow bottle to restart advertising
            if wasConnected && error != nil && autoReconnectEnabled {
                logger.info("Scheduling reconnect attempt in \(self.scanAfterDisconnectDelay)s")
                Task {
                    try? await Task.sleep(nanoseconds: UInt64(scanAfterDisconnectDelay * 1_000_000_000))
                    await MainActor.run {
                        self.performForegroundScanBurst()
                    }
                }
            }
        }
    }

    // MARK: - State Restoration

    nonisolated func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        Task { @MainActor in
            logger.info("Restoring central manager state")

            if let peripherals = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral] {
                for peripheral in peripherals {
                    logger.info("Restored peripheral: \(peripheral.name ?? "Unknown"), state: \(peripheral.state.rawValue)")

                    if peripheral.state == .connected {
                        connectedPeripheral = peripheral
                        connectedDeviceName = peripheral.name
                        peripheral.delegate = self

                        // Check if we already have services discovered
                        if let services = peripheral.services, !services.isEmpty {
                            logger.info("Services already discovered, re-discovering characteristics")
                            connectionState = .discovering
                            for service in services {
                                if service.uuid == BLEConstants.aquavateServiceUUID {
                                    peripheral.discoverCharacteristics(BLEConstants.aquavateCharacteristics, for: service)
                                } else if service.uuid == BLEConstants.batteryServiceUUID {
                                    peripheral.discoverCharacteristics(BLEConstants.batteryCharacteristics, for: service)
                                }
                            }
                        } else {
                            // Need to discover services
                            connectionState = .discovering
                            discoverServices()
                        }
                    } else if peripheral.state == .connecting {
                        connectedPeripheral = peripheral
                        connectedDeviceName = peripheral.name
                        connectionState = .connecting
                    }
                }
            }

            // Restore scan state if we were scanning
            if let scanServices = dict[CBCentralManagerRestoredStateScanServicesKey] as? [CBUUID] {
                logger.info("Restoring scan for services: \(scanServices)")
                connectionState = .scanning
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
                       characteristic.uuid == BLEConstants.batteryLevelUUID ||
                       characteristic.uuid == BLEConstants.activityStatsUUID ||
                       characteristic.uuid == BLEConstants.calibrationStateUUID {
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

            case BLEConstants.deviceSettingsUUID:
                handleDeviceSettingsUpdate(data)

            case BLEConstants.activityStatsUUID:
                handleActivityStatsUpdate(data)

            case BLEConstants.calibrationStateUUID:
                handleCalibrationStateUpdate(data)

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
            BLEConstants.commandUUID,
            BLEConstants.deviceSettingsUUID
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

            // Start idle disconnect timer to ensure bottle can sleep
            // This will be reset by performRefresh() if user does pull-to-refresh
            startIdleDisconnectTimer()

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

        // Update published properties
        currentWeightG = Int(state.currentWeightG)
        hasReceivedBottleData = true
        dailyTotalMl = Int(state.dailyTotalMl)
        batteryPercent = Int(state.batteryPercent)
        isTimeValid = state.isTimeValid
        isCalibrated = state.isCalibrated
        isStable = state.isStable
        isCalMeasuring = state.isCalMeasuring
        isCalResultReady = state.isCalResultReady
        unsyncedCount = Int(state.unsyncedCount)
        lastStateUpdateTime = Date()

        // Handle calibration result - when cal_result_ready is set, extract raw ADC from encoded fields
        if state.isCalResultReady {
            calibrationRawADC = state.calibrationRawADC
            // During calibration, currentWeightG and bottleLevelMl contain encoded ADC, not actual values
            logger.info("Calibration result ready: rawADC=\(String(describing: self.calibrationRawADC))")
        } else {
            calibrationRawADC = nil
            // Normal operation - use bottle level as-is
            bottleLevelMl = Int(state.bottleLevelMl)
        }

        logger.info("Current State: weight=\(state.currentWeightG)g, level=\(state.bottleLevelMl)ml, daily=\(state.dailyTotalMl)ml, battery=\(state.batteryPercent)%, unsynced=\(state.unsyncedCount)")
        logger.debug("  Flags: timeValid=\(state.isTimeValid), calibrated=\(state.isCalibrated), stable=\(state.isStable), calMeasuring=\(state.isCalMeasuring), calResultReady=\(state.isCalResultReady)")

        // Check if we have a pending delete confirmation
        // The bottle sends a Current State notification after processing DELETE_DRINK_RECORD
        if let completion = pendingDeleteCompletion {
            logger.info("Delete confirmed for record \(self.pendingDeleteRecordId ?? 0)")
            pendingDeleteCompletion = nil
            pendingDeleteRecordId = nil
            completion(true)
        }

        // Auto-sync if we have unsynced records and sync is not already in progress
        // Trigger when: we have unsynced records, sync is idle, and we're connected
        // This handles both initial connection and new drinks poured while connected
        if unsyncedCount > 0 && syncState == .idle && connectionState.isConnected {
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

    /// Parse and update Device Settings from BLE read
    private func handleDeviceSettingsUpdate(_ data: Data) {
        guard let settings = BLEDeviceSettings.parse(from: data) else {
            logger.error("Failed to parse Device Settings (expected 4 bytes, got \(data.count))")
            return
        }

        isShakeToEmptyEnabled = settings.isShakeToEmptyEnabled

        logger.info("Device Settings: shakeToEmpty=\(settings.isShakeToEmptyEnabled)")
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
        logger.debug("Drink Data raw (\(data.count) bytes): \(hexString)")

        // Minimum size check with better error message
        guard data.count >= BLEDrinkDataChunk.headerSize else {
            // This can happen if characteristic has no initial value set (0 bytes on subscribe)
            logger.debug("Drink Data too short (\(data.count) bytes) - ignoring initial empty notification")
            return
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

        // Ignore empty initial value (firmware sets this on init to prevent 0-byte notification)
        if chunk.totalChunks == 0 && chunk.recordCount == 0 {
            logger.debug("Ignoring empty initial drink data chunk")
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
            syncState = .idle
            lastSyncTime = Date()
            return
        }

        logger.info("Completing sync with \(self.syncBuffer.count) records")

        // Save to CoreData
        if let bottleId = currentBottleId {
            PersistenceController.shared.saveDrinkRecords(syncBuffer, for: bottleId)
            logger.info("Saved \(self.syncBuffer.count) records to CoreData")

            // Sync to HealthKit if enabled
            if let healthKitManager = healthKitManager, healthKitManager.isEnabled && healthKitManager.isAuthorized {
                Task {
                    await syncDrinksToHealthKit()
                }
            }
        } else {
            logger.error("No bottle ID for saving records")
        }

        // Clear buffer
        let recordCount = syncBuffer.count
        syncBuffer.removeAll()

        // Update state - go back to idle so we can sync again if more drinks arrive
        syncState = .idle
        syncProgress = 1.0
        lastSyncTime = Date()

        // Capture previous urgency before updating state for back-on-track detection
        let previousUrgency = hydrationReminderService?.currentUrgency

        // Update hydration state (this recalculates urgency)
        hydrationReminderService?.updateState(
            totalMl: dailyTotalMl,
            goalMl: dailyGoalMl,
            lastDrink: Date()  // Use current time as last drink since we just synced
        )

        // Check for back-on-track transition
        hydrationReminderService?.checkBackOnTrack(previousUrgency: previousUrgency)

        // Check if goal achieved
        if dailyTotalMl >= dailyGoalMl {
            hydrationReminderService?.goalAchieved()
        }

        logger.info("Drink sync complete: \(recordCount) records synced")
    }

    /// Sync any unsynced drinks to HealthKit
    /// Uses a lock to prevent concurrent execution (Issue #37)
    private func syncDrinksToHealthKit() async {
        // Prevent concurrent execution - if already syncing, queue a re-sync
        guard !isHealthKitSyncInProgress else {
            logger.info("[HealthKit] Sync already in progress, queuing re-sync")
            pendingHealthKitSync = true
            return
        }

        guard let healthKitManager = healthKitManager,
              healthKitManager.isEnabled && healthKitManager.isAuthorized else {
            return
        }

        isHealthKitSyncInProgress = true
        defer {
            isHealthKitSyncInProgress = false
            // Check if another sync was requested while we were syncing
            if pendingHealthKitSync {
                pendingHealthKitSync = false
                Task {
                    await self.syncDrinksToHealthKit()
                }
            }
        }

        let unsyncedDrinks = PersistenceController.shared.getUnsyncedDrinkRecords()
        guard !unsyncedDrinks.isEmpty else {
            logger.info("[HealthKit] No unsynced drinks to sync")
            return
        }

        logger.info("[HealthKit] Syncing \(unsyncedDrinks.count) drinks to HealthKit")

        for drink in unsyncedDrinks {
            guard let drinkId = drink.id,
                  let timestamp = drink.timestamp else {
                continue
            }

            // Defense in depth: check if sample already exists (handles crash recovery scenarios)
            if await healthKitManager.waterSampleExists(for: timestamp) {
                PersistenceController.shared.markDrinkSyncedToHealth(id: drinkId, healthKitUUID: UUID())
                continue
            }

            do {
                let hkUUID = try await healthKitManager.logWater(
                    milliliters: Int(drink.amountMl),
                    date: timestamp
                )
                PersistenceController.shared.markDrinkSyncedToHealth(id: drinkId, healthKitUUID: hkUUID)
            } catch {
                logger.error("[HealthKit] Failed to sync drink \(drinkId): \(error.localizedDescription)")
                // Continue with next drink - failed ones will retry on next sync
            }
        }
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

    /// Send PING command to reset activity timeout (keep-alive)
    func sendPingCommand() {
        writeCommand(BLECommand.ping())
        logger.debug("Sent PING command")
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

    // MARK: - Calibration Commands (iOS-Driven Calibration - Plan 060)

    /// Send CAL_MEASURE_POINT command to request stable ADC measurement
    /// The firmware will take a ~10 second stable measurement and return the raw ADC
    /// via a Current State notification with the cal_result_ready flag set
    func sendCalMeasurePointCommand(pointType: BLECommand.CalibrationPointType) {
        writeCommand(BLECommand.calMeasurePoint(pointType: pointType))
        logger.info("Sent CAL_MEASURE_POINT command (point=\(pointType == .empty ? "empty" : "full"))")
    }

    /// Send CAL_SET_DATA command to save calibration to firmware NVS
    /// After both empty and full measurements are taken, iOS calculates the scale factor
    /// and sends the calibration data to the firmware for storage
    func sendCalSetDataCommand(emptyADC: Int32, fullADC: Int32, scaleFactor: Float) {
        guard let peripheral = connectedPeripheral,
              let characteristic = characteristics[BLEConstants.commandUUID] else {
            logger.error("Cannot send calibration data: not connected")
            errorMessage = "Not connected to device"
            return
        }

        let data = BLECommand.calSetData(emptyADC: emptyADC, fullADC: fullADC, scaleFactor: scaleFactor)
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
        logger.info("Sent CAL_SET_DATA command (emptyADC=\(emptyADC), fullADC=\(fullADC), scale=\(scaleFactor))")
    }

    // MARK: - Bidirectional Sync (Delete Drink Record)

    /// Delete a drink record on the bottle (bidirectional sync)
    /// Uses pessimistic delete - waits for bottle confirmation before returning success
    /// The completion handler is called with true if bottle confirmed the delete, false otherwise
    func deleteDrinkRecord(firmwareRecordId: UInt32, completion: @escaping (Bool) -> Void) {
        guard let peripheral = connectedPeripheral,
              let characteristic = characteristics[BLEConstants.commandUUID] else {
            logger.warning("Cannot delete drink record: not connected")
            completion(false)
            return
        }

        // Cancel any previous delete timeout task
        pendingDeleteTask?.cancel()

        // Store completion handler for when Current State notification arrives
        pendingDeleteCompletion = completion
        pendingDeleteRecordId = firmwareRecordId

        let data = BLECommand.deleteDrinkRecord(recordId: firmwareRecordId)
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
        logger.info("Sent DELETE_DRINK_RECORD command for id=\(firmwareRecordId)")

        // Set a timeout in case we don't get a response
        pendingDeleteTask = Task { @MainActor in
            try? await Task.sleep(nanoseconds: 5_000_000_000) // 5 second timeout
            guard !Task.isCancelled else { return }
            if let completion = self.pendingDeleteCompletion, self.pendingDeleteRecordId == firmwareRecordId {
                self.logger.warning("Delete confirmation timeout for record \(firmwareRecordId)")
                self.pendingDeleteCompletion = nil
                self.pendingDeleteRecordId = nil
                completion(false)
            }
        }
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

    // MARK: - Daily Goal Setting

    /// Set daily hydration goal with debounce (waits 0.5s after last change before writing)
    /// - Parameter ml: Goal in milliliters (clamped to 1000-4000ml)
    func setDailyGoal(_ ml: Int) {
        // Clamp to valid range
        let clampedMl = max(1000, min(4000, ml))

        // Update local state immediately (optimistic UI)
        dailyGoalMl = clampedMl

        // Update hydration reminder service with new goal (recalculates behind-target status)
        hydrationReminderService?.updateState(
            totalMl: dailyTotalMl,
            goalMl: clampedMl,
            lastDrink: nil  // Don't update last drink time
        )

        // Cancel any pending write
        goalWriteTask?.cancel()

        // Schedule debounced write
        goalWriteTask = Task { @MainActor in
            try? await Task.sleep(nanoseconds: 500_000_000)  // 0.5s debounce
            guard !Task.isCancelled else { return }

            // Write to bottle if connected
            if connectionState.isConnected {
                writeBottleConfig(capacity: UInt16(bottleCapacityMl), goal: UInt16(clampedMl))
                logger.info("Set daily goal: \(clampedMl)ml")
            }
        }
    }

    // MARK: - Device Settings

    /// Set shake-to-empty gesture enabled/disabled
    func setShakeToEmptyEnabled(_ enabled: Bool) {
        guard let peripheral = connectedPeripheral,
              let characteristic = characteristics[BLEConstants.deviceSettingsUUID] else {
            logger.error("Cannot write device settings: peripheral or characteristic not available")
            errorMessage = "Not connected to device"
            return
        }

        let settings = BLEDeviceSettings.create(shakeToEmptyEnabled: enabled)
        let data = settings.toData()
        peripheral.writeValue(data, for: characteristic, type: .withResponse)
        logger.info("Wrote device settings: shakeToEmpty=\(enabled)")

        // Update local state
        isShakeToEmptyEnabled = enabled
    }

    // MARK: - Activity Stats (On-Demand Fetch)

    /// Fetch activity stats on-demand (called when user opens Activity Stats view)
    /// This is a lazy-loaded feature - data is only fetched when explicitly requested
    func fetchActivityStats() {
        guard connectionState.isConnected else {
            logger.warning("Cannot fetch activity stats: not connected")
            activityFetchState = .failed("Not connected")
            return
        }

        logger.info("Starting activity stats fetch")

        // Reset state for new fetch
        activityFetchState = .fetchingSummary
        activitySummary = nil
        motionWakeEvents = []
        backpackSessions = []
        expectedMotionEventCount = 0
        expectedBackpackSessionCount = 0
        currentMotionChunkIndex = 0
        totalMotionChunks = 0
        currentBackpackChunkIndex = 0
        totalBackpackChunks = 0

        // Send command to get activity summary
        writeCommand(BLECommand.getActivitySummary())
        logger.info("Sent GET_ACTIVITY_SUMMARY command")
    }

    /// Handle activity stats characteristic update
    private func handleActivityStatsUpdate(_ data: Data) {
        let hexString = data.map { String(format: "%02X", $0) }.joined(separator: " ")
        logger.debug("Activity Stats raw (\(data.count) bytes): \(hexString)")

        switch activityFetchState {
        case .fetchingSummary:
            handleActivitySummaryResponse(data)

        case .fetchingMotionEvents:
            handleMotionEventChunkResponse(data)

        case .fetchingBackpackSessions:
            handleBackpackSessionChunkResponse(data)

        default:
            logger.debug("Received activity stats data in unexpected state: \(String(describing: self.activityFetchState))")
        }
    }

    /// Handle activity summary response
    private func handleActivitySummaryResponse(_ data: Data) {
        guard let summary = BLEActivitySummary.parse(from: data) else {
            logger.error("Failed to parse activity summary (expected 12 bytes, got \(data.count))")
            activityFetchState = .failed("Invalid summary data")
            return
        }

        logger.info("Activity Summary: motionEvents=\(summary.motionEventCount), backpackSessions=\(summary.backpackSessionCount), inBackpack=\(summary.isInBackpackMode)")

        // Store summary
        activitySummary = summary
        expectedMotionEventCount = Int(summary.motionEventCount)
        expectedBackpackSessionCount = Int(summary.backpackSessionCount)

        // Calculate expected chunks
        totalMotionChunks = UInt8((expectedMotionEventCount + BLEMotionEventChunk.maxEventsPerChunk - 1) / BLEMotionEventChunk.maxEventsPerChunk)
        totalBackpackChunks = UInt8((expectedBackpackSessionCount + BLEBackpackSessionChunk.maxSessionsPerChunk - 1) / BLEBackpackSessionChunk.maxSessionsPerChunk)

        // Request motion events if any
        if expectedMotionEventCount > 0 {
            activityFetchState = .fetchingMotionEvents(currentChunk: 0, totalChunks: Int(totalMotionChunks))
            currentMotionChunkIndex = 0
            writeCommand(BLECommand.getMotionChunk(index: 0))
            logger.info("Requesting motion event chunk 0/\(self.totalMotionChunks)")
        } else if expectedBackpackSessionCount > 0 {
            // No motion events, skip to backpack sessions
            activityFetchState = .fetchingBackpackSessions(currentChunk: 0, totalChunks: Int(totalBackpackChunks))
            currentBackpackChunkIndex = 0
            writeCommand(BLECommand.getBackpackChunk(index: 0))
            logger.info("Requesting backpack session chunk 0/\(self.totalBackpackChunks)")
        } else {
            // No data to fetch
            activityFetchState = .complete
            saveActivityStatsToCoreData()
            logger.info("Activity stats fetch complete (no events)")
        }
    }

    /// Handle motion event chunk response
    private func handleMotionEventChunkResponse(_ data: Data) {
        guard let chunk = BLEMotionEventChunk.parse(from: data) else {
            logger.error("Failed to parse motion event chunk")
            activityFetchState = .failed("Invalid motion event data")
            return
        }

        logger.info("Received motion event chunk \(chunk.chunkIndex + 1)/\(chunk.totalChunks) with \(chunk.eventCount) events")

        // Append events
        motionWakeEvents.append(contentsOf: chunk.events)
        currentMotionChunkIndex = chunk.chunkIndex

        // Check if we have more chunks to fetch
        if !chunk.isLastChunk {
            let nextChunk = chunk.chunkIndex + 1
            activityFetchState = .fetchingMotionEvents(currentChunk: Int(nextChunk), totalChunks: Int(chunk.totalChunks))
            writeCommand(BLECommand.getMotionChunk(index: nextChunk))
            logger.info("Requesting motion event chunk \(nextChunk)/\(chunk.totalChunks)")
        } else if expectedBackpackSessionCount > 0 {
            // Done with motion events, fetch backpack sessions
            activityFetchState = .fetchingBackpackSessions(currentChunk: 0, totalChunks: Int(totalBackpackChunks))
            currentBackpackChunkIndex = 0
            writeCommand(BLECommand.getBackpackChunk(index: 0))
            logger.info("Requesting backpack session chunk 0/\(self.totalBackpackChunks)")
        } else {
            // All done
            activityFetchState = .complete
            saveActivityStatsToCoreData()
            logger.info("Activity stats fetch complete (\(self.motionWakeEvents.count) motion events)")
        }
    }

    /// Handle backpack session chunk response
    private func handleBackpackSessionChunkResponse(_ data: Data) {
        guard let chunk = BLEBackpackSessionChunk.parse(from: data) else {
            logger.error("Failed to parse backpack session chunk")
            activityFetchState = .failed("Invalid backpack session data")
            return
        }

        logger.info("Received backpack session chunk \(chunk.chunkIndex + 1)/\(chunk.totalChunks) with \(chunk.sessionCount) sessions")

        // Append sessions
        backpackSessions.append(contentsOf: chunk.sessions)
        currentBackpackChunkIndex = chunk.chunkIndex

        // Check if we have more chunks to fetch
        if !chunk.isLastChunk {
            let nextChunk = chunk.chunkIndex + 1
            activityFetchState = .fetchingBackpackSessions(currentChunk: Int(nextChunk), totalChunks: Int(chunk.totalChunks))
            writeCommand(BLECommand.getBackpackChunk(index: nextChunk))
            logger.info("Requesting backpack session chunk \(nextChunk)/\(chunk.totalChunks)")
        } else {
            // All done
            activityFetchState = .complete
            saveActivityStatsToCoreData()
            logger.info("Activity stats fetch complete (\(self.motionWakeEvents.count) motion events, \(self.backpackSessions.count) backpack sessions)")
        }
    }

    /// Save fetched activity stats to CoreData for offline viewing
    private func saveActivityStatsToCoreData() {
        guard let bottleId = currentBottleId else {
            logger.warning("Cannot save activity stats: no bottle ID")
            return
        }

        // Clear existing activity stats before saving new ones (fresh sync)
        PersistenceController.shared.clearActivityStats(for: bottleId)

        // Save motion wake events
        if !motionWakeEvents.isEmpty {
            PersistenceController.shared.saveMotionWakeEvents(motionWakeEvents, for: bottleId)
        }

        // Save backpack sessions
        if !backpackSessions.isEmpty {
            PersistenceController.shared.saveBackpackSessions(backpackSessions, for: bottleId)
        }

        // Update last activity sync date
        PersistenceController.shared.updateLastActivitySyncDate(for: bottleId)

        logger.info("Activity stats saved to CoreData for offline viewing")
    }

    // MARK: - Bottle-Driven Calibration (Plan 060)

    /// Handle calibration state characteristic update from bottle
    private func handleCalibrationStateUpdate(_ data: Data) {
        let hexString = data.map { String(format: "%02X", $0) }.joined(separator: " ")
        logger.debug("Calibration State raw (\(data.count) bytes): \(hexString)")

        guard let state = BLECalibrationState.parse(from: data) else {
            logger.error("Failed to parse calibration state data")
            return
        }

        calibrationState = state
        logger.info("Calibration state: \(state.stateDescription), empty=\(state.emptyADC), full=\(state.fullADC)")
    }

    /// Start bottle-driven calibration (sends START_CALIBRATION command)
    func startCalibration() {
        guard connectionState.isConnected else {
            logger.warning("Cannot start calibration: not connected")
            return
        }

        logger.info("Sending START_CALIBRATION command")
        writeCommand(BLECommand.startCalibration())
    }

    /// Cancel ongoing bottle-driven calibration (sends CANCEL_CALIBRATION command)
    /// Best effort - tries to send even during disconnection to ensure bottle returns to main screen
    func cancelCalibration() {
        // Don't strictly require connected state - try to send anyway
        // This ensures cancel is sent even when navigating away triggers disconnection
        if !connectionState.isConnected {
            logger.warning("Attempting cancel calibration while not fully connected")
        }

        logger.info("Sending CANCEL_CALIBRATION command")
        writeCommand(BLECommand.cancelCalibration())
    }

    // MARK: - Pull-to-Refresh (Async API)

    /// Perform a complete refresh: connect (if needed) and sync
    /// Stays connected for 1 minute after sync (idle timeout handles disconnect)
    /// This is the main entry point for pull-to-refresh
    func performRefresh() async -> RefreshResult {
        logger.info("performRefresh: starting")

        // Check Bluetooth availability
        guard isBluetoothReady else {
            logger.warning("performRefresh: Bluetooth not ready")
            return .bluetoothOff
        }

        // Connect if not already connected
        if !connectionState.isConnected {
            logger.info("performRefresh: not connected, attempting connection")
            let connectResult = await attemptConnection()
            switch connectResult {
            case .success:
                logger.info("performRefresh: connection successful")
            case .failure(let result):
                logger.warning("performRefresh: connection failed")
                return result
            }
        } else {
            logger.info("performRefresh: already connected")
        }

        // Send ping to reset activity timeout before sync
        sendPingCommand()

        // Perform sync
        let syncResult = await performSyncOnly()
        logger.info("performRefresh: sync result = \(String(describing: syncResult))")

        // Start/reset idle disconnect timer (1 minute)
        // Connection will stay open for viewing real-time updates
        startIdleDisconnectTimer()

        return syncResult
    }

    /// Result of connection attempt (internal)
    private enum ConnectionResult {
        case success
        case failure(RefreshResult)
    }

    /// Attempt to connect to the bottle, returning success or failure result
    /// Uses elapsed time to detect conditions - no dependency on Timer callbacks
    private func attemptConnection() async -> ConnectionResult {
        // Already connected? Return success immediately
        if connectionState.isConnected {
            return .success
        }

        // Start scanning (Timer will stop CoreBluetooth scan after scanTimeout for power saving)
        startScanning()

        let scanTimeout = BLEConstants.scanTimeout  // 10 seconds
        let overallTimeout: TimeInterval = 15.0     // 15 seconds max
        let pollInterval: UInt64 = 100_000_000      // 100ms
        let startTime = Date()

        while true {
            let elapsed = Date().timeIntervalSince(startTime)

            // 1. Success: connected
            if connectionState.isConnected {
                return .success
            }

            // 2. Bottle asleep: past scan timeout with no devices discovered
            if elapsed > scanTimeout && discoveredDevices.isEmpty && !connectionState.isConnected {
                stopScanning()
                return .failure(.bottleAsleep)
            }

            // 3. Connection error: device was found but connection failed
            if connectionState == .disconnected && !discoveredDevices.isEmpty {
                if let error = errorMessage {
                    return .failure(.connectionFailed(error))
                }
            }

            // 4. Overall timeout: something unexpected happened
            if elapsed >= overallTimeout {
                stopScanning()
                return .failure(.connectionFailed("Connection timeout"))
            }

            try? await Task.sleep(nanoseconds: pollInterval)
        }
    }

    /// Perform sync when already connected, waiting for completion
    /// Uses polling for reliability
    private func performSyncOnly() async -> RefreshResult {
        logger.info("performSyncOnly: starting, unsyncedCount=\(self.unsyncedCount), lastStateUpdateTime=\(String(describing: self.lastStateUpdateTime))")

        // Wait for Current State update if we haven't received one yet
        // This ensures unsyncedCount is accurate before deciding whether to sync
        let stateTimeout: TimeInterval = 5.0
        let startTime = Date()
        while lastStateUpdateTime == nil && Date().timeIntervalSince(startTime) < stateTimeout {
            logger.info("performSyncOnly: waiting for Current State update...")
            try? await Task.sleep(nanoseconds: 100_000_000) // 100ms
        }

        logger.info("performSyncOnly: after wait, unsyncedCount=\(self.unsyncedCount), connected=\(self.connectionState.isConnected)")

        // Check we're still connected
        guard connectionState.isConnected else {
            logger.warning("performSyncOnly: disconnected while waiting for state")
            return .connectionFailed("Disconnected unexpectedly")
        }

        // Check if there are records to sync
        guard unsyncedCount > 0 else {
            logger.info("performSyncOnly: no records to sync")
            return .alreadySynced
        }

        // If sync is already in progress, just wait for it
        if !syncState.isActive {
            // Start the sync
            logger.info("performSyncOnly: starting sync for \(self.unsyncedCount) records")
            startDrinkSync()
        } else {
            logger.info("performSyncOnly: sync already in progress, waiting")
        }

        // Wait for sync to complete using polling
        let syncTimeout: TimeInterval = 30.0
        let syncStartTime = Date()
        while Date().timeIntervalSince(syncStartTime) < syncTimeout {
            // Check if sync completed
            if syncState == .idle && syncedRecordCount > 0 {
                logger.info("performSyncOnly: sync completed with \(self.syncedRecordCount) records")
                return .synced(recordCount: syncedRecordCount)
            }

            // Check if sync failed
            if syncState == .failed {
                let message = errorMessage ?? "Unknown error"
                logger.error("performSyncOnly: sync failed - \(message)")
                return .syncFailed(message)
            }

            // Check if disconnected
            if !connectionState.isConnected {
                logger.warning("performSyncOnly: disconnected during sync")
                return .syncFailed("Disconnected during sync")
            }

            // Check if sync completed with no records (already synced case)
            if syncState == .idle && unsyncedCount == 0 {
                logger.info("performSyncOnly: sync completed, no more unsynced records")
                return syncedRecordCount > 0 ? .synced(recordCount: syncedRecordCount) : .alreadySynced
            }

            try? await Task.sleep(nanoseconds: 100_000_000) // 100ms
        }

        logger.error("performSyncOnly: sync timeout")
        return .syncFailed("Sync timeout")
    }
}
