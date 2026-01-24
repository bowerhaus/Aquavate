//
//  HomeView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI
import CoreData

struct HomeView: View {
    @EnvironmentObject var bleManager: BLEManager
    @EnvironmentObject var healthKitManager: HealthKitManager
    @EnvironmentObject var hydrationReminderService: HydrationReminderService
    @Environment(\.managedObjectContext) private var viewContext

    // Alert state for pull-to-refresh and delete
    @State private var showBottleAsleepAlert = false
    @State private var showErrorAlert = false
    @State private var errorAlertMessage = ""
    @State private var showConnectionRequiredAlert = false
    @State private var isDeleting = false

    // Fetch all drinks from CoreData, sorted by most recent first
    // Filter to today's drinks in computed property to ensure dynamic date comparison
    @FetchRequest(
        sortDescriptors: [NSSortDescriptor(keyPath: \CDDrinkRecord.timestamp, ascending: false)],
        animation: .default
    )
    private var allDrinksCD: FetchedResults<CDDrinkRecord>

    // Filter to today's drinks dynamically (not at compile time)
    // Uses 4am boundary to match firmware DRINK_DAILY_RESET_HOUR
    private var todaysDrinksCD: [CDDrinkRecord] {
        let startOfDay = Calendar.current.startOfAquavateDay(for: Date())
        return allDrinksCD.filter { ($0.timestamp ?? .distantPast) >= startOfDay }
    }

    // Convert CoreData records to display model
    private var todaysDrinks: [DrinkRecord] {
        todaysDrinksCD.map { $0.toDrinkRecord() }
    }

    // Always show the sum of drinks in CoreData so the total matches the displayed drink list
    private var displayDailyTotal: Int {
        return todaysDrinks.reduce(0) { $0 + $1.amountMl }
    }

    // Always use last known BLE values - they persist after disconnect
    private var displayBottleLevel: Int {
        return bleManager.bottleLevelMl
    }

    private var displayCapacity: Int {
        return bleManager.bottleCapacityMl
    }

    private var displayGoal: Int {
        return bleManager.dailyGoalMl
    }

    private var dailyProgress: Double {
        guard displayGoal > 0 else { return 0 }
        return min(1.0, Double(displayDailyTotal) / Double(displayGoal))
    }

    private var bottleProgress: Double {
        guard displayCapacity > 0 else { return 0 }
        return min(1.0, Double(displayBottleLevel) / Double(displayCapacity))
    }

