//
//  CircularProgressView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI

struct CircularProgressView: View {
    let current: Int
    let total: Int
    let color: Color

    private var progress: Double {
        guard total > 0 else { return 0 }
        return Double(current) / Double(total)
    }

    var body: some View {
        ZStack {
            // Background circle
            Circle()
                .stroke(color.opacity(0.2), lineWidth: 20)

            // Progress circle
            Circle()
                .trim(from: 0, to: progress)
                .stroke(
                    color,
                    style: StrokeStyle(lineWidth: 20, lineCap: .round)
                )
                .rotationEffect(.degrees(-90))
                .animation(.easeInOut, value: progress)

            // Center text
            VStack(spacing: 4) {
                Text("\(current) ml")
                    .font(.system(size: 36, weight: .bold, design: .rounded))
                    .foregroundColor(.primary)

                Text("remaining")
                    .font(.system(size: 16, weight: .regular))
                    .foregroundColor(.secondary)
            }
        }
        .frame(width: 200, height: 200)
    }
}

#Preview {
    CircularProgressView(current: 420, total: 750, color: .blue)
}
