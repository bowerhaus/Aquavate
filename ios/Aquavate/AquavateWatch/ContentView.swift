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

            // Progress percentage
            if let state = state {
                Text("\(Int(state.progress * 100))%")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }

            // Time since last drink
            if let lastDrink = state?.lastDrinkTime {
                Text(lastDrink, style: .relative)
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
        }
        .containerBackground(urgencyColor.gradient.opacity(0.2), for: .navigation)
    }
}

#Preview {
    ContentView()
        .environmentObject(WatchSessionManager.shared)
}