    // Delete today's drinks at given offsets - requires bottle connection for bidirectional sync
    private func deleteTodaysDrinks(at offsets: IndexSet) {
        // Require bottle connection for deletion
        guard bleManager.connectionState.isConnected else {
            showConnectionRequiredAlert = true
            return
        }

        // Prevent multiple simultaneous deletes
        guard !isDeleting else { return }
        isDeleting = true

        // Process deletes sequentially
        Task { @MainActor in
            for index in offsets {
                let cdRecord = todaysDrinksCD[index]
                guard let id = cdRecord.id else { continue }

                let firmwareId = cdRecord.firmwareRecordId
                let healthKitUUID = cdRecord.healthKitSampleUUID

                // If we have a firmware ID, use pessimistic delete
                if firmwareId != 0 {
                    await withCheckedContinuation { continuation in
                        bleManager.deleteDrinkRecord(firmwareRecordId: UInt32(firmwareId)) { success in
                            if success {
                                // Delete from HealthKit first (if synced)
                                Task { @MainActor in
                                    if let hkUUID = healthKitUUID,
                                       self.healthKitManager.isEnabled && self.healthKitManager.isAuthorized {
                                        do {
                                            try await self.healthKitManager.deleteWaterSample(uuid: hkUUID)
                                        } catch {
                                            // Log error but continue with CoreData delete
                                            print("[HealthKit] Delete error: \(error)")
                                        }
                                    }
                                    // Bottle confirmed - now safe to delete locally
                                    PersistenceController.shared.deleteDrinkRecord(id: id)
                                }
                            }
                            // If failed, leave record in place - user can retry
                            continuation.resume()
                        }
                    }
                } else {
                    // No firmware ID (old record) - delete locally only
                    // Still delete from HealthKit if synced
                    if let hkUUID = healthKitUUID,
                       healthKitManager.isEnabled && healthKitManager.isAuthorized {
                        do {
                            try await healthKitManager.deleteWaterSample(uuid: hkUUID)
                        } catch {
                            print("[HealthKit] Delete error: \(error)")
                        }
                    }
                    PersistenceController.shared.deleteDrinkRecord(id: id)
                }
            }
            isDeleting = false
        }
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
        VStack(spacing: 0) {
            // Fixed header bar
            HStack(spacing: 6) {
                Circle()
                    .fill(connectionStatusColor)
                    .frame(width: 8, height: 8)
                Text(connectionStatusText)
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Spacer()

                // Battery indicator (when connected)
                if bleManager.connectionState.isConnected {
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
            .padding(.vertical, 10)
            .background(Color(.systemBackground))

            Divider()

            // Fixed progress section
            VStack(spacing: 16) {
                // Status warnings (when connected)
                if bleManager.connectionState.isConnected {
                    HStack(spacing: 12) {
                        // Only show calibration warning when NOT calibrated
                        if !bleManager.isCalibrated {
                            StatusBadge(
                                label: "Not Calibrated",
                                isActive: true,
                                activeColor: .orange,
                                inactiveColor: .gray
                            )
                        }
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

                // Human figure progress - daily goal (PRIMARY FOCUS)
                HumanFigureProgressView(
                    current: displayDailyTotal,
                    total: displayGoal,
                    color: .blue,
                    label: "of \(displayGoal)ml goal",
                    expectedCurrent: hydrationReminderService.calculateExpectedIntake(),
                    deficitMl: hydrationReminderService.deficitMl,
                    urgency: hydrationReminderService.currentUrgency
                )

                // Bottle level progress bar (SECONDARY)
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text("Bottle Level")
                            .font(.headline)
                        Spacer()
                        Text("\(Int(bottleProgress * 100))%")
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                    }

                    ProgressView(value: bottleProgress)
                        .tint(.blue)

                    HStack {
                        Text("\(displayBottleLevel)ml")
                            .font(.subheadline)
                            .fontWeight(.medium)
                        Text("/ \(displayCapacity)ml")
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                    }
                }
                .padding(.horizontal)
            }
            .padding(.vertical, 8)
            .background(Color(.systemGroupedBackground))

            // Today's drinks list
            List {
                if todaysDrinksCD.isEmpty {
                    Section {
                        VStack(spacing: 16) {
                            Image(systemName: "drop.slash")
                                .font(.system(size: 48))
                                .foregroundStyle(.secondary)
                            Text("No drinks recorded today")
                                .font(.title3)
                                .foregroundStyle(.secondary)
                            Text("Pull down to sync with your bottle")
                                .font(.caption)
                                .foregroundStyle(.tertiary)
                        }
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 40)
                    }
                } else {
                    Section {
                        ForEach(todaysDrinksCD) { cdRecord in
                            DrinkListItem(drink: cdRecord.toDrinkRecord())
                                .frame(minHeight: 44)
                        }
                        .onDelete(perform: deleteTodaysDrinks)
                    } header: {
                        Text("Today's Drinks")
                    } footer: {
                        HStack {
                            Text("Total consumed:")
                            Spacer()
                            Text("\(displayDailyTotal)ml")
                                .fontWeight(.semibold)
                        }
                        .padding(.top, 8)
                    }
                }
            }
            .refreshable {
                await handleRefresh()
            }
        }
        .alert("Bottle is Asleep", isPresented: $showBottleAsleepAlert) {
            Button("OK", role: .cancel) { }
        } message: {
            Text("Tilt your bottle to wake it up, then pull down to try again.")
        }
        .alert("Sync Error", isPresented: $showErrorAlert) {
            Button("OK", role: .cancel) { }
        } message: {
            Text(errorAlertMessage)
        }
        .alert("Connection Required", isPresented: $showConnectionRequiredAlert) {
            Button("OK", role: .cancel) { }
        } message: {
            Text("Please connect to your bottle before deleting drinks. This ensures both the app and bottle stay in sync.")
        }
    }

    // MARK: - Pull-to-Refresh

    private func handleRefresh() async {
        let result = await bleManager.performRefresh()

        switch result {
        case .synced(let count):
            // Success - records synced and disconnected
            print("Synced \(count) records")
        case .alreadySynced:
            // Success - no new records, disconnected
            print("Already synced")
        case .bottleAsleep:
            showBottleAsleepAlert = true
        case .bluetoothOff:
            errorAlertMessage = "Bluetooth is turned off. Please enable Bluetooth in Settings."
            showErrorAlert = true
        case .connectionFailed(let message):
            errorAlertMessage = "Connection failed: \(message)"
            showErrorAlert = true
        case .syncFailed(let message):
            errorAlertMessage = "Sync failed: \(message)"
            showErrorAlert = true
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
        .environmentObject(HealthKitManager())
        .environmentObject(HydrationReminderService())
}
