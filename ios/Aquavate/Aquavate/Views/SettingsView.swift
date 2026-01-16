//
//  SettingsView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI

struct SettingsView: View {
    let bottle = Bottle.sample
    @State private var useOunces = false
    @State private var notificationsEnabled = true

    private var lastSyncTime: String {
        let formatter = DateFormatter()
        formatter.timeStyle = .short
        return formatter.string(from: Date().addingTimeInterval(-300)) // 5 min ago
    }

    var body: some View {
        NavigationView {
            List {
                // Bottle Configuration
                Section("Bottle Configuration") {
                    HStack {
                        Text("Name")
                        Spacer()
                        Text(bottle.name)
                            .foregroundStyle(.secondary)
                    }

                    HStack {
                        Text("Capacity")
                        Spacer()
                        Text("\(bottle.capacityMl)ml")
                            .foregroundStyle(.secondary)
                    }

                    HStack {
                        Text("Daily Goal")
                        Spacer()
                        Text("\(bottle.dailyGoalMl)ml")
                            .foregroundStyle(.secondary)
                    }
                }

                // Device Section
                Section("Device") {
                    HStack {
                        Image(systemName: "battery.75")
                            .foregroundStyle(.green)
                        Text("Battery")
                        Spacer()
                        Text("\(bottle.batteryPercent)%")
                            .foregroundStyle(.secondary)
                    }

                    HStack {
                        Image(systemName: "bluetooth")
                            .foregroundStyle(.blue)
                        Text("Status")
                        Spacer()
                        HStack(spacing: 4) {
                            Circle()
                                .fill(.green)
                                .frame(width: 8, height: 8)
                            Text("Connected")
                                .foregroundStyle(.secondary)
                        }
                    }

                    HStack {
                        Image(systemName: "arrow.triangle.2.circlepath")
                            .foregroundStyle(.secondary)
                        Text("Last Sync")
                        Spacer()
                        Text(lastSyncTime)
                            .foregroundStyle(.secondary)
                    }
                }

                // Preferences
                Section("Preferences") {
                    Toggle(isOn: $useOunces) {
                        HStack {
                            Image(systemName: "ruler")
                                .foregroundStyle(.orange)
                            Text("Use Ounces (oz)")
                        }
                    }

                    Toggle(isOn: $notificationsEnabled) {
                        HStack {
                            Image(systemName: "bell.fill")
                                .foregroundStyle(.purple)
                            Text("Notifications")
                        }
                    }
                }

                // About
                Section("About") {
                    HStack {
                        Text("Version")
                        Spacer()
                        Text("1.0.0 (Placeholder UI)")
                            .foregroundStyle(.secondary)
                            .font(.caption)
                    }

                    Link(destination: URL(string: "https://github.com/bowerhaus/Aquavate")!) {
                        HStack {
                            Image(systemName: "link")
                            Text("GitHub Repository")
                            Spacer()
                            Image(systemName: "arrow.up.forward")
                                .font(.caption)
                        }
                    }
                }
            }
            .navigationTitle("Settings")
        }
    }
}

#Preview {
    SettingsView()
}
