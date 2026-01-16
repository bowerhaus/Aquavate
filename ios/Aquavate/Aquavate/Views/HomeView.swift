//
//  HomeView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI

struct HomeView: View {
    let bottle = Bottle.sample
    let drinks = DrinkRecord.sampleData

    private var todaysDrinks: [DrinkRecord] {
        let calendar = Calendar.current
        let today = calendar.startOfDay(for: Date())
        return drinks.filter { calendar.startOfDay(for: $0.timestamp) == today }
    }

    private var todaysTotalMl: Int {
        todaysDrinks.reduce(0) { $0 + $1.amountMl }
    }

    private var dailyProgress: Double {
        guard bottle.dailyGoalMl > 0 else { return 0 }
        return Double(todaysTotalMl) / Double(bottle.dailyGoalMl)
    }

    private var recentDrinks: [DrinkRecord] {
        Array(todaysDrinks.prefix(5))
    }

    var body: some View {
        NavigationView {
            ScrollView {
                VStack(spacing: 24) {
                    // Sync status
                    HStack {
                        Image(systemName: "arrow.triangle.2.circlepath")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        Text("Last synced 5 min ago")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    .padding(.top, 8)

                    // Circular progress ring - bottle level
                    CircularProgressView(
                        current: bottle.currentLevelMl,
                        total: bottle.capacityMl,
                        color: .blue
                    )
                    .padding(.vertical, 16)

                    // Daily goal progress bar
                    VStack(alignment: .leading, spacing: 8) {
                        HStack {
                            Text("Today's Goal")
                                .font(.headline)
                            Spacer()
                            Text("\(Int(dailyProgress * 100))%")
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                        }

                        ProgressView(value: dailyProgress)
                            .tint(.blue)

                        HStack {
                            Text("\(todaysTotalMl)ml")
                                .font(.subheadline)
                                .fontWeight(.medium)
                            Text("/ \(bottle.dailyGoalMl)ml")
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                        }
                    }
                    .padding(.horizontal)

                    Divider()
                        .padding(.horizontal)

                    // Recent drinks list
                    VStack(alignment: .leading, spacing: 12) {
                        Text("Recent Drinks")
                            .font(.headline)
                            .padding(.horizontal)

                        if recentDrinks.isEmpty {
                            Text("No drinks recorded today")
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 32)
                        } else {
                            ForEach(recentDrinks) { drink in
                                DrinkListItem(drink: drink)
                                    .padding(.horizontal)

                                if drink.id != recentDrinks.last?.id {
                                    Divider()
                                        .padding(.leading, 60)
                                }
                            }
                        }
                    }

                    Spacer(minLength: 20)
                }
            }
            .navigationTitle("Aquavate")
        }
    }
}

#Preview {
    HomeView()
}
