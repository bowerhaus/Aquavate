//
//  ContentView.swift
//  AquavateWatch
//
//  Created by Claude Code on 22/01/2026.
//

import SwiftUI

struct ContentView: View {
    @EnvironmentObject var sessionManager: WatchSessionManager

    private var state: HydrationState? {
        sessionManager.hydrationState
    }

    private var urgencyColor: Color {
        state?.urgency.color ?? .blue
    }

    private var liters: Double {
        Double(state?.dailyTotalMl ?? 0) / 1000.0
    }

    private var goalLiters: Double {
        Double(state?.dailyGoalMl ?? 2000) / 1000.0
    }

    @ViewBuilder
    private var statusText: some View {
        if let state = state {
            if state.isGoalAchieved {
                Text("Goal reached! 🎉")
            } else if state.deficitMl > 0 {
                // Format deficit for display
                if state.deficitMl >= 1000 {
                    Text("\(String(format: "%.1f", Double(state.deficitMl) / 1000.0))L to catch up")
                } else {
                    Text("\(state.deficitMl)ml to catch up")
                }
            } else {
                Text("On track ✓")
            }
        } else {
            Text("--")
        }
    }

    var body: some View {
        VStack(spacing: 12) {
            // Large colored water drop
            Image(systemName: "drop.fill")
                .font(.system(size: 60))
                .foregroundColor(urgencyColor)

            // Daily progress
            Text("\(liters, specifier: "%.1f")L / \(goalLiters, specifier: "%.1f")L")
                .font(.title3)
                .fontWeight(.medium)

            // Status text
            statusText
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .containerBackground(urgencyColor.gradient.opacity(0.2), for: .navigation)
    }
}

#Preview {
    ContentView()
        .environmentObject(WatchSessionManager.shared)
}
