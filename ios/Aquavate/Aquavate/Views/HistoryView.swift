//
//  HistoryView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI
import CoreData

struct HistoryView: View {
    @EnvironmentObject var bleManager: BLEManager
    @Environment(\.managedObjectContext) private var viewContext
    @State private var selectedDate: Date = Date()
    @State private var showConnectionRequiredAlert = false
    @State private var isDeleting = false
    @State private var showBottleAsleepAlert = false
    @State private var showErrorAlert = false
    @State private var errorAlertMessage = ""

    // Fetch last 7 days of drink records from CoreData
    // Predicate limits CoreData fetch to recent records only (reduces memory usage)
    // Uses midnight boundary to match firmware DRINK_DAILY_RESET_HOUR
    @FetchRequest(
        sortDescriptors: [NSSortDescriptor(keyPath: \CDDrinkRecord.timestamp, ascending: false)],
        predicate: NSPredicate(format: "timestamp >= %@", Calendar.current.date(byAdding: .day, value: -7, to: Calendar.current.startOfAquavateDay(for: Date()))! as CVarArg),
        animation: .default
    )
    private var recentDrinksCD: FetchedResults<CDDrinkRecord>

    private var last7Days: [Date] {
        let calendar = Calendar.current
        return (0..<7).compactMap { daysAgo in
            calendar.date(byAdding: .day, value: -daysAgo, to: Date())
        }.reversed()
    }

    // Compute totals directly from CoreData to avoid creating intermediate objects
    private func totalForDate(_ date: Date) -> Int {
        let calendar = Calendar.current
        let targetDay = calendar.startOfAquavateDay(for: date)

        return recentDrinksCD
            .filter { calendar.startOfAquavateDay(for: $0.timestamp ?? .distantPast) == targetDay }
            .reduce(0) { $0 + Int($1.amountMl) }
    }

    // Get CDDrinkRecords for selected date (for deletion)
    private var drinksForSelectedDateCD: [CDDrinkRecord] {
        let calendar = Calendar.current
        let targetDay = calendar.startOfAquavateDay(for: selectedDate)

        return recentDrinksCD
            .filter { calendar.startOfAquavateDay(for: $0.timestamp ?? .distantPast) == targetDay }
            .sorted { ($0.timestamp ?? .distantPast) > ($1.timestamp ?? .distantPast) }
    }

    // Delete drinks at given offsets - requires bottle connection for bidirectional sync
    private func deleteDrinks(at offsets: IndexSet) {
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
                let cdRecord = drinksForSelectedDateCD[index]
                guard let id = cdRecord.id else { continue }

                let firmwareId = cdRecord.firmwareRecordId

                // If we have a firmware ID, use pessimistic delete
                if firmwareId != 0 {
                    await withCheckedContinuation { continuation in
                        bleManager.deleteDrinkRecord(firmwareRecordId: UInt32(firmwareId)) { success in
                            if success {
                                // Bottle confirmed - now safe to delete locally
                                PersistenceController.shared.deleteDrinkRecord(id: id)
                            }
                            // If failed, leave record in place - user can retry
                            continuation.resume()
                        }
                    }
                } else {
                    // No firmware ID (old record) - delete locally only
                    PersistenceController.shared.deleteDrinkRecord(id: id)
                }
            }
            isDeleting = false
        }
    }

    var body: some View {
        NavigationView {
            VStack(spacing: 0) {
                // Calendar grid - 7 days
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 12) {
                        ForEach(last7Days, id: \.self) { date in
                            DayCard(
                                date: date,
                                totalMl: totalForDate(date),
                                isSelected: Calendar.current.isDate(date, inSameAquavateDayAs: selectedDate)
                            )
                            .id(date)
                            .onTapGesture {
                                selectedDate = date
                            }
                        }
                    }
                    .padding(.horizontal)
                    .padding(.vertical, 16)
                }
                .defaultScrollAnchor(.trailing)
                .background(Color(.systemGroupedBackground))

                // Selected day detail
                if drinksForSelectedDateCD.isEmpty {
                    VStack(spacing: 16) {
                        Spacer()
                        Image(systemName: "drop.slash")
                            .font(.system(size: 48))
                            .foregroundStyle(.secondary)
                        Text("No drinks recorded")
                            .font(.title3)
                            .foregroundStyle(.secondary)
                        Spacer()
                    }
                } else {
                    List {
                        Section {
                            ForEach(drinksForSelectedDateCD) { cdRecord in
                                DrinkListItem(drink: cdRecord.toDrinkRecord())
                                    .frame(minHeight: 44)
                            }
                            .onDelete(perform: deleteDrinks)
                        } header: {
                            Text("Drinks for \(formattedDate(selectedDate))")
                        } footer: {
                            HStack {
                                Text("Total consumed:")
                                Spacer()
                                Text("\(totalForDate(selectedDate))ml")
                                    .fontWeight(.semibold)
                            }
                            .padding(.top, 8)
                        }
                    }
                }
            }
            .navigationTitle("History")
            .refreshable {
                await handleRefresh()
            }
            .alert("Connection Required", isPresented: $showConnectionRequiredAlert) {
                Button("OK", role: .cancel) { }
            } message: {
                Text("Please connect to your bottle before deleting drinks. This ensures both the app and bottle stay in sync.")
            }
            .alert("Bottle is Asleep", isPresented: $showBottleAsleepAlert) {
                Button("Retry") {
                    Task {
                        await handleRefresh()
                    }
                }
                Button("Cancel", role: .cancel) { }
            } message: {
                Text("Tap or tilt your bottle to wake it up, then hit Retry.")
            }
            .alert("Sync Error", isPresented: $showErrorAlert) {
                Button("OK", role: .cancel) { }
            } message: {
                Text(errorAlertMessage)
            }
        }
    }

    private func handleRefresh() async {
        let result = await bleManager.performRefresh()

        switch result {
        case .synced(let count):
            print("History: Synced \(count) records")
        case .alreadySynced:
            print("History: Already synced")
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

    private func formattedDate(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        return formatter.string(from: date)
    }
}

struct DayCard: View {
    let date: Date
    let totalMl: Int
    let isSelected: Bool

    private var dayName: String {
        let formatter = DateFormatter()
        formatter.dateFormat = "EEE"
        return formatter.string(from: date)
    }

    private var dayNumber: String {
        let formatter = DateFormatter()
        formatter.dateFormat = "d"
        return formatter.string(from: date)
    }

    private var isToday: Bool {
        Calendar.current.isDateInAquavateToday(date)
    }

    var body: some View {
        VStack(spacing: 8) {
            Text(dayName)
                .font(.caption)
                .foregroundStyle(.secondary)

            Text(dayNumber)
                .font(.title2)
                .fontWeight(isToday ? .bold : .regular)

            Text("\(totalMl)ml")
                .font(.caption)
                .fontWeight(.medium)
                .foregroundStyle(totalMl > 0 ? .blue : .secondary)
        }
        .frame(width: 60)
        .padding(.vertical, 12)
        .background(isSelected ? Color.blue.opacity(0.1) : Color.clear)
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .stroke(isSelected ? Color.blue : Color.clear, lineWidth: 2)
        )
        .cornerRadius(8)
    }
}

#Preview {
    HistoryView()
        .environmentObject(BLEManager())
}
