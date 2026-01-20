//
//  DebugView.swift
//  Aquavate
//
//  Debug/test helper view for hardware testing.
//  Shows raw BLE data and provides diagnostic tools.
//

import SwiftUI

struct DebugView: View {
    @EnvironmentObject var bleManager: BLEManager
    @State private var logEntries: [LogEntry] = []
    @State private var showClearConfirmation = false

    struct LogEntry: Identifiable {
        let id = UUID()
        let timestamp: Date
        let message: String
        let type: LogType

        enum LogType {
            case info, success, warning, error
        }
    }

    var body: some View {
        NavigationView {
            List {
                // Connection Status
                Section("Connection") {
                    LabeledContent("State", value: bleManager.connectionState.rawValue)
                    LabeledContent("Device", value: bleManager.connectedDeviceName ?? "None")
                    LabeledContent("BT Ready", value: bleManager.isBluetoothReady ? "Yes" : "No")

                    if let error = bleManager.errorMessage {
                        HStack {
                            Image(systemName: "exclamationmark.triangle")
                                .foregroundStyle(.orange)
                            Text(error)
                                .font(.caption)
                        }
                    }
                }

                // Raw Current State
                Section("Current State (Raw)") {
                    LabeledContent("Weight", value: "\(bleManager.currentWeightG) g")
                    LabeledContent("Bottle Level", value: "\(bleManager.bottleLevelMl) ml")
                    LabeledContent("Daily Total", value: "\(bleManager.dailyTotalMl) ml")
                    LabeledContent("Battery", value: "\(bleManager.batteryPercent)%")

                    Divider()

                    LabeledContent("Time Valid", value: bleManager.isTimeValid ? "Yes" : "No")
                    LabeledContent("Calibrated", value: bleManager.isCalibrated ? "Yes" : "No")
                    LabeledContent("Stable", value: bleManager.isStable ? "Yes" : "No")
                    LabeledContent("Unsynced", value: "\(bleManager.unsyncedCount)")

                    if let lastUpdate = bleManager.lastStateUpdateTime {
                        LabeledContent("Last Update", value: lastUpdate.formatted(.dateTime.hour().minute().second()))
                    }
                }

                // Bottle Config
                Section("Bottle Config") {
                    LabeledContent("Capacity", value: "\(bleManager.bottleCapacityMl) ml")
                    LabeledContent("Daily Goal", value: "\(bleManager.dailyGoalMl) ml")
                }

                // Sync State
                Section("Sync State") {
                    LabeledContent("State", value: bleManager.syncState.rawValue)
                    LabeledContent("Progress", value: "\(Int(bleManager.syncProgress * 100))%")
                    LabeledContent("Records Synced", value: "\(bleManager.syncedRecordCount)")
                    LabeledContent("Total to Sync", value: "\(bleManager.totalRecordsToSync)")

                    if let lastSync = bleManager.lastSyncTime {
                        LabeledContent("Last Sync", value: lastSync.formatted(.dateTime))
                    }
                }

                // Quick Actions
                Section("Quick Actions") {
                    Button("Scan for Devices") {
                        addLog("Starting scan...", type: .info)
                        bleManager.startScanning()
                    }
                    .disabled(bleManager.connectionState.isActive)

                    Button("Disconnect") {
                        addLog("Disconnecting...", type: .info)
                        bleManager.disconnect()
                    }
                    .disabled(!bleManager.connectionState.isConnected)

                    Divider()

                    Button("Read Bottle Config") {
                        addLog("Reading bottle config...", type: .info)
                        bleManager.readBottleConfig()
                    }
                    .disabled(!bleManager.connectionState.isConnected)

                    Button("Sync Time") {
                        addLog("Syncing time...", type: .info)
                        bleManager.syncDeviceTime()
                    }
                    .disabled(!bleManager.connectionState.isConnected)

                    Button("Start Drink Sync") {
                        addLog("Starting drink sync (\(bleManager.unsyncedCount) records)...", type: .info)
                        bleManager.startDrinkSync()
                    }
                    .disabled(!bleManager.connectionState.isConnected || bleManager.unsyncedCount == 0)
                }

                // Commands
                Section("Commands") {
                    Button("TARE (Zero Scale)") {
                        addLog("Sent TARE command", type: .success)
                        bleManager.sendTareCommand()
                    }
                    .disabled(!bleManager.connectionState.isConnected)

                    Button("Reset Daily Total") {
                        addLog("Sent RESET_DAILY command", type: .success)
                        bleManager.sendResetDailyCommand()
                        PersistenceController.shared.deleteTodaysDrinkRecords()
                        addLog("Deleted today's CoreData records", type: .info)
                    }
                    .disabled(!bleManager.connectionState.isConnected)

                    Button("Start Calibration") {
                        addLog("Sent START_CALIBRATION command", type: .warning)
                        bleManager.sendStartCalibrationCommand()
                    }
                    .disabled(!bleManager.connectionState.isConnected)

                    Button("Cancel Calibration") {
                        addLog("Sent CANCEL_CALIBRATION command", type: .info)
                        bleManager.sendCancelCalibrationCommand()
                    }
                    .disabled(!bleManager.connectionState.isConnected)

                    Button("Clear Device History", role: .destructive) {
                        showClearConfirmation = true
                    }
                    .disabled(!bleManager.connectionState.isConnected)
                }

                // CoreData Stats
                Section("CoreData") {
                    let todaysRecords = PersistenceController.shared.getTodaysDrinkRecords()
                    let todaysTotal = PersistenceController.shared.getTodaysTotalMl()
                    let allRecords = PersistenceController.shared.getAllDrinkRecords()

                    LabeledContent("Today's Records", value: "\(todaysRecords.count)")
                    LabeledContent("Today's Total", value: "\(todaysTotal) ml")
                    LabeledContent("All Records", value: "\(allRecords.count)")

                    // Show recent record timestamps for debugging
                    if !allRecords.isEmpty {
                        Divider()
                        Text("Recent Records:").font(.caption).foregroundStyle(.secondary)
                        ForEach(allRecords.prefix(5), id: \.objectID) { record in
                            let ts = record.timestamp ?? Date.distantPast
                            let amount = record.amountMl
                            HStack {
                                Text("\(amount) ml")
                                    .font(.caption)
                                Spacer()
                                Text(ts.formatted(.dateTime))
                                    .font(.caption2)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }

                    Divider()

                    Button("Delete All CoreData Records", role: .destructive) {
                        PersistenceController.shared.deleteAllDrinkRecords()
                        addLog("Deleted all CoreData records", type: .warning)
                    }
                }

                // Activity Log
                Section("Activity Log") {
                    if logEntries.isEmpty {
                        Text("No activity yet")
                            .foregroundStyle(.secondary)
                    } else {
                        ForEach(logEntries.reversed()) { entry in
                            HStack {
                                Circle()
                                    .fill(colorForLogType(entry.type))
                                    .frame(width: 8, height: 8)
                                VStack(alignment: .leading) {
                                    Text(entry.message)
                                        .font(.caption)
                                    Text(entry.timestamp.formatted(.dateTime.hour().minute().second()))
                                        .font(.caption2)
                                        .foregroundStyle(.secondary)
                                }
                            }
                        }

                        Button("Clear Log") {
                            logEntries.removeAll()
                        }
                    }
                }
            }
            .navigationTitle("Debug")
            .confirmationDialog("Clear Device History?", isPresented: $showClearConfirmation) {
                Button("Clear History", role: .destructive) {
                    addLog("Sent CLEAR_HISTORY command", type: .warning)
                    bleManager.sendClearHistoryCommand()
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("This will delete all drink records on the device. This cannot be undone.")
            }
            .onReceive(bleManager.$connectionState) { state in
                addLog("Connection: \(state.rawValue)", type: state.isConnected ? .success : .info)
            }
            .onReceive(bleManager.$syncState) { state in
                if state != .idle {
                    addLog("Sync: \(state.rawValue)", type: state == .complete ? .success : .info)
                }
            }
        }
    }

    private func addLog(_ message: String, type: LogEntry.LogType) {
        logEntries.append(LogEntry(timestamp: Date(), message: message, type: type))
        // Keep last 50 entries
        if logEntries.count > 50 {
            logEntries.removeFirst()
        }
    }

    private func colorForLogType(_ type: LogEntry.LogType) -> Color {
        switch type {
        case .info: return .blue
        case .success: return .green
        case .warning: return .orange
        case .error: return .red
        }
    }
}

#Preview {
    DebugView()
        .environmentObject(BLEManager())
}
