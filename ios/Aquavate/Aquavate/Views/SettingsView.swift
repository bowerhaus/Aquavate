//
//  SettingsView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI

// MARK: - Settings Tab Categories

private enum SettingsTab: String, CaseIterable {
    case general = "General"
    case device = "Device"
    case more = "More"
}

// MARK: - Settings View

struct SettingsView: View {
    @EnvironmentObject var bleManager: BLEManager
    @EnvironmentObject var healthKitManager: HealthKitManager
    @EnvironmentObject var notificationManager: NotificationManager
    @EnvironmentObject var hydrationReminderService: HydrationReminderService

    // Tab selection
    @State private var selectedTab: SettingsTab = .general

    // Daily goal picker sheet state
    @State private var isGoalPickerPresented = false
    @State private var selectedGoalMl: Int = 2000

    // Clear history confirmation
    @State private var showClearHistoryAlert = false

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
                // MARK: - Category Picker
                Section {
                    Picker("Category", selection: $selectedTab) {
                        ForEach(SettingsTab.allCases, id: \.self) { tab in
                            Text(tab.rawValue).tag(tab)
                        }
                    }
                    .pickerStyle(.segmented)
                    .listRowBackground(Color.clear)
                    .listRowInsets(EdgeInsets(top: 8, leading: 0, bottom: 8, trailing: 0))
                }

                // MARK: - Tab Content
                switch selectedTab {
                case .general:
                    generalContent
                case .device:
                    deviceContent
                case .more:
                    moreContent
                }
            }
            .navigationTitle("Settings")
            .alert("Clear Device History", isPresented: $showClearHistoryAlert) {
                Button("Cancel", role: .cancel) { }
                Button("Clear", role: .destructive) {
                    bleManager.sendClearHistoryCommand()
                }
            } message: {
                Text("This will permanently delete all drink history stored on the device. This cannot be undone.")
            }
            .onAppear {
                bleManager.beginKeepAlive()
            }
            .onDisappear {
                bleManager.endKeepAlive()
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

    // MARK: - General Tab

    @ViewBuilder
    private var generalContent: some View {
        // Device Status
        Section("Device Status") {
            // Compact connection status + battery in one row
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

        // Your Goals
        Section("Your Goals") {
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
        }
    }

    // MARK: - Device Tab

    @ViewBuilder
    private var deviceContent: some View {
        // Device Setup
        Section {
            HStack {
                Image(systemName: bleManager.isCalibrated ? "checkmark.circle.fill" : "exclamationmark.circle")
                    .foregroundStyle(isConnected ? (bleManager.isCalibrated ? .green : .orange) : .secondary)
                Text("Calibrated")
                Spacer()
                Text(isConnected ? (bleManager.isCalibrated ? "Yes" : "No") : "—")
                    .foregroundStyle(.secondary)
            }
            .opacity(isConnected ? 1.0 : 0.5)

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
            .opacity(isConnected ? 1.0 : 0.5)
        } header: {
            Text("Device Setup")
        } footer: {
            if !isConnected {
                Text("Connect your bottle to calibrate and configure")
            }
        }

        // Device Controls
        Section {
            Button {
                bleManager.sendTareCommand()
            } label: {
                HStack {
                    Image(systemName: "arrow.counterclockwise")
                        .foregroundStyle(.blue)
                    Text("Tare (Zero Scale)")
                }
            }
            .disabled(!isConnected)

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
            .disabled(!isConnected)

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
                .disabled(!isConnected)
            }

            Button(role: .destructive) {
                showClearHistoryAlert = true
            } label: {
                HStack {
                    Image(systemName: "trash")
                    Text("Clear Device History")
                }
            }
            .disabled(!isConnected)

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
        } header: {
            Text("Device Controls")
        } footer: {
            if !isConnected {
                Text("Connect your bottle to use device controls")
            }
        }
    }

    // MARK: - More Tab

    @ViewBuilder
    private var moreContent: some View {
        // Reminder Options (conditional)
        if notificationManager.isEnabled {
            Section("Reminder Options") {
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

                Toggle(isOn: $hydrationReminderService.testModeEnabled) {
                    HStack {
                        Image(systemName: "timer")
                            .foregroundStyle(.orange)
                        Text("Earlier Reminders")
                    }
                }

                Toggle(isOn: $notificationManager.backOnTrackEnabled) {
                    HStack {
                        Image(systemName: "arrow.up.heart.fill")
                            .foregroundStyle(.green)
                        Text("Back On Track Alerts")
                    }
                }
            }
        }

        // Diagnostics
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
}

#Preview {
    SettingsView()
        .environmentObject(BLEManager())
        .environmentObject(HealthKitManager())
        .environmentObject(NotificationManager())
        .environmentObject(HydrationReminderService())
}
