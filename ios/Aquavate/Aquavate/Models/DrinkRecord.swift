//
//  DrinkRecord.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import Foundation

struct DrinkRecord: Identifiable {
    let id: UUID
    let timestamp: Date
    let amountMl: Int
    let bottleLevelMl: Int

    init(id: UUID = UUID(), timestamp: Date, amountMl: Int, bottleLevelMl: Int) {
        self.id = id
        self.timestamp = timestamp
        self.amountMl = amountMl
        self.bottleLevelMl = bottleLevelMl
    }
}

// MARK: - Sample Data
extension DrinkRecord {
    static var sampleData: [DrinkRecord] {
        let now = Date()
        let calendar = Calendar.current

        return [
            // Today's drinks
            DrinkRecord(
                timestamp: calendar.date(byAdding: .minute, value: -15, to: now)!,
                amountMl: 180,
                bottleLevelMl: 420
            ),
            DrinkRecord(
                timestamp: calendar.date(byAdding: .hour, value: -2, to: now)!,
                amountMl: 150,
                bottleLevelMl: 600
            ),
            DrinkRecord(
                timestamp: calendar.date(byAdding: .hour, value: -4, to: now)!,
                amountMl: 200,
                bottleLevelMl: 750
            ),
            DrinkRecord(
                timestamp: calendar.date(byAdding: .hour, value: -6, to: now)!,
                amountMl: 250,
                bottleLevelMl: 700
            ),
            DrinkRecord(
                timestamp: calendar.date(byAdding: .hour, value: -8, to: now)!,
                amountMl: 180,
                bottleLevelMl: 450
            ),

            // Yesterday's drinks
            DrinkRecord(
                timestamp: calendar.date(byAdding: .day, value: -1, to: now)!,
                amountMl: 220,
                bottleLevelMl: 230
            ),
            DrinkRecord(
                timestamp: calendar.date(byAdding: .hour, value: -28, to: now)!,
                amountMl: 180,
                bottleLevelMl: 450
            ),
            DrinkRecord(
                timestamp: calendar.date(byAdding: .hour, value: -30, to: now)!,
                amountMl: 200,
                bottleLevelMl: 630
            ),
            DrinkRecord(
                timestamp: calendar.date(byAdding: .hour, value: -33, to: now)!,
                amountMl: 150,
                bottleLevelMl: 750
            ),

            // Two days ago
            DrinkRecord(
                timestamp: calendar.date(byAdding: .day, value: -2, to: now)!,
                amountMl: 190,
                bottleLevelMl: 310
            ),
            DrinkRecord(
                timestamp: calendar.date(byAdding: .hour, value: -52, to: now)!,
                amountMl: 170,
                bottleLevelMl: 500
            ),
            DrinkRecord(
                timestamp: calendar.date(byAdding: .hour, value: -54, to: now)!,
                amountMl: 250,
                bottleLevelMl: 670
            )
        ]
    }
}
