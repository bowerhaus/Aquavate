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

    /// Round a value to the nearest 50ml
    private func roundToNearest50(_ value: Int) -> Int {
        return ((value + 25) / 50) * 50
    }

    @ViewBuilder
    private var statusText: some View {
        if let state = state {
            if state.isGoalAchieved {
                Text("Goal reached! 🎉")
            } else {
                // Round deficit to nearest 50ml, only show if >= 50ml
                let roundedDeficit = roundToNearest50(state.deficitMl)
                if roundedDeficit >= 50 {
                    // Format deficit for display
                    if roundedDeficit >= 1000 {
                        Text("\(String(format: "%.1f", Double(roundedDeficit) / 1000.0))L to catch up")
                    } else {
                        Text("\(roundedDeficit)ml to catch up")
                    }
                } else {
                    Text("On track ✓")
                }
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

            // Status text (deficit or on track)
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
