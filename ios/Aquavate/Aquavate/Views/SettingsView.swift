//
//  SettingsView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI
import UniformTypeIdentifiers

// MARK: - Settings View (Option 5: Smart Contextual)

struct SettingsView: View {
    @EnvironmentObject var bleManager: BLEManager
    @EnvironmentObject var healthKitManager: HealthKitManager
    @EnvironmentObject var notificationManager: NotificationManager
    @EnvironmentObject var hydrationReminderService: HydrationReminderService

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
        if percent > 75 { return "battery.100" }
        else if percent > 50 { return "battery.75" }
        else if percent > 25 { return "battery.50" }
        else if percent > 10 { return "battery.25" }
        else { return "battery.0" }
    }

    private var batteryColor: Color {
        let percent = bleManager.batteryPercent
        if percent > 20 { return .green }
        else if percent > 10 { return .orange }
        else { return .red }
    }

    var body: some View {
        NavigationView {
            List {
                // MARK: - Device Status (inline, not a sub-page)
                Section("Device Status") {
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

                    if !isConnected && bleManager.connectionState == .disconnected {
                        HStack {
                            Image(systemName: "info.circle")
                                .foregroundStyle(.blue)
                            Text("Some options require connection to the bottle to be displayed. Pull to refresh on Home to connect.")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                    }


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

                // MARK: - Category Navigation
                Section {
                    // Goals & Health
                    NavigationLink {
                        GoalsSettingsPage(
                            isGoalPickerPresented: $isGoalPickerPresented,
                            selectedGoalMl: $selectedGoalMl
                        )
                    } label: {
                        HStack {
                            Image(systemName: "target")
                                .foregroundStyle(.blue)
                                .frame(width: 28)
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Goals & Health")
                                Text(goalsSummary)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                                    .lineLimit(1)
                            }
                        }
                    }

                    // Device Information
                    NavigationLink {
                        DeviceInformationPage()
                    } label: {
                        HStack {
                            Image(systemName: "gearshape")
                                .foregroundStyle(.gray)
                                .frame(width: 28)
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Device Information")
                                Text(deviceInfoSummary)
                                    .font(.caption)
                                    .foregroundStyle(deviceInfoSummaryColor)
                                    .lineLimit(1)
                            }
                        }
                    }

                    // Device Controls
                    NavigationLink {
                        DeviceControlsPage(showClearHistoryAlert: $showClearHistoryAlert)
                    } label: {
                        HStack {
                            Image(systemName: "slider.horizontal.3")
                                .foregroundStyle(.purple)
                                .frame(width: 28)
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Device Controls")
                                Text(deviceControlsSummary)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                                    .lineLimit(1)
                            }
                        }
                    }

                    // Reminder Options (only when reminders enabled)
                    if notificationManager.isEnabled {
                        NavigationLink {
                            ReminderOptionsPage()
                        } label: {
                            HStack {
                                Image(systemName: "bell.fill")
                                    .foregroundStyle(.purple)
                                    .frame(width: 28)
                                VStack(alignment: .leading, spacing: 2) {
                                    Text("Reminder Options")
                                    Text(reminderSummary)
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                        .lineLimit(1)
                                }
                            }
                        }
                    }

                    // Data (Export & Import)
                    NavigationLink {
                        DataSettingsPage()
                    } label: {
                        HStack {
                            Image(systemName: "externaldrive.fill")
                                .foregroundStyle(.green)
                                .frame(width: 28)
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Data")
                                Text(dataSummary)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                                    .lineLimit(1)
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

    // MARK: - Contextual Summaries

    private var goalsSummary: String {
        var parts: [String] = ["\(bleManager.dailyGoalMl)ml goal"]
        if notificationManager.isEnabled {
            parts.append("Reminders on")
        }
        if healthKitManager.isEnabled {
            parts.append("Health sync on")
        }
        return parts.joined(separator: " · ")
    }

    private var deviceInfoSummary: String {
        if !isConnected {
            return "Calibration, sleep analysis"
        }
        return bleManager.isCalibrated ? "Calibrated" : "Not calibrated"
    }

    private var deviceInfoSummaryColor: Color {
        if !isConnected { return .secondary }
        return bleManager.isCalibrated ? .secondary : .orange
    }

    private var deviceControlsSummary: String {
        if !isConnected {
            return "Requires connection"
        }
        if bleManager.unsyncedCount > 0 {
            return "\(bleManager.unsyncedCount) unsynced records"
        }
        return "Tare, reset, sync, gestures"
    }

    private var reminderSummary: String {
        if notificationManager.dailyLimitEnabled {
            return "\(notificationManager.remindersSentToday)/\(NotificationManager.maxRemindersPerDay) sent today"
        }
        return "\(notificationManager.remindersSentToday) sent today"
    }

    private var dataSummary: String {
        let count = (try? PersistenceController.shared.viewContext.count(for: CDDrinkRecord.fetchRequest())) ?? 0
        return "\(count) drink records"
    }
}

// MARK: - Goals & Health Sub-Page

private struct GoalsSettingsPage: View {
    @EnvironmentObject var bleManager: BLEManager
    @EnvironmentObject var healthKitManager: HealthKitManager
    @EnvironmentObject var notificationManager: NotificationManager

    @Binding var isGoalPickerPresented: Bool
    @Binding var selectedGoalMl: Int

    private var isConnected: Bool {
        bleManager.connectionState.isConnected
    }

    var body: some View {
        List {
            Section("Daily Goal") {
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
            }

            Section("Notifications") {
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

                HStack {
                    Image(systemName: notificationManager.isAuthorized ? "checkmark.circle.fill" : "xmark.circle")
                        .foregroundStyle(notificationManager.isAuthorized ? .green : .orange)
                    Text("Notification Status")
                    Spacer()
                    Text(notificationManager.isAuthorized ? "Authorized" : "Not Authorized")
                        .foregroundStyle(.secondary)
                        .font(.subheadline)
                }
            }

            Section("Health") {
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

                    HStack {
                        Image(systemName: healthKitManager.isAuthorized ? "checkmark.circle.fill" : "xmark.circle")
                            .foregroundStyle(healthKitManager.isAuthorized ? .green : .orange)
                        Text("Health Status")
                        Spacer()
                        Text(healthKitManager.isAuthorized ? "Authorized" : "Not Authorized")
                            .foregroundStyle(.secondary)
                            .font(.subheadline)
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
        .navigationTitle("Goals & Health")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Device Information Sub-Page

private struct DeviceInformationPage: View {
    @EnvironmentObject var bleManager: BLEManager

    private var isConnected: Bool {
        bleManager.connectionState.isConnected
    }

    var body: some View {
        List {
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
                Text("Calibration")
            } footer: {
                if !isConnected {
                    Text("Connect your bottle to calibrate and configure")
                }
            }

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
        }
        .navigationTitle("Device Information")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Device Controls Sub-Page

private struct DeviceControlsPage: View {
    @EnvironmentObject var bleManager: BLEManager

    @Binding var showClearHistoryAlert: Bool

    private var isConnected: Bool {
        bleManager.connectionState.isConnected
    }

    var body: some View {
        List {
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
            } header: {
                Text("Commands")
            } footer: {
                if !isConnected {
                    Text("Connect your bottle to use device controls")
                }
            }

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
                .disabled(!isConnected)
            }
        }
        .navigationTitle("Device Controls")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Reminder Options Sub-Page

private struct ReminderOptionsPage: View {
    @EnvironmentObject var notificationManager: NotificationManager
    @EnvironmentObject var hydrationReminderService: HydrationReminderService

    var body: some View {
        List {
            Section {
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
        .navigationTitle("Reminder Options")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Data Sub-Page

private struct DataSettingsPage: View {
    @StateObject private var backupManager = BackupManager()

    // Import flow state
    @State private var showFileImporter = false
    @State private var pendingBackup: AquavateBackup?
    @State private var backupSummary: BackupSummary?
    @State private var showImportPreview = false

    // Export flow state
    @State private var exportFileURL: URL?
    @State private var showShareSheet = false

    // Alerts
    @State private var showSuccessAlert = false
    @State private var showErrorAlert = false
    @State private var errorMessage = ""
    @State private var successMessage = ""
    @State private var showReplaceConfirmation = false

    // Data counts
    @State private var bottleCount = 0
    @State private var drinkCount = 0
    @State private var motionEventCount = 0
    @State private var backpackSessionCount = 0

    var body: some View {
        List {
            // MARK: Current Data Summary
            Section("Your Data") {
                dataCountRow(icon: "drop.fill", color: .blue, label: "Bottles", count: bottleCount)
                dataCountRow(icon: "cup.and.saucer.fill", color: .cyan, label: "Drink Records", count: drinkCount)
                dataCountRow(icon: "moon.zzz", color: .purple, label: "Wake Events", count: motionEventCount)
                dataCountRow(icon: "backpack.fill", color: .orange, label: "Backpack Sessions", count: backpackSessionCount)
            }

            // MARK: Export
            Section {
                Button {
                    exportBackup()
                } label: {
                    HStack {
                        Image(systemName: "square.and.arrow.up")
                            .foregroundStyle(.blue)
                        Text("Export Backup")
                        Spacer()
                        if backupManager.isExporting {
                            ProgressView()
                                .scaleEffect(0.7)
                        }
                    }
                }
                .disabled(backupManager.isExporting)
            } header: {
                Text("Export")
            } footer: {
                Text("Creates a JSON file containing all your data. Share via AirDrop, Files, email, or any other app.")
            }

            // MARK: Import
            Section {
                Button {
                    showFileImporter = true
                } label: {
                    HStack {
                        Image(systemName: "square.and.arrow.down")
                            .foregroundStyle(.green)
                        Text("Import Backup")
                        Spacer()
                        if backupManager.isImporting {
                            ProgressView()
                                .scaleEffect(0.7)
                        }
                    }
                }
                .disabled(backupManager.isImporting)
            } header: {
                Text("Import")
            } footer: {
                Text("Restore from a previously exported Aquavate backup file.")
            }
        }
        .navigationTitle("Data")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear { loadDataCounts() }
        .fileImporter(
            isPresented: $showFileImporter,
            allowedContentTypes: [.json],
            allowsMultipleSelection: false
        ) { result in
            handleFileImport(result)
        }
        .sheet(isPresented: $showImportPreview) {
            if let summary = backupSummary {
                ImportPreviewSheet(
                    summary: summary,
                    onMerge: { performImport(mode: .merge) },
                    onReplace: { showReplaceConfirmation = true },
                    onCancel: {
                        showImportPreview = false
                        pendingBackup = nil
                        backupSummary = nil
                    }
                )
            }
        }
        .sheet(isPresented: $showShareSheet) {
            if let url = exportFileURL {
                ShareSheet(activityItems: [url])
            }
        }
        .alert("Replace All Data?", isPresented: $showReplaceConfirmation) {
            Button("Cancel", role: .cancel) { }
            Button("Replace", role: .destructive) {
                showImportPreview = false
                performImport(mode: .replace)
            }
        } message: {
            Text("This will permanently delete all existing data and replace it with the backup contents. This cannot be undone.")
        }
        .alert("Import Complete", isPresented: $showSuccessAlert) {
            Button("OK") { }
        } message: {
            Text(successMessage)
        }
        .alert("Error", isPresented: $showErrorAlert) {
            Button("OK") { }
        } message: {
            Text(errorMessage)
        }
    }

    // MARK: - Helper Views

    private func dataCountRow(icon: String, color: Color, label: String, count: Int) -> some View {
        HStack {
            Image(systemName: icon)
                .foregroundStyle(color)
                .frame(width: 28)
            Text(label)
            Spacer()
            Text("\(count)")
                .foregroundStyle(.secondary)
        }
    }

    // MARK: - Actions

    private func loadDataCounts() {
        let context = PersistenceController.shared.viewContext
        bottleCount = (try? context.count(for: CDBottle.fetchRequest())) ?? 0
        drinkCount = (try? context.count(for: CDDrinkRecord.fetchRequest())) ?? 0
        motionEventCount = (try? context.count(for: CDMotionWakeEvent.fetchRequest())) ?? 0
        backpackSessionCount = (try? context.count(for: CDBackpackSession.fetchRequest())) ?? 0
    }

    private func exportBackup() {
        Task {
            do {
                let (data, filename) = try await backupManager.createBackup()
                let tempDir = FileManager.default.temporaryDirectory
                let fileURL = tempDir.appendingPathComponent(filename)
                try data.write(to: fileURL)
                exportFileURL = fileURL
                showShareSheet = true
            } catch {
                errorMessage = error.localizedDescription
                showErrorAlert = true
            }
        }
    }

    private func handleFileImport(_ result: Result<[URL], Error>) {
        switch result {
        case .success(let urls):
            guard let url = urls.first else { return }

            guard url.startAccessingSecurityScopedResource() else {
                errorMessage = "Cannot access the selected file."
                showErrorAlert = true
                return
            }
            defer { url.stopAccessingSecurityScopedResource() }

            do {
                let data = try Data(contentsOf: url)
                let (backup, summary) = try backupManager.parseBackup(from: data)
                pendingBackup = backup
                backupSummary = summary
                showImportPreview = true
            } catch let error as BackupError {
                errorMessage = error.localizedDescription
                showErrorAlert = true
            } catch {
                errorMessage = "Failed to read backup file: \(error.localizedDescription)"
                showErrorAlert = true
            }

        case .failure(let error):
            errorMessage = "File selection failed: \(error.localizedDescription)"
            showErrorAlert = true
        }
    }

    private func performImport(mode: ImportMode) {
        guard let backup = pendingBackup else { return }

        Task {
            do {
                let result = try await backupManager.importBackup(backup, mode: mode)

                let modeText = mode == .merge ? "merged" : "replaced"
                var parts: [String] = []
                if result.bottlesImported > 0 { parts.append("\(result.bottlesImported) bottle(s)") }
                if result.drinksImported > 0 { parts.append("\(result.drinksImported) drink(s)") }
                if result.motionEventsImported > 0 { parts.append("\(result.motionEventsImported) wake event(s)") }
                if result.backpackSessionsImported > 0 { parts.append("\(result.backpackSessionsImported) session(s)") }

                if parts.isEmpty {
                    successMessage = "Import complete. No new data to import (all records already exist)."
                } else {
                    successMessage = "Successfully \(modeText): \(parts.joined(separator: ", "))."
                }
                if result.duplicatesSkipped > 0 {
                    successMessage += " \(result.duplicatesSkipped) duplicate(s) skipped."
                }

                showImportPreview = false
                pendingBackup = nil
                backupSummary = nil
                showSuccessAlert = true
                loadDataCounts()
            } catch {
                showImportPreview = false
                errorMessage = error.localizedDescription
                showErrorAlert = true
            }
        }
    }
}

// MARK: - Import Preview Sheet

private struct ImportPreviewSheet: View {
    let summary: BackupSummary
    let onMerge: () -> Void
    let onReplace: () -> Void
    let onCancel: () -> Void

    var body: some View {
        NavigationView {
            List {
                Section("Backup Info") {
                    HStack {
                        Text("Export Date")
                        Spacer()
                        Text(summary.exportDate, style: .date)
                            .foregroundStyle(.secondary)
                    }
                    HStack {
                        Text("App Version")
                        Spacer()
                        Text(summary.appVersion)
                            .foregroundStyle(.secondary)
                    }
                    HStack {
                        Text("Format Version")
                        Spacer()
                        Text("v\(summary.formatVersion)")
                            .foregroundStyle(.secondary)
                    }
                }

                Section("Contents") {
                    HStack {
                        Image(systemName: "drop.fill").foregroundStyle(.blue)
                        Text("Bottles")
                        Spacer()
                        Text("\(summary.bottleCount)")
                            .foregroundStyle(.secondary)
                    }
                    HStack {
                        Image(systemName: "cup.and.saucer.fill").foregroundStyle(.cyan)
                        Text("Drink Records")
                        Spacer()
                        Text("\(summary.drinkRecordCount)")
                            .foregroundStyle(.secondary)
                    }
                    HStack {
                        Image(systemName: "moon.zzz").foregroundStyle(.purple)
                        Text("Wake Events")
                        Spacer()
                        Text("\(summary.motionWakeEventCount)")
                            .foregroundStyle(.secondary)
                    }
                    HStack {
                        Image(systemName: "backpack.fill").foregroundStyle(.orange)
                        Text("Backpack Sessions")
                        Spacer()
                        Text("\(summary.backpackSessionCount)")
                            .foregroundStyle(.secondary)
                    }
                    if summary.hasSettings {
                        HStack {
                            Image(systemName: "gearshape.fill").foregroundStyle(.gray)
                            Text("App Settings")
                            Spacer()
                            Text("Included")
                                .foregroundStyle(.secondary)
                        }
                    }
                }

                Section("Import Mode") {
                    Button {
                        onMerge()
                    } label: {
                        HStack {
                            Image(systemName: "plus.circle.fill")
                                .foregroundStyle(.green)
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Merge")
                                    .foregroundStyle(.primary)
                                Text("Add new records, skip duplicates")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }

                    Button(role: .destructive) {
                        onReplace()
                    } label: {
                        HStack {
                            Image(systemName: "arrow.triangle.2.circlepath")
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Replace All")
                                Text("Delete existing data, import everything")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }
            }
            .navigationTitle("Import Preview")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { onCancel() }
                }
            }
        }
        .presentationDetents([.large])
    }
}

// MARK: - Share Sheet

private struct ShareSheet: UIViewControllerRepresentable {
    let activityItems: [Any]

    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: activityItems, applicationActivities: nil)
    }

    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}
}

#Preview {
    SettingsView()
        .environmentObject(BLEManager())
        .environmentObject(HealthKitManager())
        .environmentObject(NotificationManager())
        .environmentObject(HydrationReminderService())
}
