//
//  CalibrationView.swift
//  Aquavate
//
//  Main calibration UI with guided screens for two-point calibration.
//  Uses CalibrationManager state machine to coordinate the flow.
//

import SwiftUI
import UIKit

/// Main calibration view with guided screens
struct CalibrationView: View {
    @EnvironmentObject private var bleManager: BLEManager
    @StateObject private var calibrationManager: CalibrationManager
    @Environment(\.dismiss) private var dismiss

    init() {
        // CalibrationManager will be properly initialized in onAppear
        // This is a placeholder that will be replaced
        _calibrationManager = StateObject(wrappedValue: CalibrationManager(bleManager: BLEManager()))
    }

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                // Progress indicator (shown for all states except notStarted)
                if calibrationManager.state.stepNumber > 0 {
                    CalibrationProgressView(currentStep: calibrationManager.state.stepNumber)
                        .padding(.vertical)

                    Divider()
                }

                // Content based on state
                ScrollView {
                    contentForState
                        .padding()
                }
            }
            .navigationTitle("Calibrate Bottle")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    if calibrationManager.state != .notStarted {
                        Button("Cancel") {
                            calibrationManager.cancel()
                            dismiss()
                        }
                    }
                }
            }
            .onAppear {
                // Re-initialize with proper bleManager when view appears
                calibrationManager.cancel()
            }
        }
    }

    // MARK: - Content Views

    @ViewBuilder
    private var contentForState: some View {
        switch calibrationManager.state {
        case .notStarted:
            welcomeScreen

        case .emptyBottlePrompt, .measuringEmpty, .emptyMeasured:
            // Combined Step 1 screen: prompt + measuring progress on same screen
            emptyBottleScreen

        case .fullBottlePrompt, .measuringFull, .fullMeasured:
            // Combined Step 2 screen: prompt + measuring progress on same screen
            fullBottleScreen

        case .savingCalibration:
            savingScreen

        case .complete(let scaleFactor):
            completeScreen(scaleFactor: scaleFactor)

        case .failed(let error):
            failedScreen(error: error)
        }
    }

    // MARK: - Welcome Screen

    private var welcomeScreen: some View {
        VStack(spacing: 24) {
            Spacer()

            BottleGraphicView(fillPercent: 0.0, targetHeight: 160)

            Text("Calibrate Your Bottle")
                .font(.title2)
                .fontWeight(.bold)

            Text("Calibration teaches your bottle to measure water accurately. You'll weigh it empty, then full with a known amount.")
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal)

            Spacer()

            CalibrationActionButton(
                title: "Start Calibration",
                isEnabled: bleManager.connectionState.isConnected
            ) {
                calibrationManager.startCalibration()
            }

            if !bleManager.connectionState.isConnected {
                Text("Connect to your bottle to begin calibration")
                    .font(.caption)
                    .foregroundColor(.orange)
            }
        }
    }

    // MARK: - Empty Bottle Screen (Combined prompt + measuring)

    /// Whether we're currently measuring the empty bottle
    private var isMeasuringEmpty: Bool {
        if case .measuringEmpty = calibrationManager.state { return true }
        return false
    }

    private var emptyBottleScreen: some View {
        VStack(spacing: 24) {
            Spacer()

            BottleGraphicView(fillPercent: 0.0, targetHeight: 160)

            VStack(spacing: 8) {
                Text("Step 1: Empty Bottle")
                    .font(.title3)
                    .fontWeight(.semibold)

                VStack(alignment: .leading, spacing: 8) {
                    instructionRow(number: 1, text: "Empty your bottle completely and put the cap on")
                    instructionRow(number: 2, text: "Set the bottle upright on a flat surface")
                }
                .padding()
            }

            // Show measuring progress or stability indicator based on state
            if isMeasuringEmpty {
                MeasurementProgressView(
                    progress: calibrationManager.measurementProgress,
                    message: "Keep the bottle still"
                )
            } else {
                StabilityIndicator(
                    isStable: calibrationManager.isStable,
                    currentWeightG: calibrationManager.currentWeightG
                )

                HStack(spacing: 8) {
                    ProgressView()
                    Text("Waiting for stable measurement...")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
                .padding()
            }

            Spacer()
        }
    }

    // MARK: - Full Bottle Screen (Combined prompt + measuring)

    /// Whether we're currently measuring the full bottle
    private var isMeasuringFull: Bool {
        if case .measuringFull = calibrationManager.state { return true }
        return false
    }

    private var fullBottleScreen: some View {
        VStack(spacing: 24) {
            Spacer()

            BottleGraphicView(fillPercent: 1.0, targetHeight: 160)

            VStack(spacing: 8) {
                Text("Step 2: Full Bottle")
                    .font(.title3)
                    .fontWeight(.semibold)

                VStack(alignment: .leading, spacing: 8) {
                    instructionRow(number: 1, text: "Fill the bottle with exactly \(bleManager.bottleCapacityMl)ml of water")
                    instructionRow(number: 2, text: "Put the cap on and set it upright on a flat surface")
                }
                .padding()
            }

            // Show measuring progress or stability indicator based on state
            if isMeasuringFull {
                MeasurementProgressView(
                    progress: calibrationManager.measurementProgress,
                    message: "Keep the bottle still"
                )
            } else {
                StabilityIndicator(
                    isStable: calibrationManager.isStable,
                    currentWeightG: calibrationManager.currentWeightG
                )

                HStack(spacing: 8) {
                    ProgressView()
                    Text("Waiting for stable measurement...")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
                .padding()
            }

            Spacer()
        }
    }

    // MARK: - Saving Screen

    private var savingScreen: some View {
        VStack(spacing: 24) {
            Spacer()

            ProgressView()
                .scaleEffect(1.5)

            Text("Saving Calibration...")
                .font(.title3)
                .fontWeight(.semibold)

            Text("Writing calibration data to your bottle")
                .font(.subheadline)
                .foregroundColor(.secondary)

            Spacer()
        }
    }

    // MARK: - Complete Screen

    private func completeScreen(scaleFactor: Float) -> some View {
        VStack(spacing: 24) {
            Spacer()

            Image(systemName: "checkmark.seal.fill")
                .font(.system(size: 80))
                .foregroundColor(.green)

            Text("Calibration Complete")
                .font(.title2)
                .fontWeight(.bold)

            Text("Your bottle is now calibrated and ready for accurate drink tracking.")
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal)

            CalibrationResultsCard(
                emptyWeightG: estimatedWeight(from: calibrationManager.state),
                fullWeightG: estimatedFullWeight(from: calibrationManager.state),
                waterCapacityMl: bleManager.bottleCapacityMl,
                scaleFactor: scaleFactor
            )

            Spacer()

            CalibrationActionButton(
                title: "Done",
                isEnabled: true
            ) {
                dismiss()
            }
        }
    }

    // MARK: - Failed Screen

    private func failedScreen(error: CalibrationError) -> some View {
        VStack(spacing: 24) {
            Spacer()

            Image(systemName: "xmark.circle.fill")
                .font(.system(size: 60))
                .foregroundColor(.red)

            Text("Calibration Failed")
                .font(.title3)
                .fontWeight(.semibold)

            Text(error.localizedDescription)
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal)

            Spacer()

            VStack(spacing: 12) {
                CalibrationActionButton(
                    title: "Retry",
                    isEnabled: true
                ) {
                    calibrationManager.retry()
                }

                CalibrationSecondaryButton(
                    title: "Cancel",
                    role: .cancel
                ) {
                    dismiss()
                }
            }
        }
    }

    // MARK: - Helpers

    private func instructionRow(number: Int, text: String) -> some View {
        HStack(alignment: .top, spacing: 12) {
            Text("\(number).")
                .fontWeight(.semibold)
                .foregroundColor(.blue)
            Text(text)
                .foregroundColor(.secondary)
        }
        .font(.subheadline)
    }

    private func estimatedWeight(from state: CalibrationState) -> Int {
        // Rough estimate based on typical ADC values (placeholder)
        // In practice, we'd need to store the weight reading when measured
        if case .complete = state {
            return 245 // Typical empty bottle weight
        }
        return calibrationManager.currentWeightG
    }

    private func estimatedFullWeight(from state: CalibrationState) -> Int {
        if case .complete = state {
            return estimatedWeight(from: state) + bleManager.bottleCapacityMl
        }
        return calibrationManager.currentWeightG
    }
}

