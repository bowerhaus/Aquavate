//
//  CalibrationProgressView.swift
//  Aquavate
//
//  Step progress indicator for calibration flow.
//  Shows 3 steps: Empty, Full, Save with connecting lines.
//

import SwiftUI
import UIKit

/// Step indicator for calibration progress
struct CalibrationProgressView: View {
    /// Current step (1, 2, or 3)
    let currentStep: Int

    /// Step labels
    private let steps = ["Empty", "Full", "Save"]

    var body: some View {
        HStack(spacing: 0) {
            ForEach(0..<steps.count, id: \.self) { index in
                let stepNumber = index + 1

                // Step indicator
                StepIndicator(
                    step: stepNumber,
                    label: steps[index],
                    isCompleted: stepNumber < currentStep,
                    isCurrent: stepNumber == currentStep
                )

                // Connecting line (except after last step)
                if index < steps.count - 1 {
                    ProgressLine(completed: stepNumber < currentStep)
                }
            }
        }
        .padding(.horizontal)
    }
}

// MARK: - Step Indicator

/// Individual step circle with label
private struct StepIndicator: View {
    let step: Int
    let label: String
    let isCompleted: Bool
    let isCurrent: Bool

    var body: some View {
        VStack(spacing: 6) {
            ZStack {
                Circle()
                    .fill(fillColor)
                    .frame(width: 28, height: 28)

                if isCompleted {
                    Image(systemName: "checkmark")
                        .font(.system(size: 14, weight: .bold))
                        .foregroundColor(.white)
                } else {
                    Text("\(step)")
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundColor(isCurrent ? .white : .secondary)
                }
            }

            Text(label)
                .font(.caption)
                .fontWeight(isCurrent ? .medium : .regular)
                .foregroundColor(isCurrent || isCompleted ? .primary : .secondary)
        }
    }

    private var fillColor: Color {
        if isCompleted {
            return .green
        } else if isCurrent {
            return .blue
        } else {
            return Color(UIColor.systemGray4)
        }
    }
}

// MARK: - Progress Line

/// Connecting line between steps
private struct ProgressLine: View {
    let completed: Bool

    var body: some View {
        Rectangle()
            .fill(completed ? Color.green : Color(UIColor.systemGray4))
            .frame(height: 3)
            .frame(maxWidth: .infinity)
            .padding(.bottom, 20) // Align with circles, not labels
    }
}

// MARK: - Preview

#Preview("Step 1") {
    CalibrationProgressView(currentStep: 1)
        .padding()
}

#Preview("Step 2") {
    CalibrationProgressView(currentStep: 2)
        .padding()
}

#Preview("Step 3 (Complete)") {
    CalibrationProgressView(currentStep: 3)
        .padding()
}

#Preview("All Steps Complete") {
    CalibrationProgressView(currentStep: 4)
        .padding()
}
