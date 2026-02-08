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

// MARK: - Unified Session Model

enum SessionType {
    case normal(drinkTaken: Bool)
    case backpack(timerWakes: Int32)
}

struct UnifiedSession: Identifiable {
    let id: UUID
    let timestamp: Date
    let durationSec: Int32
    let type: SessionType
}

// MARK: - Activity Stats View

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
                    } else if unifiedSessions.isEmpty {
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

            // Summary Counts Section (Last 7 Days)
            if !sessionsLast7Days.isEmpty {
                Section("Last 7 Days") {
                    HStack {
                        Image(systemName: "hand.raised.fill")
                            .foregroundStyle(.blue)
                        Text("Normal Sessions")
                        Spacer()
                        Text("\(normalSessionsLast7Days)")
                            .foregroundStyle(.secondary)
                    }

                    HStack {
                        Image(systemName: "bag")
                            .foregroundStyle(.orange)
                        Text("Backpack Sessions")
                        Spacer()
                        Text("\(backpackSessionsLast7Days)")
                            .foregroundStyle(.secondary)
                    }
                }
            }

            // Unified Sessions List (Last 7 Days)
            if !sessionsLast7Days.isEmpty {
                Section("Sessions") {
                    ForEach(sessionsLast7Days) { session in
                        HStack {
                            // Timestamp
                            VStack(alignment: .leading, spacing: 2) {
                                Text(session.timestamp, style: .time)
                                    .font(.headline)
                                Text(session.timestamp, style: .date)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }

                            // Drink indicator (for normal sessions)
                            if case .normal(let drinkTaken) = session.type, drinkTaken {
                                Image(systemName: "drop.fill")
                                    .foregroundStyle(.blue)
                                    .font(.caption)
                            }

                            Spacer()

                            // Duration and type
                            VStack(alignment: .trailing, spacing: 2) {
                                Text(formatDuration(Int(session.durationSec)))
                                    .font(.subheadline)

                                // Session type badge
                                switch session.type {
                                case .normal:
                                    Text("Normal")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                case .backpack(let timerWakes):
                                    HStack(spacing: 2) {
                                        Image(systemName: "bag")
                                        Text("Backpack")
                                        if timerWakes > 0 {
                                            Text("(\(timerWakes))")
                                        }
                                    }
                                    .font(.caption)
                                    .foregroundStyle(.orange)
                                }
                            }
                        }
                        .padding(.vertical, 2)
                    }
                }
            }

            // Empty State
            if sessionsLast7Days.isEmpty {
                if bleManager.connectionState.isConnected && bleManager.activityFetchState == .complete {
                    Section {
                        HStack {
                            Image(systemName: "checkmark.circle")
                                .foregroundStyle(.green)
                            Text("No activity recorded in the last 7 days")
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

    /// Merge motion wake events and backpack sessions into a single sorted list
    private var unifiedSessions: [UnifiedSession] {
        var sessions: [UnifiedSession] = []

        // Add motion wake events as "Normal" sessions
        for event in motionWakeEvents {
            sessions.append(UnifiedSession(
                id: event.id ?? UUID(),
                timestamp: event.timestamp ?? Date(),
                durationSec: Int32(event.durationSec),
                type: .normal(drinkTaken: event.drinkTaken)
            ))
        }

        // Add backpack sessions
        for session in backpackSessions {
            sessions.append(UnifiedSession(
                id: session.id ?? UUID(),
                timestamp: session.startTimestamp ?? Date(),
                durationSec: session.durationSec,
                type: .backpack(timerWakes: session.timerWakeCount)
            ))
        }

        // Sort by timestamp descending (most recent first)
        return sessions.sorted { $0.timestamp > $1.timestamp }
    }

    /// Sessions from the last 7 days only
    private var sessionsLast7Days: [UnifiedSession] {
        let sevenDaysAgo = Calendar.current.date(byAdding: .day, value: -7, to: Date()) ?? Date()
        return unifiedSessions.filter { $0.timestamp >= sevenDaysAgo }
    }

    /// Count of normal sessions in the last 7 days
    private var normalSessionsLast7Days: Int {
        sessionsLast7Days.filter {
            if case .normal = $0.type { return true }
            return false
        }.count
    }

    /// Count of backpack sessions in the last 7 days
    private var backpackSessionsLast7Days: Int {
        sessionsLast7Days.filter {
            if case .backpack = $0.type { return true }
            return false
        }.count
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

}

#Preview {
    NavigationView {
        ActivityStatsView()
            .environmentObject(BLEManager())
            .environment(\.managedObjectContext, PersistenceController.preview.container.viewContext)
    }
}