// MARK: - CalibrationView with Environment Injection

/// Wrapper that properly injects the BLEManager into CalibrationManager
struct CalibrationViewWrapper: View {
    @EnvironmentObject private var bleManager: BLEManager

    var body: some View {
        CalibrationViewContent(bleManager: bleManager)
    }
}

/// Internal view with proper CalibrationManager initialization
private struct CalibrationViewContent: View {
    @ObservedObject var bleManager: BLEManager
    @StateObject private var calibrationManager: CalibrationManager
    @Environment(\.dismiss) private var dismiss

    init(bleManager: BLEManager) {
        self.bleManager = bleManager
        _calibrationManager = StateObject(wrappedValue: CalibrationManager(bleManager: bleManager))
    }

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                // Progress indicator (shown for all states except notStarted)
                if calibrationManager.state.stepNumber > 0 {
                    CalibrationProgressView(currentStep: calibrationManager.state.stepNumber)
                        .padding(.vertical)

                    Divider()
                }

                // Content based on state
                ScrollView {
                    contentForState
                        .padding()
                }
            }
            .navigationTitle("Calibrate Bottle")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    if calibrationManager.state != .notStarted {
                        Button("Cancel") {
                            calibrationManager.cancel()
                            dismiss()
                        }
                    }
                }
            }
        }
    }

    // MARK: - Content Views

    @ViewBuilder
    private var contentForState: some View {
        switch calibrationManager.state {
        case .notStarted:
            welcomeScreen

        case .emptyBottlePrompt, .measuringEmpty, .emptyMeasured:
            // Combined Step 1 screen: prompt + measuring progress on same screen
            emptyBottleScreen

        case .fullBottlePrompt, .measuringFull, .fullMeasured:
            // Combined Step 2 screen: prompt + measuring progress on same screen
            fullBottleScreen

        case .savingCalibration:
            savingScreen

        case .complete(let scaleFactor):
            completeScreen(scaleFactor: scaleFactor)

        case .failed(let error):
            failedScreen(error: error)
        }
    }

    // MARK: - Welcome Screen

    private var welcomeScreen: some View {
        VStack(spacing: 24) {
            Spacer()

            BottleGraphicView(fillPercent: 0.0, targetHeight: 160)

            Text("Calibrate Your Bottle")
                .font(.title2)
                .fontWeight(.bold)

            Text("Calibration teaches your bottle to measure water accurately. You'll weigh it empty, then full with a known amount.")
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal)

            Spacer()

            CalibrationActionButton(
                title: "Start Calibration",
                isEnabled: bleManager.connectionState.isConnected
            ) {
                calibrationManager.startCalibration()
            }

            if !bleManager.connectionState.isConnected {
                Text("Connect to your bottle to begin calibration")
                    .font(.caption)
                    .foregroundColor(.orange)
            }
        }
    }

    // MARK: - Empty Bottle Screen (Combined prompt + measuring)

    /// Whether we're currently measuring the empty bottle
    private var isMeasuringEmpty: Bool {
        if case .measuringEmpty = calibrationManager.state { return true }
        return false
    }

    private var emptyBottleScreen: some View {
        VStack(spacing: 24) {
            Spacer()

            BottleGraphicView(fillPercent: 0.0, targetHeight: 160)

            VStack(spacing: 8) {
                Text("Step 1: Empty Bottle")
                    .font(.title3)
                    .fontWeight(.semibold)

                VStack(alignment: .leading, spacing: 8) {
                    instructionRow(number: 1, text: "Empty your bottle completely and put the cap on")
                    instructionRow(number: 2, text: "Set the bottle upright on a flat surface")
                }
                .padding()
            }

            // Show measuring progress or stability indicator based on state
            if isMeasuringEmpty {
                MeasurementProgressView(
                    progress: calibrationManager.measurementProgress,
                    message: "Keep the bottle still"
                )
            } else {
                StabilityIndicator(
                    isStable: calibrationManager.isStable,
                    currentWeightG: calibrationManager.currentWeightG
                )

                HStack(spacing: 8) {
                    ProgressView()
                    Text("Waiting for stable measurement...")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
                .padding()
            }

            Spacer()
        }
    }

    // MARK: - Full Bottle Screen (Combined prompt + measuring)

    /// Whether we're currently measuring the full bottle
    private var isMeasuringFull: Bool {
        if case .measuringFull = calibrationManager.state { return true }
        return false
    }

    private var fullBottleScreen: some View {
        VStack(spacing: 24) {
            Spacer()

            BottleGraphicView(fillPercent: 1.0, targetHeight: 160)

            VStack(spacing: 8) {
                Text("Step 2: Full Bottle")
                    .font(.title3)
                    .fontWeight(.semibold)

                VStack(alignment: .leading, spacing: 8) {
                    instructionRow(number: 1, text: "Fill the bottle with exactly \(bleManager.bottleCapacityMl)ml of water")
                    instructionRow(number: 2, text: "Put the cap on and set it upright on a flat surface")
                }
                .padding()
            }

            // Show measuring progress or stability indicator based on state
            if isMeasuringFull {
                MeasurementProgressView(
                    progress: calibrationManager.measurementProgress,
                    message: "Keep the bottle still"
                )
            } else {
                StabilityIndicator(
                    isStable: calibrationManager.isStable,
                    currentWeightG: calibrationManager.currentWeightG
                )

                HStack(spacing: 8) {
                    ProgressView()
                    Text("Waiting for stable measurement...")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
                .padding()
            }

            Spacer()
        }
    }

    // MARK: - Saving Screen

    private var savingScreen: some View {
        VStack(spacing: 24) {
            Spacer()

            ProgressView()
                .scaleEffect(1.5)

            Text("Saving Calibration...")
                .font(.title3)
                .fontWeight(.semibold)

            Text("Writing calibration data to your bottle")
                .font(.subheadline)
                .foregroundColor(.secondary)

            Spacer()
        }
    }

    // MARK: - Complete Screen

    private func completeScreen(scaleFactor: Float) -> some View {
        VStack(spacing: 24) {
            Spacer()

            Image(systemName: "checkmark.seal.fill")
                .font(.system(size: 80))
                .foregroundColor(.green)

            Text("Calibration Complete")
                .font(.title2)
                .fontWeight(.bold)

            Text("Your bottle is now calibrated and ready for accurate drink tracking.")
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal)

            CalibrationResultsCard(
                emptyWeightG: 245, // Placeholder - would need to track actual weights
                fullWeightG: 245 + bleManager.bottleCapacityMl,
                waterCapacityMl: bleManager.bottleCapacityMl,
                scaleFactor: scaleFactor
            )

            Spacer()

            CalibrationActionButton(
                title: "Done",
                isEnabled: true
            ) {
                dismiss()
            }
        }
    }

    // MARK: - Failed Screen

    private func failedScreen(error: CalibrationError) -> some View {
        VStack(spacing: 24) {
            Spacer()

            Image(systemName: "xmark.circle.fill")
                .font(.system(size: 60))
                .foregroundColor(.red)

            Text("Calibration Failed")
                .font(.title3)
                .fontWeight(.semibold)

            Text(error.localizedDescription)
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal)

            Spacer()

            VStack(spacing: 12) {
                CalibrationActionButton(
                    title: "Retry",
                    isEnabled: true
                ) {
                    calibrationManager.retry()
                }

                CalibrationSecondaryButton(
                    title: "Cancel",
                    role: .cancel
                ) {
                    dismiss()
                }
            }
        }
    }

    // MARK: - Helpers

    private func instructionRow(number: Int, text: String) -> some View {
        HStack(alignment: .top, spacing: 12) {
            Text("\(number).")
                .fontWeight(.semibold)
                .foregroundColor(.blue)
            Text(text)
                .foregroundColor(.secondary)
        }
        .font(.subheadline)
    }
}

// MARK: - Preview

#Preview {
    CalibrationViewWrapper()
        .environmentObject(BLEManager())
}
