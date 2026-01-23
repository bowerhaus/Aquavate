//
//  ActivityStatsView.swift
//  Aquavate
//
//  Sleep mode analysis view for battery diagnostics (Issue #36).
//  Displays motion wake events and backpack sessions to help users
//  understand bottle activity and battery usage patterns.
//

import SwiftUI

struct ActivityStatsView: View {
    @EnvironmentObject var bleManager: BLEManager

    var body: some View {
        List {
            // Connection Status Section
            if !bleManager.connectionState.isConnected {
                Section {
                    HStack {
                        Image(systemName: "exclamationmark.triangle")
                            .foregroundStyle(.orange)
                        Text("Connect to bottle to view activity stats")
                            .foregroundStyle(.secondary)
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

                // Summary Section
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

                    Section("Since Last Charge") {
                        HStack {
                            Image(systemName: "hand.raised.fill")
                                .foregroundStyle(.blue)
                            Text("Motion Wakes")
                            Spacer()
                            Text("\(summary.motionEventCount)")
                                .foregroundStyle(.secondary)
                        }

                        HStack {
                            Image(systemName: "bag")
                                .foregroundStyle(.orange)
                            Text("Backpack Sessions")
                            Spacer()
                            Text("\(summary.backpackSessionCount)")
                                .foregroundStyle(.secondary)
                        }
                    }
                }

                // Motion Wake Events Section
                if !bleManager.motionWakeEvents.isEmpty {
                    Section("Recent Motion Wakes") {
                        ForEach(bleManager.motionWakeEvents.reversed().prefix(20), id: \.timestamp) { event in
                            HStack {
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(event.date, style: .time)
                                        .font(.headline)
                                    Text(event.date, style: .date)
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
                                    if event.enteredSleepType == .extended {
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

                // Backpack Sessions Section
                if !bleManager.backpackSessions.isEmpty {
                    Section("Backpack Sessions") {
                        ForEach(bleManager.backpackSessions.reversed(), id: \.startTimestamp) { session in
                            VStack(alignment: .leading, spacing: 4) {
                                HStack {
                                    Text(session.startDate, style: .date)
                                    Text(session.startDate, style: .time)
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
                                    Text(exitReasonText(session.exit))
                                }
                                .font(.caption)
                                .foregroundStyle(.secondary)
                            }
                            .padding(.vertical, 4)
                        }
                    }
                }

                // Empty State
                if bleManager.activityFetchState == .complete &&
                   bleManager.motionWakeEvents.isEmpty &&
                   bleManager.backpackSessions.isEmpty {
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
            // Lazy load: fetch data only when view appears
            if bleManager.connectionState.isConnected {
                bleManager.fetchActivityStats()
            }
        }
        .refreshable {
            // Pull to refresh
            if bleManager.connectionState.isConnected {
                bleManager.fetchActivityStats()
            }
        }
    }

    // MARK: - Computed Properties

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

    private func exitReasonText(_ reason: BLEBackpackSession.ExitReason) -> String {
        switch reason {
        case .motionDetected:
            return "Motion detected"
        case .stillActive:
            return "Still active"
        case .powerCycle:
            return "Power cycle"
        }
    }
}

#Preview {
    NavigationView {
        ActivityStatsView()
            .environmentObject(BLEManager())
    }
}
