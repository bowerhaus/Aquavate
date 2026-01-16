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

            // Drink details
            VStack(alignment: .leading, spacing: 4) {
                Text(timeString)
                    .font(.headline)

                Text("\(drink.amountMl)ml consumed")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            // Remaining level
            VStack(alignment: .trailing, spacing: 4) {
                Text("\(drink.bottleLevelMl)ml")
                    .font(.subheadline)
                    .fontWeight(.medium)

                Text("remaining")
                    .font(.caption)
                    .foregroundStyle(.secondary)
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
