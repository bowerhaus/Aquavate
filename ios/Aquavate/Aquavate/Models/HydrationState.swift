//
//  HydrationState.swift
//  Aquavate
//
//  Created by Claude Code on 22/01/2026.
//

import Foundation
import SwiftUI

// MARK: - Hydration Urgency

enum HydrationUrgency: Int, Codable, Comparable {
    case onTrack = 0    // Blue: on pace or ahead of schedule
    case attention = 1  // Amber: 1-19% behind pace
    case overdue = 2    // Red: 20%+ behind pace

    var color: Color {
        switch self {
        case .onTrack: return .blue
        case .attention: return .orange
        case .overdue: return .red
        }
    }

    var description: String {
        switch self {
        case .onTrack: return "On Track"
        case .attention: return "Time to Hydrate"
        case .overdue: return "Overdue"
        }
    }

    static func < (lhs: HydrationUrgency, rhs: HydrationUrgency) -> Bool {
        lhs.rawValue < rhs.rawValue
    }
}

// MARK: - Hydration State

struct HydrationState: Codable {
    let dailyTotalMl: Int
    let dailyGoalMl: Int
    let lastDrinkTime: Date?
    let urgency: HydrationUrgency
    let deficitMl: Int          // How much behind pace (0 if on track, negative if ahead)
    let timestamp: Date

    var progress: Double {
        guard dailyGoalMl > 0 else { return 0 }
        return min(Double(dailyTotalMl) / Double(dailyGoalMl), 1.0)
    }

    var isGoalAchieved: Bool {
        dailyTotalMl >= dailyGoalMl
    }

    // Custom decoder for backwards compatibility with saved data missing deficitMl
    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        dailyTotalMl = try container.decode(Int.self, forKey: .dailyTotalMl)
        dailyGoalMl = try container.decode(Int.self, forKey: .dailyGoalMl)
        lastDrinkTime = try container.decodeIfPresent(Date.self, forKey: .lastDrinkTime)
        urgency = try container.decode(HydrationUrgency.self, forKey: .urgency)
        deficitMl = try container.decodeIfPresent(Int.self, forKey: .deficitMl) ?? 0
        timestamp = try container.decode(Date.self, forKey: .timestamp)
    }

    // Standard memberwise initializer
    init(dailyTotalMl: Int, dailyGoalMl: Int, lastDrinkTime: Date?, urgency: HydrationUrgency, deficitMl: Int, timestamp: Date) {
        self.dailyTotalMl = dailyTotalMl
        self.dailyGoalMl = dailyGoalMl
        self.lastDrinkTime = lastDrinkTime
        self.urgency = urgency
        self.deficitMl = deficitMl
        self.timestamp = timestamp
    }
}
