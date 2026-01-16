//
//  HomeView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI

struct HomeView: View {
    @EnvironmentObject var bleManager: BLEManager

    // Mock data for drink history (will be replaced with CoreData in Phase 4.3-4.4)
    let drinks = DrinkRecord.sampleData

    private var todaysDrinks: [DrinkRecord] {
        let calendar = Calendar.current
        let today = calendar.startOfDay(for: Date())
        return drinks.filter { calendar.startOfDay(for: $0.timestamp) == today }
    }

    // Use BLE data when connected, otherwise calculate from mock drinks
    private var displayDailyTotal: Int {
        if bleManager.connectionState.isConnected {
            return bleManager.dailyTotalMl
        }
        return todaysDrinks.reduce(0) { $0 + $1.amountMl }
    }

    // Use BLE data when connected, otherwise use sample bottle
    private var displayBottleLevel: Int {
        if bleManager.connectionState.isConnected {
            return bleManager.bottleLevelMl
        }
        return Bottle.sample.currentLevelMl
    }

    private var displayCapacity: Int {
        if bleManager.connectionState.isConnected {
            return bleManager.bottleCapacityMl
        }
        return Bottle.sample.capacityMl
    }

    private var displayGoal: Int {
        if bleManager.connectionState.isConnected {
            return bleManager.dailyGoalMl
        }
        return Bottle.sample.dailyGoalMl
    }

    private var dailyProgress: Double {
        guard displayGoal > 0 else { return 0 }
        return min(1.0, Double(displayDailyTotal) / Double(displayGoal))
    }

    private var recentDrinks: [DrinkRecord] {
        Array(todaysDrinks.prefix(5))
    }

    // Connection status text
    private var connectionStatusText: String {
        switch bleManager.connectionState {
        case .disconnected:
            return "Not connected"
        case .scanning:
            return "Scanning..."
        case .connecting:
            return "Connecting..."
        case .discovering:
            return "Discovering..."
        case .connected:
            if let lastUpdate = bleManager.lastStateUpdateTime {
                let elapsed = Date().timeIntervalSince(lastUpdate)
                if elapsed < 60 {
                    return "Updated just now"
                } else if elapsed < 3600 {
                    return "Updated \(Int(elapsed / 60))m ago"
                } else {
                    return "Updated \(Int(elapsed / 3600))h ago"
                }
            }
            return "Connected"
        }
    }

    // Connection status color
    private var connectionStatusColor: Color {
        switch bleManager.connectionState {
        case .connected: return .green
        case .scanning, .connecting, .discovering: return .orange
        case .disconnected: return .gray
        }
    }

    var body: some View {
        NavigationView {
            ScrollView {
                VStack(spacing: 24) {
                    // Connection status indicator
                    HStack(spacing: 6) {
                        Circle()
                            .fill(connectionStatusColor)
                            .frame(width: 8, height: 8)
                        Text(connectionStatusText)
                            .font(.caption)
                            .foregroundStyle(.secondary)

                        if bleManager.connectionState.isConnected {
                            Spacer()
                            // Battery indicator
                            HStack(spacing: 4) {
                                Image(systemName: batteryIconName)
                                    .font(.caption)
                                    .foregroundStyle(batteryColor)
                                Text("\(bleManager.batteryPercent)%")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                    .padding(.horizontal)
                    .padding(.top, 8)

                    // Status flags (when connected)
                    if bleManager.connectionState.isConnected {
                        HStack(spacing: 12) {
                            StatusBadge(
                                label: "Calibrated",
                                isActive: bleManager.isCalibrated,
                                activeColor: .green,
                                inactiveColor: .orange
                            )
                            StatusBadge(
                                label: "Stable",
                                isActive: bleManager.isStable,
                                activeColor: .blue,
                                inactiveColor: .gray
                            )
                            if bleManager.unsyncedCount > 0 {
                                StatusBadge(
                                    label: "\(bleManager.unsyncedCount) unsynced",
                                    isActive: true,
                                    activeColor: .orange,
                                    inactiveColor: .gray
                                )
                            }
                        }
                        .padding(.horizontal)
                    }

                    // Circular progress ring - bottle level
                    CircularProgressView(
                        current: displayBottleLevel,
                        total: displayCapacity,
                        color: .blue
                    )
                    .padding(.vertical, 16)

                    // Daily goal progress bar
                    VStack(alignment: .leading, spacing: 8) {
                        HStack {
                            Text("Today's Goal")
                                .font(.headline)
                            Spacer()
                            Text("\(Int(dailyProgress * 100))%")
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                        }

                        ProgressView(value: dailyProgress)
                            .tint(.blue)

                        HStack {
                            Text("\(displayDailyTotal)ml")
                                .font(.subheadline)
                                .fontWeight(.medium)
                            Text("/ \(displayGoal)ml")
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                        }
                    }
                    .padding(.horizontal)

                    Divider()
                        .padding(.horizontal)

                    // Recent drinks list
                    VStack(alignment: .leading, spacing: 12) {
                        Text("Recent Drinks")
                            .font(.headline)
                            .padding(.horizontal)

                        if recentDrinks.isEmpty {
                            Text("No drinks recorded today")
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 32)
                        } else {
                            ForEach(recentDrinks) { drink in
                                DrinkListItem(drink: drink)
                                    .padding(.horizontal)

                                if drink.id != recentDrinks.last?.id {
                                    Divider()
                                        .padding(.leading, 60)
                                }
                            }
                        }
                    }

                    Spacer(minLength: 20)
                }
            }
            .navigationTitle("Aquavate")
        }
    }

    // MARK: - Battery Helpers

    private var batteryIconName: String {
        let percent = bleManager.batteryPercent
        if percent > 75 {
            return "battery.100"
        } else if percent > 50 {
            return "battery.75"
        } else if percent > 25 {
            return "battery.50"
        } else if percent > 10 {
            return "battery.25"
        } else {
            return "battery.0"
        }
    }

    private var batteryColor: Color {
        let percent = bleManager.batteryPercent
        if percent > 20 {
            return .green
        } else if percent > 10 {
            return .orange
        } else {
            return .red
        }
    }
}

// MARK: - Status Badge Component

struct StatusBadge: View {
    let label: String
    let isActive: Bool
    let activeColor: Color
    let inactiveColor: Color

    var body: some View {
        HStack(spacing: 4) {
            Circle()
                .fill(isActive ? activeColor : inactiveColor)
                .frame(width: 6, height: 6)
            Text(label)
                .font(.caption2)
                .foregroundStyle(isActive ? .primary : .secondary)
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 4)
        .background(
            RoundedRectangle(cornerRadius: 8)
                .fill((isActive ? activeColor : inactiveColor).opacity(0.1))
        )
    }
}

#Preview {
    HomeView()
        .environmentObject(BLEManager())
}
