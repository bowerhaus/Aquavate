//
//  CalibrationManager.swift
//  Aquavate
//
//  State machine for iOS-driven two-point calibration flow.
//  Coordinates BLE communication with firmware to capture empty/full ADC readings.
//

import Foundation
import Combine
import os.log

// MARK: - Calibration State

/// State machine states for calibration flow
enum CalibrationState: Equatable {
    case notStarted
    case emptyBottlePrompt          // Step 1: Waiting for user to place empty bottle
    case measuringEmpty             // Step 1: Firmware taking stable measurement
    case emptyMeasured(adc: Int32)  // Step 1: Empty measurement complete
    case fullBottlePrompt           // Step 2: Waiting for user to fill bottle
    case measuringFull              // Step 2: Firmware taking stable measurement
    case fullMeasured(adc: Int32)   // Step 2: Full measurement complete
    case savingCalibration          // Step 3: Saving to firmware NVS
    case complete(scaleFactor: Float)
    case failed(CalibrationError)

    /// Current step number (1-3) for progress indicator
    var stepNumber: Int {
        switch self {
        case .notStarted:
            return 0
        case .emptyBottlePrompt, .measuringEmpty, .emptyMeasured:
            return 1
        case .fullBottlePrompt, .measuringFull, .fullMeasured:
            return 2
        case .savingCalibration, .complete, .failed:
            return 3
        }
    }

    /// Whether user can proceed (for button enablement)
    var canProceed: Bool {
        switch self {
        case .emptyBottlePrompt, .fullBottlePrompt:
            return true
        case .emptyMeasured, .fullMeasured:
            return true
        default:
            return false
        }
    }

    /// Whether a measurement is currently in progress
    var isMeasuring: Bool {
        switch self {
        case .measuringEmpty, .measuringFull:
            return true
        default:
            return false
        }
    }
}

// MARK: - Calibration Error

/// Errors that can occur during calibration
enum CalibrationError: Error, Equatable, LocalizedError {
    case notConnected
    case measurementTimeout
    case measurementUnstable
    case invalidMeasurement(String)
    case saveFailed
    case disconnected
    case cancelled

    var errorDescription: String? {
        switch self {
        case .notConnected:
            return "Device not connected"
        case .measurementTimeout:
            return "Measurement timed out. Please ensure the bottle is placed firmly on the sensor."
        case .measurementUnstable:
            return "Unable to get a stable reading. Please keep the bottle still."
        case .invalidMeasurement(let reason):
            return reason
        case .saveFailed:
            return "Failed to save calibration to device."
        case .disconnected:
            return "Device disconnected during calibration."
        case .cancelled:
            return "Calibration was cancelled."
        }
    }
}

// MARK: - Calibration Manager

/// Manages the iOS-driven calibration flow
/// Observes BLEManager state and coordinates the two-point calibration process
@MainActor
class CalibrationManager: ObservableObject {

    // MARK: - Published Properties

    /// Current calibration state
    @Published private(set) var state: CalibrationState = .notStarted

    /// Whether the weight reading is stable (from BLE)
    @Published private(set) var isStable: Bool = false

    /// Current weight in grams (from BLE)
    @Published private(set) var currentWeightG: Int = 0

    /// Measurement progress (0.0 to 1.0) - simulated during firmware measurement
    @Published private(set) var measurementProgress: Double = 0.0

    // MARK: - Private Properties

    /// Reference to BLE manager for communication
    private weak var bleManager: BLEManager?

    /// Subscriptions for BLE state changes
    private var cancellables = Set<AnyCancellable>()

    /// Empty bottle ADC reading
    private var emptyADC: Int32?

    /// Full bottle ADC reading
    private var fullADC: Int32?

    /// Timer for measurement progress animation
    private var progressTimer: Timer?

    /// Task for measurement timeout
    private var timeoutTask: Task<Void, Never>?

    /// Logger
    private let logger = Logger(subsystem: "com.aquavate", category: "Calibration")

    /// Measurement timeout duration (seconds)
    private let measurementTimeout: TimeInterval = 30.0

    /// Expected measurement duration for progress animation (seconds)
    private let expectedMeasurementDuration: TimeInterval = 10.0

    // MARK: - Initialization

    init(bleManager: BLEManager) {
        self.bleManager = bleManager
        setupSubscriptions()
    }

