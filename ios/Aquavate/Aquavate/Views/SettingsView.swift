//
//  SettingsView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var bleManager: BLEManager
    @EnvironmentObject var healthKitManager: HealthKitManager
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

                    #if DEBUG
                    // Connection controls only visible in debug builds
                    // Release builds use pull-to-refresh on HomeView for sync
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
                    #endif

                    if let error = bleManager.errorMessage {
                        HStack {
                            Image(systemName: "exclamationmark.triangle")
                                .foregroundStyle(.orange)
                            Text(error)
                                .foregroundStyle(.secondary)
                                .font(.caption)
                            Spacer()
                            Button {
                                bleManager.errorMessage = nil
                            } label: {
                                Image(systemName: "xmark.circle.fill")
                                    .foregroundStyle(.secondary)
                            }
                            .buttonStyle(.plain)
                        }
                    }

                    // Bluetooth not ready warning
                    if !bleManager.isBluetoothReady && bleManager.connectionState == .disconnected {
                        HStack {
                            Image(systemName: "bluetooth.slash")
                                .foregroundStyle(.red)
                            Text("Bluetooth is off or unavailable")
                                .foregroundStyle(.secondary)
                                .font(.caption)
                        }
                    }
                }

                // Bottle Configuration
                Section("Bottle Configuration") {
                    HStack {
                        Text("Device")
                        Spacer()
                        Text(bleManager.connectedDeviceName ?? "Not connected")
                            .foregroundStyle(.secondary)
                    }

                    HStack {
                        Text("Capacity")
                        Spacer()
                        Text("\(bleManager.bottleCapacityMl)ml")
                            .foregroundStyle(.secondary)
                    }

                    HStack {
                        Text("Daily Goal")
                        Spacer()
                        Text("\(bleManager.dailyGoalMl)ml")
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

                        // Sync status
                        if bleManager.syncState.isActive {
                            HStack {
                                ProgressView()
                                    .scaleEffect(0.7)
                                Text("Syncing...")
                                Spacer()
                                Text("\(Int(bleManager.syncProgress * 100))%")
                                    .foregroundStyle(.secondary)
                            }
                        }

                        // Last synced timestamp
                        if let lastSync = bleManager.lastSyncTime {
                            HStack {
                                Image(systemName: "checkmark.circle.fill")
                                    .foregroundStyle(.green)
                                Text("Last Synced")
                                Spacer()
                                Text(lastSync, style: .relative)
                                    .foregroundStyle(.secondary)
                                    .font(.caption)
                            }
                        }
                    }

                    // Device Commands
                    Section("Device Commands") {
                        Button {
                            bleManager.sendTareCommand()
                        } label: {
                            HStack {
                                Image(systemName: "arrow.counterclockwise")
                                    .foregroundStyle(.blue)
                                Text("Tare (Zero Scale)")
                            }
                        }

                        Button {
                            bleManager.sendResetDailyCommand()
                            PersistenceController.shared.deleteTodaysDrinkRecords()
                        } label: {
                            HStack {
                                Image(systemName: "arrow.uturn.backward")
                                    .foregroundStyle(.orange)
                                Text("Reset Daily Total")
                            }
                        }

                        Button {
                            bleManager.syncDeviceTime()
                        } label: {
                            HStack {
                                Image(systemName: "clock.arrow.circlepath")
                                    .foregroundStyle(.green)
                                Text("Sync Time")
                            }
                        }

                        if bleManager.unsyncedCount > 0 && !bleManager.syncState.isActive {
                            Button {
                                bleManager.startDrinkSync()
                            } label: {
                                HStack {
                                    Image(systemName: "arrow.triangle.2.circlepath")
                                        .foregroundStyle(.purple)
                                    Text("Sync Drink History")
                                    Spacer()
                                    Text("\(bleManager.unsyncedCount) records")
                                        .foregroundStyle(.secondary)
                                        .font(.caption)
                                }
                            }
                        }

                        Button(role: .destructive) {
                            bleManager.sendClearHistoryCommand()
                        } label: {
                            HStack {
                                Image(systemName: "trash")
                                Text("Clear Device History")
                            }
                        }
                    }

                    // Gestures
                    Section("Gestures") {
                        Toggle(isOn: Binding(
                            get: { bleManager.isShakeToEmptyEnabled },
                            set: { bleManager.setShakeToEmptyEnabled($0) }
                        )) {
                            VStack(alignment: .leading, spacing: 4) {
                                HStack {
                                    Image(systemName: "hand.wave.fill")
                                        .foregroundStyle(.orange)
                                    Text("Shake to Empty")
                                }
                                Text("Shake bottle while inverted to reset water level to zero")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }

                // Apple Health Integration
                Section("Apple Health") {
                    if healthKitManager.isHealthKitAvailable {
                        Toggle(isOn: $healthKitManager.isEnabled) {
                            HStack {
                                Image(systemName: "heart.fill")
                                    .foregroundStyle(.red)
                                Text("Sync to Health")
                            }
                        }
                        .onChange(of: healthKitManager.isEnabled) { _, enabled in
                            if enabled {
                                Task {
                                    do {
                                        try await healthKitManager.requestAuthorization()
                                    } catch {
                                        print("[HealthKit] Authorization error: \(error)")
                                        healthKitManager.isEnabled = false
                                    }
                                }
                            }
                        }

                        if healthKitManager.isEnabled {
                            HStack {
                                Image(systemName: healthKitManager.isAuthorized ? "checkmark.circle.fill" : "exclamationmark.circle")
                                    .foregroundStyle(healthKitManager.isAuthorized ? .green : .orange)
                                Text("Status")
                                Spacer()
                                Text(healthKitManager.isAuthorized ? "Connected" : "Not Authorized")
                                    .foregroundStyle(.secondary)
                                    .font(.subheadline)
                            }
                        }
                    } else {
                        HStack {
                            Image(systemName: "heart.slash")
                                .foregroundStyle(.secondary)
                            Text("HealthKit not available on this device")
                                .foregroundStyle(.secondary)
                        }
                    }
                }

                // Preferences
                Section("Preferences") {
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
        .environmentObject(HealthKitManager())
}
