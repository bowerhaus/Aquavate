//
//  DrinkListItem.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI

struct DrinkListItem: View {
    let drink: DrinkRecord

    private var timeString: String {
        let formatter = DateFormatter()
        formatter.timeStyle = .short
        return formatter.string(from: drink.timestamp)
    }

    var body: some View {
        HStack(spacing: 12) {
            // Water drop icon
            Image(systemName: "drop.fill")
                .font(.title2)
                .foregroundStyle(.blue)
                .frame(width: 32)

            // Drink details - amount consumed emphasized
            VStack(alignment: .leading, spacing: 4) {
                Text("\(drink.amountMl)ml")
                    .font(.headline)

                Text(timeString)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            // Remaining level - de-emphasized (clamped to 0 minimum)
            VStack(alignment: .trailing, spacing: 4) {
                Text("\(max(0, drink.bottleLevelMl))ml")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)

                Text("remaining")
                    .font(.caption)
                    .foregroundStyle(.tertiary)
            }
        }
        .padding(.vertical, 4)
    }
}

#Preview {
    List {
        DrinkListItem(drink: DrinkRecord.sampleData[0])
        DrinkListItem(drink: DrinkRecord.sampleData[1])
        DrinkListItem(drink: DrinkRecord.sampleData[2])
    }
}
