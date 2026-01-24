//
//  ActivityStatsView.swift
//  Aquavate
//
//  Sleep mode analysis view for battery diagnostics (Issue #36).
//  Displays motion wake events and backpack sessions to help users
//  understand bottle activity and battery usage patterns.
//
//  Data is stored locally in CoreData for offline viewing.
//

import SwiftUI
import CoreData

struct ActivityStatsView: View {
    @EnvironmentObject var bleManager: BLEManager
    @Environment(\.managedObjectContext) private var viewContext

    // Fetch motion wake events from CoreData, sorted by timestamp descending
    @FetchRequest(
        sortDescriptors: [NSSortDescriptor(keyPath: \CDMotionWakeEvent.timestamp, ascending: false)],
        animation: .default
    ) private var motionWakeEvents: FetchedResults<CDMotionWakeEvent>

    // Fetch backpack sessions from CoreData, sorted by start timestamp descending
    @FetchRequest(
        sortDescriptors: [NSSortDescriptor(keyPath: \CDBackpackSession.startTimestamp, ascending: false)],
        animation: .default
    ) private var backpackSessions: FetchedResults<CDBackpackSession>

    var body: some View {
        List {
            // Last Synced / Connection Status Section
            if !bleManager.connectionState.isConnected {
                Section {
                    if let lastSync = lastActivitySyncDate {
                        HStack {
                            Image(systemName: "clock")
                                .foregroundStyle(.secondary)
                            Text("Last synced \(lastSync, style: .relative) ago")
                                .foregroundStyle(.secondary)
                        }
                    } else if motionWakeEvents.isEmpty && backpackSessions.isEmpty {
                        HStack {
                            Image(systemName: "exclamationmark.triangle")
                                .foregroundStyle(.orange)
                            Text("Connect to bottle to view activity stats")
                                .foregroundStyle(.secondary)
                        }
                    }
                }
            } else {
                // Loading State
                if bleManager.activityFetchState.isLoading {
                    Section {
                        HStack {
                            ProgressView()
                                .scaleEffect(0.8)
                            Text(loadingStatusText)
                                .foregroundStyle(.secondary)
                        }
                    }
                }

                // Error State
                if case .failed(let error) = bleManager.activityFetchState {
                    Section {
                        HStack {
                            Image(systemName: "exclamationmark.triangle")
                                .foregroundStyle(.red)
                            Text(error)
                                .foregroundStyle(.secondary)
                        }
                    }
                }

                // Current Status Section (only when connected - shows live data)
                if let summary = bleManager.activitySummary {
                    Section("Current Status") {
                        if summary.isInBackpackMode {
                            HStack {
                                Image(systemName: "bag.fill")
                                    .foregroundStyle(.orange)
                                VStack(alignment: .leading, spacing: 2) {
                                    Text("In Backpack Mode")
                                        .font(.headline)
                                    if let startDate = summary.currentSessionStartDate {
                                        Text("Started \(startDate, style: .relative) ago")
                                            .font(.caption)
                                            .foregroundStyle(.secondary)
                                    }
                                }
                                Spacer()
                                Text("\(summary.currentTimerWakes) wakes")
                                    .foregroundStyle(.secondary)
                            }
                        } else {
                            HStack {
                                Image(systemName: "drop.fill")
                                    .foregroundStyle(.blue)
                                Text("Normal Mode")
                                    .font(.headline)
                                Spacer()
                                Text("Ready")
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }
            }

            // Summary Counts Section (from CoreData)
            if !motionWakeEvents.isEmpty || !backpackSessions.isEmpty {
                Section("Since Last Charge") {
                    HStack {
                        Image(systemName: "hand.raised.fill")
                            .foregroundStyle(.blue)
                        Text("Motion Wakes")
                        Spacer()
                        Text("\(motionWakeEvents.count)")
                            .foregroundStyle(.secondary)
                    }

                    HStack {
                        Image(systemName: "bag")
                            .foregroundStyle(.orange)
                        Text("Backpack Sessions")
                        Spacer()
                        Text("\(backpackSessions.count)")
                            .foregroundStyle(.secondary)
                    }
                }
            }

            // Motion Wake Events Section (from CoreData)
            if !motionWakeEvents.isEmpty {
                Section("Recent Motion Wakes") {
                    ForEach(motionWakeEvents.prefix(20), id: \.id) { event in
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(event.timestamp ?? Date(), style: .time)
                                    .font(.headline)
                                Text(event.timestamp ?? Date(), style: .date)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                            if event.drinkTaken {
                                Image(systemName: "drop.fill")
                                    .foregroundStyle(.blue)
                                    .font(.caption)
                            }
                            Spacer()
                            VStack(alignment: .trailing, spacing: 2) {
                                Text(formatDuration(Int(event.durationSec)))
                                    .font(.subheadline)
                                if event.sleepType == 1 {  // Extended sleep
                                    HStack(spacing: 2) {
                                        Image(systemName: "bag")
                                        Text("Backpack")
                                    }
                                    .font(.caption)
                                    .foregroundStyle(.orange)
                                } else {
                                    Text("Normal")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                            }
                        }
                        .padding(.vertical, 2)
                    }
                }
            }

            // Backpack Sessions Section (from CoreData)
            if !backpackSessions.isEmpty {
                Section("Backpack Sessions") {
                    ForEach(backpackSessions, id: \.id) { session in
                        VStack(alignment: .leading, spacing: 4) {
                            HStack {
                                Text(session.startTimestamp ?? Date(), style: .date)
                                Text(session.startTimestamp ?? Date(), style: .time)
                            }
                            .font(.headline)

                            HStack {
                                Label(formatDuration(Int(session.durationSec)), systemImage: "clock")
                                Spacer()
                                Label("\(session.timerWakeCount) wakes", systemImage: "timer")
                            }
                            .font(.subheadline)
                            .foregroundStyle(.secondary)

                            HStack {
                                Text("Exit:")
                                Text(exitReasonText(Int(session.exitReason)))
                            }
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        }
                        .padding(.vertical, 4)
                    }
                }
            }

            // Empty State
            if motionWakeEvents.isEmpty && backpackSessions.isEmpty {
                if bleManager.connectionState.isConnected && bleManager.activityFetchState == .complete {
                    Section {
                        HStack {
                            Image(systemName: "checkmark.circle")
                                .foregroundStyle(.green)
                            Text("No activity recorded since last charge")
                                .foregroundStyle(.secondary)
                        }
                    }
                }
            }
        }
        .navigationTitle("Activity Stats")
        .onAppear {
            // Lazy load: fetch data only when view appears and connected
            if bleManager.connectionState.isConnected {
                bleManager.fetchActivityStats()
            }
        }
        .refreshable {
            // Pull to refresh (only works when connected)
            if bleManager.connectionState.isConnected {
                bleManager.fetchActivityStats()
            }
        }
    }

    // MARK: - Computed Properties

    private var lastActivitySyncDate: Date? {
        let bottleId = PersistenceController.shared.getOrCreateDefaultBottle().id
        guard let id = bottleId else { return nil }
        return PersistenceController.shared.getLastActivitySyncDate(for: id)
    }

    private var loadingStatusText: String {
        switch bleManager.activityFetchState {
        case .fetchingSummary:
            return "Loading summary..."
        case .fetchingMotionEvents(let current, let total):
            return "Loading motion events (\(current + 1)/\(total))..."
        case .fetchingBackpackSessions(let current, let total):
            return "Loading backpack sessions (\(current + 1)/\(total))..."
        default:
            return "Loading..."
        }
    }

    // MARK: - Helper Methods

    private func formatDuration(_ seconds: Int) -> String {
        let hours = seconds / 3600
        let minutes = (seconds % 3600) / 60
        let secs = seconds % 60

        if hours > 0 {
            return "\(hours)h \(minutes)m"
        } else if minutes > 0 {
            return "\(minutes)m \(secs)s"
        } else {
            return "\(secs)s"
        }
    }

    private func exitReasonText(_ reason: Int) -> String {
        switch reason {
        case 0:
            return "Motion detected"
        case 1:
            return "Still active"
        case 2:
            return "Power cycle"
        default:
            return "Unknown"
        }
    }
}

#Preview {
    NavigationView {
        ActivityStatsView()
            .environmentObject(BLEManager())
            .environment(\.managedObjectContext, PersistenceController.preview.container.viewContext)
    }
}
