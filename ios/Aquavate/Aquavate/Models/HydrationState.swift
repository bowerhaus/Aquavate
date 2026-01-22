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
    case onTrack = 0    // Blue: < 45 min since last drink
    case attention = 1  // Amber: 45-90 min since last drink
    case overdue = 2    // Red: 90+ min since last drink

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
    let timestamp: Date

    var progress: Double {
        guard dailyGoalMl > 0 else { return 0 }
        return min(Double(dailyTotalMl) / Double(dailyGoalMl), 1.0)
    }

    var isGoalAchieved: Bool {
        dailyTotalMl >= dailyGoalMl
    }
}
