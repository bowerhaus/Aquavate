//
//  HumanFigureProgressView.swift
//  Aquavate
//
//  Created by Claude Code on 17/01/2026.
//

import SwiftUI

struct HumanFigureProgressView: View {
    let current: Int
    let total: Int
    let color: Color
    let label: String
    let expectedCurrent: Int?
    let deficitMl: Int
    let urgency: HydrationUrgency

    init(current: Int, total: Int, color: Color = .blue, label: String = "remaining", expectedCurrent: Int? = nil, deficitMl: Int = 0, urgency: HydrationUrgency = .onTrack) {
        self.current = current
        self.total = total
        self.color = color
        self.label = label
        self.expectedCurrent = expectedCurrent
        self.deficitMl = deficitMl
        self.urgency = urgency
    }

    private var progress: Double {
        guard total > 0 else { return 0 }
        return min(1.0, Double(current) / Double(total))
    }

    private var expectedProgress: Double {
        guard total > 0, let expected = expectedCurrent else { return 0 }
        return min(1.0, Double(expected) / Double(total))
    }

    /// Round a value to the nearest 50ml
    private func roundToNearest50(_ value: Int) -> Int {
        return ((value + 25) / 50) * 50
    }

    private var roundedDeficitMl: Int {
        roundToNearest50(deficitMl)
    }

    private var isBehindTarget: Bool {
        roundedDeficitMl >= 50
    }

    /// Progress level at which overdue begins (80% of expected = 20% behind)
    private var overdueThresholdProgress: Double {
        expectedProgress * 0.8
    }

    /// Whether deficit exceeds the overdue threshold (20%+)
    private var isOverdue: Bool {
        progress < overdueThresholdProgress
    }

    /// Top of orange zone: up to overdue threshold, or expected if not overdue
    private var orangeTopProgress: Double {
        isOverdue ? overdueThresholdProgress : expectedProgress
    }

    var body: some View {
        VStack(spacing: 16) {
            ZStack {
                // White background layer - colors with opacity blend over this
                Image("HumanFigureFilled")
                    .renderingMode(.template)
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .foregroundColor(.white)

                // Deficit zone: gradient from attention (bottom) to overdue (top of head)
                // Only show when isBehindTarget (50ml rounded threshold) for consistency with text
                // Gradient spans from current progress (yellow) to top of head (red), but:
                // - Only filled up to expected progress level
                // - White above expected progress (background shows through)
                if expectedCurrent != nil && isBehindTarget {
                    GeometryReader { geometry in
                        VStack {
                            Spacer(minLength: 0)
                            // Full gradient from progress to top of head
                            Rectangle()
                                .fill(
                                    LinearGradient(
                                        colors: [HydrationUrgency.attention.color, HydrationUrgency.overdue.color],
                                        startPoint: .bottom,
                                        endPoint: .top
                                    )
                                )
                                .frame(height: geometry.size.height * (1.0 - progress))
                                .opacity(0.7)
                        }
                        .offset(y: -geometry.size.height * progress)
                    }
                    // First mask: clip to expected progress height
                    .mask(
                        GeometryReader { geometry in
                            VStack {
                                Spacer(minLength: 0)
                                Rectangle()
                                    .frame(height: geometry.size.height * expectedProgress)
                            }
                        }
                    )
                    // Second mask: clip to human figure shape
                    .mask(
                        Image("HumanFigureFilled")
                            .resizable()
                            .aspectRatio(contentMode: .fit)
                    )
                    .animation(.easeInOut(duration: 0.5), value: expectedProgress)
                }

                // Actual progress fill from bottom using filled silhouette as mask
                GeometryReader { geometry in
                    VStack {
                        Spacer(minLength: 0)
                        Rectangle()
                            .fill(color)
                            .frame(height: geometry.size.height * progress)
                    }
                }
                .mask(
                    Image("HumanFigureFilled")
                        .resizable()
                        .aspectRatio(contentMode: .fit)
                )
                .animation(.easeInOut(duration: 0.5), value: progress)

                // Outline SVG on top
                Image("HumanFigureOutline")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
            }
            .frame(width: 132, height: 220)

            // Text below figure
            VStack(spacing: 4) {
                Text("\(current) ml")
                    .font(.system(size: 32, weight: .bold, design: .rounded))
                    .foregroundColor(.primary)

                Text(label)
                    .font(.system(size: 14, weight: .regular))
                    .foregroundColor(.secondary)

                // Behind target indicator (only show if >= 50ml, rounded to nearest 50)
                if isBehindTarget {
                    Text("\(roundedDeficitMl) ml behind target")
                        .font(.system(size: 12, weight: .medium))
                        .foregroundColor(.secondary)
                        .padding(.top, 4)
                }
            }
        }
    }
}

#Preview("0%") {
    HumanFigureProgressView(current: 0, total: 2000, color: .blue, label: "of 2000ml goal")
}

#Preview("25%") {
    HumanFigureProgressView(current: 500, total: 2000, color: .blue, label: "of 2000ml goal")
}

#Preview("50%") {
    HumanFigureProgressView(current: 1000, total: 2000, color: .blue, label: "of 2000ml goal")
}

#Preview("75%") {
    HumanFigureProgressView(current: 1500, total: 2000, color: .blue, label: "of 2000ml goal")
}

#Preview("100%") {
    HumanFigureProgressView(current: 2000, total: 2000, color: .blue, label: "of 2000ml goal")
}

#Preview("15% behind - orange only") {
    // 850 actual vs 1000 expected = 15% behind (below 20% overdue threshold)
    // Shows: blue (850) + orange (850 to 1000)
    HumanFigureProgressView(
        current: 850,
        total: 2000,
        color: .blue,
        label: "of 2000ml goal",
        expectedCurrent: 1000,
        deficitMl: 150,
        urgency: .attention
    )
}

#Preview("30% behind - orange + red") {
    // 700 actual vs 1000 expected = 30% behind (exceeds 20% overdue threshold)
    // Shows: blue (700) + orange (700 to 800) + red (800 to 1000)
    HumanFigureProgressView(
        current: 700,
        total: 2000,
        color: .blue,
        label: "of 2000ml goal",
        expectedCurrent: 1000,
        deficitMl: 300,
        urgency: .overdue
    )
}

#Preview("On Track - 60% actual, 50% expected") {
    HumanFigureProgressView(
        current: 1200,
        total: 2000,
        color: .blue,
        label: "of 2000ml goal",
        expectedCurrent: 1000,
        deficitMl: 0,
        urgency: .onTrack
    )
}
