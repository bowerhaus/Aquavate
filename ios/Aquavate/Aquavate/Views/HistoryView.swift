//
//  HistoryView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI

struct HistoryView: View {
    let drinks = DrinkRecord.sampleData
    @State private var selectedDate: Date = Date()

    private var last7Days: [Date] {
        let calendar = Calendar.current
        return (0..<7).compactMap { daysAgo in
            calendar.date(byAdding: .day, value: -daysAgo, to: Date())
        }.reversed()
    }

    private func totalForDate(_ date: Date) -> Int {
        let calendar = Calendar.current
        let targetDay = calendar.startOfDay(for: date)

        return drinks
            .filter { calendar.startOfDay(for: $0.timestamp) == targetDay }
            .reduce(0) { $0 + $1.amountMl }
    }

    private var drinksForSelectedDate: [DrinkRecord] {
        let calendar = Calendar.current
        let targetDay = calendar.startOfDay(for: selectedDate)

        return drinks
            .filter { calendar.startOfDay(for: $0.timestamp) == targetDay }
            .sorted { $0.timestamp > $1.timestamp }
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
                                isSelected: Calendar.current.isDate(date, inSameDayAs: selectedDate)
                            )
                            .onTapGesture {
                                selectedDate = date
                            }
                        }
                    }
                    .padding(.horizontal)
                    .padding(.vertical, 16)
                }
                .background(Color(.systemGroupedBackground))

                // Selected day detail
                if drinksForSelectedDate.isEmpty {
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
                            ForEach(drinksForSelectedDate) { drink in
                                DrinkListItem(drink: drink)
                            }
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
        Calendar.current.isDateInToday(date)
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
}
