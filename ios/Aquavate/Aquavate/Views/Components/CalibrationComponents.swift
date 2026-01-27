//
//  CalibrationComponents.swift
//  Aquavate
//
//  Reusable components for the calibration flow.
//

import SwiftUI
import UIKit

// MARK: - Stability Indicator

/// Shows current weight and stability status from BLE
struct StabilityIndicator: View {
    let isStable: Bool
    let currentWeightG: Int

    var body: some View {
        HStack {
            Label {
                Text("\(currentWeightG)g")
                    .fontWeight(.medium)
            } icon: {
                Image(systemName: "scalemass")
            }

            Spacer()

            HStack(spacing: 6) {
                Circle()
                    .fill(isStable ? Color.green : Color.orange)
                    .frame(width: 10, height: 10)

                Text(isStable ? "Stable" : "Stabilizing...")
                    .foregroundColor(isStable ? .green : .orange)
            }
        }
        .font(.subheadline)
        .padding()
        .background(Color(UIColor.secondarySystemBackground))
        .cornerRadius(10)
    }
}

// MARK: - Measurement Progress View

/// Animated progress bar during firmware measurement
struct MeasurementProgressView: View {
    let progress: Double
    let message: String

    init(progress: Double, message: String = "Keep the bottle still") {
        self.progress = progress
        self.message = message
    }

    var body: some View {
        VStack(spacing: 16) {
            ProgressView(value: progress)
                .progressViewStyle(.linear)
                .tint(.blue)
                .scaleEffect(y: 2)

            Text(message)
                .font(.subheadline)
                .foregroundColor(.secondary)
        }
        .padding(.horizontal)
    }
}

// MARK: - Calibration Results Card

/// Shows calibration results after successful completion
struct CalibrationResultsCard: View {
    let emptyWeightG: Int
    let fullWeightG: Int
    let waterCapacityMl: Int
    let scaleFactor: Float

    var body: some View {
        VStack(spacing: 12) {
            ResultRow(label: "Empty weight", value: "\(emptyWeightG)g")
            ResultRow(label: "Full weight", value: "\(fullWeightG)g")
            ResultRow(label: "Water capacity", value: "\(waterCapacityMl)ml")
            Divider()
            ResultRow(label: "Scale factor", value: String(format: "%.1f ADC/g", scaleFactor))
        }
        .padding()
        .background(Color(UIColor.secondarySystemBackground))
        .cornerRadius(12)
    }
}

/// Single row in results card
private struct ResultRow: View {
    let label: String
    let value: String

    var body: some View {
        HStack {
            Text(label)
                .foregroundColor(.secondary)
            Spacer()
            Text(value)
                .fontWeight(.medium)
        }
        .font(.subheadline)
    }
}

// MARK: - Calibration Action Button

/// Primary action button for calibration flow
struct CalibrationActionButton: View {
    let title: String
    let isEnabled: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.headline)
                .frame(maxWidth: .infinity)
                .padding()
                .background(isEnabled ? Color.blue : Color(UIColor.systemGray4))
                .foregroundColor(.white)
                .cornerRadius(12)
        }
        .disabled(!isEnabled)
    }
}

// MARK: - Calibration Secondary Button

/// Secondary button (e.g., Cancel, Retry)
struct CalibrationSecondaryButton: View {
    let title: String
    let role: ButtonRole?
    let action: () -> Void

    init(title: String, role: ButtonRole? = nil, action: @escaping () -> Void) {
        self.title = title
        self.role = role
        self.action = action
    }

    var body: some View {
        Button(role: role, action: action) {
            Text(title)
                .font(.headline)
                .frame(maxWidth: .infinity)
                .padding()
                .background(Color(UIColor.secondarySystemBackground))
                .foregroundColor(role == .destructive ? .red : .primary)
                .cornerRadius(12)
        }
    }
}

// MARK: - Previews

#Preview("Stability - Stable") {
    StabilityIndicator(isStable: true, currentWeightG: 245)
        .padding()
}

#Preview("Stability - Unstable") {
    StabilityIndicator(isStable: false, currentWeightG: 243)
        .padding()
}

#Preview("Measurement Progress") {
    MeasurementProgressView(progress: 0.6)
        .padding()
}

#Preview("Results Card") {
    CalibrationResultsCard(
        emptyWeightG: 245,
        fullWeightG: 1075,
        waterCapacityMl: 830,
        scaleFactor: 450.2
    )
    .padding()
}

#Preview("Action Buttons") {
    VStack(spacing: 16) {
        CalibrationActionButton(title: "Measure Empty", isEnabled: true) { }
        CalibrationActionButton(title: "Measure Empty", isEnabled: false) { }
        CalibrationSecondaryButton(title: "Cancel", role: .cancel) { }
    }
    .padding()
}
