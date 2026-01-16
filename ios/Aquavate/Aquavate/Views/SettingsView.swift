//
//  SettingsView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var bleManager: BLEManager
    let bottle = Bottle.sample
    @State private var useOunces = false
    @State private var notificationsEnabled = true

    private var connectionStatusColor: Color {
        switch bleManager.connectionState {
        case .connected:
            return .green
        case .scanning, .connecting, .discovering:
            return .orange
        case .disconnected:
            return .gray
        }
    }

    private var connectionStatusText: String {
        if let deviceName = bleManager.connectedDeviceName, bleManager.connectionState.isConnected {
            return deviceName
        }
        return bleManager.connectionState.rawValue
    }

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

    var body: some View {
        NavigationView {
            List {
                // Device Connection Section
                Section("Device Connection") {
                    HStack {
                        Image(systemName: "bluetooth")
                            .foregroundStyle(.blue)
                        Text("Status")
                        Spacer()
                        HStack(spacing: 4) {
                            if bleManager.connectionState == .scanning || bleManager.connectionState == .connecting {
                                ProgressView()
                                    .scaleEffect(0.7)
                            } else {
                                Circle()
                                    .fill(connectionStatusColor)
                                    .frame(width: 8, height: 8)
                            }
                            Text(connectionStatusText)
                                .foregroundStyle(.secondary)
                                .font(.subheadline)
                        }
                    }

                    if bleManager.connectionState.isConnected {
                        Button(role: .destructive) {
                            bleManager.disconnect()
                        } label: {
                            HStack {
                                Image(systemName: "xmark.circle")
                                Text("Disconnect")
                            }
                        }
                    } else if bleManager.connectionState == .disconnected {
                        Button {
                            bleManager.startScanning()
                        } label: {
                            HStack {
                                Image(systemName: "antenna.radiowaves.left.and.right")
                                Text("Scan for Device")
                            }
                        }
                        .disabled(!bleManager.isBluetoothReady)
                    } else {
                        Button(role: .cancel) {
                            bleManager.stopScanning()
                        } label: {
                            HStack {
                                Image(systemName: "xmark")
                                Text("Cancel")
                            }
                        }
                    }

                    // Show discovered devices if scanning found multiple
                    if !bleManager.discoveredDevices.isEmpty && bleManager.connectionState == .scanning {
                        ForEach(Array(bleManager.discoveredDevices.keys.sorted()), id: \.self) { deviceName in
                            Button {
                                bleManager.connect(toDeviceNamed: deviceName)
                            } label: {
                                HStack {
                                    Image(systemName: "drop.fill")
                                        .foregroundStyle(.blue)
                                    Text(deviceName)
                                    Spacer()
                                    Image(systemName: "chevron.right")
                                        .foregroundStyle(.secondary)
                                        .font(.caption)
                                }
                            }
                        }
                    }

                    if let error = bleManager.errorMessage {
                        HStack {
                            Image(systemName: "exclamationmark.triangle")
                                .foregroundStyle(.orange)
                            Text(error)
                                .foregroundStyle(.secondary)
                                .font(.caption)
                        }
                    }
                }

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

                // Device Info (when connected)
                if bleManager.connectionState.isConnected {
                    Section("Device Info") {
                        HStack {
                            Image(systemName: batteryIconName)
                                .foregroundStyle(batteryColor)
                            Text("Battery")
                            Spacer()
                            Text("\(bleManager.batteryPercent)%")
                                .foregroundStyle(.secondary)
                        }

                        HStack {
                            Image(systemName: "scalemass")
                                .foregroundStyle(.blue)
                            Text("Current Weight")
                            Spacer()
                            Text("\(bleManager.currentWeightG)g")
                                .foregroundStyle(.secondary)
                        }

                        HStack {
                            Image(systemName: bleManager.isCalibrated ? "checkmark.circle.fill" : "exclamationmark.circle")
                                .foregroundStyle(bleManager.isCalibrated ? .green : .orange)
                            Text("Calibrated")
                            Spacer()
                            Text(bleManager.isCalibrated ? "Yes" : "No")
                                .foregroundStyle(.secondary)
                        }

                        HStack {
                            Image(systemName: bleManager.isTimeValid ? "clock.fill" : "clock.badge.exclamationmark")
                                .foregroundStyle(bleManager.isTimeValid ? .green : .orange)
                            Text("Time Set")
                            Spacer()
                            Text(bleManager.isTimeValid ? "Yes" : "No")
                                .foregroundStyle(.secondary)
                        }

                        if bleManager.unsyncedCount > 0 {
                            HStack {
                                Image(systemName: "arrow.triangle.2.circlepath")
                                    .foregroundStyle(.orange)
                                Text("Unsynced Records")
                                Spacer()
                                Text("\(bleManager.unsyncedCount)")
                                    .foregroundStyle(.secondary)
                            }
                        }
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
                        Text("1.0.0 (BLE Phase 4.1)")
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
        .environmentObject(BLEManager())
}
