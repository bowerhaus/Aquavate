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

    init(current: Int, total: Int, color: Color = .blue, label: String = "remaining") {
        self.current = current
        self.total = total
        self.color = color
        self.label = label
    }

    private var progress: Double {
        guard total > 0 else { return 0 }
        return min(1.0, Double(current) / Double(total))
    }

    var body: some View {
        VStack(spacing: 16) {
            ZStack {
                // Progress fill from bottom using filled silhouette as mask
                GeometryReader { geometry in
                    VStack {
                        Spacer()
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

                // Outline image on top
                Image("HumanFigure")
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
