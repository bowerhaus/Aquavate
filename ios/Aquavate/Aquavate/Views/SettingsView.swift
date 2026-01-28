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
    @EnvironmentObject var notificationManager: NotificationManager
    @EnvironmentObject var hydrationReminderService: HydrationReminderService

    // Daily goal picker sheet state
    @State private var isGoalPickerPresented = false
    @State private var selectedGoalMl: Int = 2000

    // Keep-alive timer to prevent bottle disconnect while on Settings
    @State private var keepAliveTimer: Timer?

    // DisclosureGroup expansion state
    @State private var isDeviceInfoExpanded = false
    @State private var isCommandsExpanded = false
    @State private var isDangerZoneExpanded = false
    @State private var isReminderOptionsExpanded = false
    @State private var isBottleDetailsExpanded = false

    // Goal options: 1000ml to 4000ml in 250ml steps
    private let goalOptions = stride(from: 1000, through: 4000, by: 250).map { $0 }

    private var isConnected: Bool {
        bleManager.connectionState.isConnected
    }

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
        if let deviceName = bleManager.connectedDeviceName, isConnected {
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
                // MARK: - Device
                Section("Device") {
                    // Compact connection status + battery
                    HStack {
                        Image(systemName: "bluetooth")
                            .foregroundStyle(.blue)
                        if bleManager.connectionState == .scanning || bleManager.connectionState == .connecting {
                            ProgressView()
                                .scaleEffect(0.7)
                        } else {
                            Circle()
                                .fill(connectionStatusColor)
                                .frame(width: 8, height: 8)
                        }
                        Text(connectionStatusText)
                            .font(.subheadline)
                        Spacer()
                        if isConnected {
                            HStack(spacing: 4) {
                                Image(systemName: batteryIconName)
                                    .foregroundStyle(batteryColor)
                                Text("\(bleManager.batteryPercent)%")
                                    .font(.subheadline)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }

                    // Disconnected banner
                    if !isConnected && bleManager.connectionState == .disconnected {
                        HStack {
                            Image(systemName: "info.circle")
                                .foregroundStyle(.blue)
                            Text("Pull to refresh on Home to connect your bottle")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                    }

                    // Error message
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

                // MARK: - Goals & Tracking
                Section("Goals & Tracking") {
                    // Daily Goal
                    Button {
                        selectedGoalMl = bleManager.dailyGoalMl
                        isGoalPickerPresented = true
                    } label: {
                        HStack {
                            Image(systemName: "target")
                                .foregroundStyle(.blue)
                            Text("Daily Goal")
                                .foregroundStyle(.primary)
                            Spacer()
                            Text("\(bleManager.dailyGoalMl)ml")
                                .foregroundStyle(.secondary)
                            if isConnected {
                                Image(systemName: "chevron.right")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                    .disabled(!isConnected)

                    // Calibrate Bottle - always visible
                    NavigationLink {
                        CalibrationViewWrapper()
                    } label: {
                        HStack {
                            Image(systemName: bleManager.isCalibrated ? "checkmark.seal.fill" : "seal")
                                .foregroundStyle(isConnected ? (bleManager.isCalibrated ? .green : .orange) : .secondary)
                            Text("Calibrate Bottle")
                            Spacer()
                            if !isConnected {
                                Text("Requires connection")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            } else if !bleManager.isCalibrated {
                                Text("Required")
                                    .font(.caption)
                                    .foregroundStyle(.orange)
                            }
                        }
                    }
                    .disabled(!isConnected)

                    // Bottle details
                    DisclosureGroup(isExpanded: $isBottleDetailsExpanded) {
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
                    } label: {
                        HStack {
                            Image(systemName: "waterbottle")
                                .foregroundStyle(.cyan)
                            Text("Bottle Details")
                        }
                    }
                }

                // MARK: - Notifications
                Section("Notifications") {
                    // Hydration Reminders toggle
                    Toggle(isOn: $notificationManager.isEnabled) {
                        HStack {
                            Image(systemName: "bell.fill")
                                .foregroundStyle(.purple)
                            Text("Hydration Reminders")
                        }
                    }
                    .onChange(of: notificationManager.isEnabled) { _, enabled in
                        if enabled {
                            Task { try? await notificationManager.requestAuthorization() }
                        }
                    }

                    // Apple Health toggle
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
                    } else {
                        HStack {
                            Image(systemName: "heart.slash")
                                .foregroundStyle(.secondary)
                            Text("HealthKit not available on this device")
                                .foregroundStyle(.secondary)
                        }
                    }

                    // Reminder Options - collapsed by default
                    if notificationManager.isEnabled {
                        DisclosureGroup(isExpanded: $isReminderOptionsExpanded) {
                            HStack {
                                Image(systemName: notificationManager.isAuthorized ? "checkmark.circle.fill" : "exclamationmark.circle")
                                    .foregroundStyle(notificationManager.isAuthorized ? .green : .orange)
                                Text("Status")
                                Spacer()
                                Text(notificationManager.isAuthorized ? "Authorized" : "Not Authorized")
                                    .foregroundStyle(.secondary)
                                    .font(.subheadline)
                            }

                            HStack {
                                Image(systemName: "drop.fill")
                                    .foregroundStyle(hydrationReminderService.currentUrgency.color)
                                Text("Current Status")
                                Spacer()
                                Text(hydrationReminderService.currentUrgency.description)
                                    .foregroundStyle(.secondary)
                                    .font(.subheadline)
                            }

                            HStack {
                                Image(systemName: "bell.badge")
                                    .foregroundStyle(.secondary)
                                Text("Reminders Today")
                                Spacer()
                                if notificationManager.dailyLimitEnabled {
                                    Text("\(notificationManager.remindersSentToday)/\(NotificationManager.maxRemindersPerDay)")
                                        .foregroundStyle(.secondary)
                                        .font(.subheadline)
                                } else {
                                    Text("\(notificationManager.remindersSentToday)")
                                        .foregroundStyle(.secondary)
                                        .font(.subheadline)
                                }
                            }

                            Toggle(isOn: $notificationManager.dailyLimitEnabled) {
                                HStack {
                                    Image(systemName: "number.circle")
                                        .foregroundStyle(.blue)
                                    Text("Limit Daily Reminders")
                                }
                            }

                            Toggle(isOn: $notificationManager.backOnTrackEnabled) {
                                HStack {
                                    Image(systemName: "arrow.up.heart.fill")
                                        .foregroundStyle(.green)
                                    Text("Back On Track Alerts")
                                }
                            }
                        } label: {
                            HStack {
                                Image(systemName: "slider.horizontal.3")
                                    .foregroundStyle(.purple)
                                Text("Reminder Options")
                            }
                        }
                    }

                    // Health status
                    if healthKitManager.isEnabled && healthKitManager.isHealthKitAvailable {
                        HStack {
                            Image(systemName: healthKitManager.isAuthorized ? "checkmark.circle.fill" : "exclamationmark.circle")
                                .foregroundStyle(healthKitManager.isAuthorized ? .green : .orange)
                            Text("Health Status")
                            Spacer()
                            Text(healthKitManager.isAuthorized ? "Connected" : "Not Authorized")
                                .foregroundStyle(.secondary)
                                .font(.subheadline)
                        }
                    }
                }

                // MARK: - Device Settings
                Section {
                    // Shake to Empty toggle
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
                    .disabled(!isConnected)

                    // Device Info - collapsed by default
                    DisclosureGroup(isExpanded: $isDeviceInfoExpanded) {
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
                    } label: {
                        HStack {
                            Image(systemName: "info.circle")
                                .foregroundStyle(.blue)
                            Text("Device Info")
                        }
                    }

                    // Commands - collapsed by default
                    DisclosureGroup(isExpanded: $isCommandsExpanded) {
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
                    } label: {
                        HStack {
                            Image(systemName: "gearshape")
                                .foregroundStyle(.gray)
                            Text("Commands")
                        }
                    }

                    // Danger Zone - collapsed by default
                    DisclosureGroup(isExpanded: $isDangerZoneExpanded) {
                        Button(role: .destructive) {
                            bleManager.sendClearHistoryCommand()
                        } label: {
                            HStack {
                                Image(systemName: "trash")
                                Text("Clear Device History")
                            }
                        }
                    } label: {
                        HStack {
                            Image(systemName: "exclamationmark.triangle")
                                .foregroundStyle(.red)
                            Text("Danger Zone")
                        }
                    }
                } header: {
                    Text("Device Settings")
                } footer: {
                    if !isConnected {
                        Text("Connect your bottle to access device settings")
                    }
                }
                .disabled(!isConnected)
                .opacity(isConnected ? 1.0 : 0.5)

                // MARK: - Diagnostics
                Section("Diagnostics") {
                    NavigationLink {
                        ActivityStatsView()
                    } label: {
                        HStack {
                            Image(systemName: "moon.zzz")
                                .foregroundStyle(.purple)
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Sleep Mode Analysis")
                                Text("View wake events and backpack sessions")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                            Spacer()
                            if bleManager.activityFetchState.isLoading {
                                ProgressView()
                                    .scaleEffect(0.7)
                            }
                        }
                    }
                }

                // MARK: - About
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

                // MARK: - Debug
                #if DEBUG
                Section("Debug") {
                    // Connection controls
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

                    // Show discovered devices if scanning
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

                    // Hydration reminder test mode
                    Toggle(isOn: $hydrationReminderService.testModeEnabled) {
                        HStack {
                            Image(systemName: "timer")
                                .foregroundStyle(.orange)
                            Text("Test Mode (Earlier Reminders)")
                        }
                    }
                }
                #endif
            }
            .navigationTitle("Settings")
            .onAppear {
                // Keep bottle connected while on Settings
                bleManager.cancelIdleDisconnectTimer()
                if isConnected {
                    bleManager.sendPingCommand()
                }
                // Periodic ping every 30s to prevent firmware sleep
                keepAliveTimer = Timer.scheduledTimer(withTimeInterval: 30.0, repeats: true) { _ in
                    if bleManager.connectionState.isConnected {
                        bleManager.sendPingCommand()
                    }
                }
            }
            .onDisappear {
                // Resume normal idle disconnect behavior
                keepAliveTimer?.invalidate()
                keepAliveTimer = nil
                if isConnected {
                    bleManager.startIdleDisconnectTimer()
                }
            }
            .sheet(isPresented: $isGoalPickerPresented) {
                NavigationView {
                    VStack(spacing: 20) {
                        Text("Select your daily hydration target")
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                            .padding(.top)

                        Picker("Daily Goal", selection: $selectedGoalMl) {
                            ForEach(goalOptions, id: \.self) { goal in
                                Text("\(goal)ml").tag(goal)
                            }
                        }
                        .pickerStyle(.wheel)
                        .frame(height: 150)

                        Spacer()
                    }
                    .navigationTitle("Daily Goal")
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar {
                        ToolbarItem(placement: .cancellationAction) {
                            Button("Cancel") {
                                isGoalPickerPresented = false
                            }
                        }
                        ToolbarItem(placement: .confirmationAction) {
                            Button("Save") {
                                bleManager.setDailyGoal(selectedGoalMl)
                                isGoalPickerPresented = false
                            }
                        }
                    }
                }
                .presentationDetents([.medium])
            }
        }
    }
}

#Preview {
    SettingsView()
        .environmentObject(BLEManager())
        .environmentObject(HealthKitManager())
        .environmentObject(NotificationManager())
        .environmentObject(HydrationReminderService())
}