    private func setupSubscriptions() {
        guard let bleManager = bleManager else { return }

        // Subscribe to stability changes
        bleManager.$isStable
            .receive(on: DispatchQueue.main)
            .sink { [weak self] stable in
                self?.isStable = stable
            }
            .store(in: &cancellables)

        // Subscribe to weight changes
        bleManager.$currentWeightG
            .receive(on: DispatchQueue.main)
            .sink { [weak self] weight in
                self?.currentWeightG = weight
            }
            .store(in: &cancellables)

        // Subscribe to calibration measurement state
        bleManager.$isCalMeasuring
            .receive(on: DispatchQueue.main)
            .sink { [weak self] measuring in
                self?.handleCalMeasuringChange(measuring)
            }
            .store(in: &cancellables)

        // Subscribe to calibration result ready
        bleManager.$isCalResultReady
            .combineLatest(bleManager.$calibrationRawADC)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] ready, adc in
                if ready, let adc = adc {
                    self?.handleCalibrationResult(adc: adc)
                }
            }
            .store(in: &cancellables)

        // Subscribe to connection state
        bleManager.$connectionState
            .receive(on: DispatchQueue.main)
            .sink { [weak self] connectionState in
                guard let self = self else { return }
                if !connectionState.isConnected && self.state != .notStarted && self.state != .complete(scaleFactor: 0) {
                    // Handle unexpected disconnect during calibration
                    if case .failed = self.state {
                        // Already failed, don't override
                    } else if case .complete = self.state {
                        // Already complete, don't override
                    } else {
                        self.state = .failed(.disconnected)
                        self.cleanup()
                    }
                }
            }
            .store(in: &cancellables)
    }

    // MARK: - Public API

    /// Start the calibration flow
    func startCalibration() {
        guard let bleManager = bleManager, bleManager.connectionState.isConnected else {
            state = .failed(.notConnected)
            return
        }

        logger.info("Starting calibration flow")

        // Cancel idle disconnect timer - we need to stay connected during calibration
        bleManager.cancelIdleDisconnectTimer()

        // Reset state
        emptyADC = nil
        fullADC = nil
        measurementProgress = 0.0

        // Move to first step
        state = .emptyBottlePrompt
    }

    /// User confirms empty bottle is placed, start measurement
    func confirmEmptyBottle() {
        guard let bleManager = bleManager, bleManager.connectionState.isConnected else {
            state = .failed(.notConnected)
            return
        }

        guard case .emptyBottlePrompt = state else {
            logger.warning("confirmEmptyBottle called in wrong state: \(String(describing: self.state))")
            return
        }

        logger.info("Starting empty bottle measurement")
        state = .measuringEmpty
        measurementProgress = 0.0

        // Start progress animation
        startProgressAnimation()

        // Start timeout
        startMeasurementTimeout()

        // Send command to firmware
        bleManager.sendCalMeasurePointCommand(pointType: .empty)
    }

    /// User confirms full bottle is placed, start measurement
    func confirmFullBottle() {
        guard let bleManager = bleManager, bleManager.connectionState.isConnected else {
            state = .failed(.notConnected)
            return
        }

        guard case .fullBottlePrompt = state, emptyADC != nil else {
            logger.warning("confirmFullBottle called in wrong state or missing emptyADC")
            return
        }

        logger.info("Starting full bottle measurement")
        state = .measuringFull
        measurementProgress = 0.0

        // Start progress animation
        startProgressAnimation()

        // Start timeout
        startMeasurementTimeout()

        // Send command to firmware
        bleManager.sendCalMeasurePointCommand(pointType: .full)
    }

    /// Proceed from empty measurement to full bottle prompt
    func proceedToFullBottle() {
        guard case .emptyMeasured(let adc) = state else {
            logger.warning("proceedToFullBottle called in wrong state")
            return
        }

        logger.info("Proceeding to full bottle step (emptyADC=\(adc))")
        emptyADC = adc
        state = .fullBottlePrompt
    }

    /// Complete calibration by saving to firmware
    func completeCalibration() {
        guard case .fullMeasured(let fullADCValue) = state, let emptyADCValue = emptyADC else {
            logger.warning("completeCalibration called in wrong state or missing ADC values")
            return
        }

        guard let bleManager = bleManager, bleManager.connectionState.isConnected else {
            state = .failed(.notConnected)
            return
        }

        // Validate measurements
        guard fullADCValue > emptyADCValue else {
            state = .failed(.invalidMeasurement("Full bottle reading must be greater than empty bottle reading. Please ensure the bottle was properly filled."))
            // Restart idle timer since calibration failed
            bleManager.startIdleDisconnectTimer()
            return
        }

        // Calculate scale factor
        let bottleCapacityMl = Float(bleManager.bottleCapacityMl)
        let scaleFactor = Float(fullADCValue - emptyADCValue) / bottleCapacityMl

        logger.info("Saving calibration: emptyADC=\(emptyADCValue), fullADC=\(fullADCValue), scaleFactor=\(scaleFactor)")

        state = .savingCalibration
        fullADC = fullADCValue

        // Send calibration data to firmware
        bleManager.sendCalSetDataCommand(emptyADC: emptyADCValue, fullADC: fullADCValue, scaleFactor: scaleFactor)

        // Wait briefly for firmware to process, then mark complete
        // The firmware doesn't send an explicit confirmation, so we assume success after a short delay
        Task {
            try? await Task.sleep(nanoseconds: 500_000_000) // 0.5 seconds
            await MainActor.run {
                if case .savingCalibration = self.state {
                    self.state = .complete(scaleFactor: scaleFactor)
                    self.logger.info("Calibration complete")
                    // Restart idle disconnect timer now that calibration is done
                    self.bleManager?.startIdleDisconnectTimer()
                }
            }
        }
    }

    /// Cancel the calibration flow
    func cancel() {
        logger.info("Calibration cancelled")
        cleanup()
        state = .notStarted
        // Restart idle disconnect timer since calibration is done
        bleManager?.startIdleDisconnectTimer()
    }

    /// Retry calibration from beginning
    func retry() {
        cleanup()
        startCalibration()
    }

    /// Retry from current step (after failure)
    func retryCurrentStep() {
        cleanup()

        // Determine which step to retry
        if emptyADC == nil {
            state = .emptyBottlePrompt
        } else {
            state = .fullBottlePrompt
        }
    }

    // MARK: - Private Methods

    /// Handle calibration measuring state change from BLE
    private func handleCalMeasuringChange(_ measuring: Bool) {
        if measuring {
            logger.debug("Firmware started calibration measurement")
        } else {
            logger.debug("Firmware stopped calibration measurement")
        }
    }

    /// Handle calibration result from BLE
    private func handleCalibrationResult(adc: Int32) {
        // Cancel timeout since we got a result
        timeoutTask?.cancel()
        timeoutTask = nil

        // Stop progress animation
        stopProgressAnimation()
        measurementProgress = 1.0

        logger.info("Received calibration ADC result: \(adc)")

        switch state {
        case .measuringEmpty:
            emptyADC = adc
            state = .emptyMeasured(adc: adc)
            logger.info("Empty bottle measurement complete: ADC=\(adc)")

        case .measuringFull:
            fullADC = adc
            state = .fullMeasured(adc: adc)
            logger.info("Full bottle measurement complete: ADC=\(adc)")

        default:
            logger.warning("Received calibration result in unexpected state: \(String(describing: self.state))")
        }
    }

    /// Start the progress animation timer
    private func startProgressAnimation() {
        progressTimer?.invalidate()

        let updateInterval: TimeInterval = 0.1 // 100ms updates
        let incrementPerUpdate = updateInterval / expectedMeasurementDuration

        progressTimer = Timer.scheduledTimer(withTimeInterval: updateInterval, repeats: true) { [weak self] timer in
            Task { @MainActor in
                guard let self = self else {
                    timer.invalidate()
                    return
                }

                if self.measurementProgress < 0.95 {
                    self.measurementProgress = min(self.measurementProgress + incrementPerUpdate, 0.95)
                } else {
                    // Hold at 95% until result arrives
                    timer.invalidate()
                }
            }
        }
    }

    /// Stop the progress animation timer
    private func stopProgressAnimation() {
        progressTimer?.invalidate()
        progressTimer = nil
    }

    /// Start the measurement timeout
    private func startMeasurementTimeout() {
        timeoutTask?.cancel()

        timeoutTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(self?.measurementTimeout ?? 30.0) * 1_000_000_000)

            guard !Task.isCancelled else { return }

            await MainActor.run { [weak self] in
                guard let self = self else { return }

                if self.state.isMeasuring {
                    self.logger.warning("Calibration measurement timeout")
                    self.cleanup()
                    self.state = .failed(.measurementTimeout)
                    // Restart idle timer since calibration failed
                    self.bleManager?.startIdleDisconnectTimer()
                }
            }
        }
    }

    /// Clean up timers and tasks
    private func cleanup() {
        progressTimer?.invalidate()
        progressTimer = nil
        timeoutTask?.cancel()
        timeoutTask = nil
        measurementProgress = 0.0
    }

    deinit {
        progressTimer?.invalidate()
        timeoutTask?.cancel()
    }
}
