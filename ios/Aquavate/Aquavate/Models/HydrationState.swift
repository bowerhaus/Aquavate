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

    // Notification request fields (for iPhone-triggered Watch notifications)
    let shouldNotify: Bool           // True if Watch should show notification
    let notificationType: String?    // "reminder" or "goalReached"
    let notificationTitle: String?   // e.g., "Hydration Reminder"
    let notificationBody: String?    // e.g., "Time to hydrate! You're 350ml behind pace."

    var progress: Double {
        guard dailyGoalMl > 0 else { return 0 }
        return min(Double(dailyTotalMl) / Double(dailyGoalMl), 1.0)
    }

    var isGoalAchieved: Bool {
        dailyTotalMl >= dailyGoalMl
    }

    // Custom decoder for backwards compatibility with saved data missing optional fields
    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        dailyTotalMl = try container.decode(Int.self, forKey: .dailyTotalMl)
        dailyGoalMl = try container.decode(Int.self, forKey: .dailyGoalMl)
        lastDrinkTime = try container.decodeIfPresent(Date.self, forKey: .lastDrinkTime)
        urgency = try container.decode(HydrationUrgency.self, forKey: .urgency)
        deficitMl = try container.decodeIfPresent(Int.self, forKey: .deficitMl) ?? 0
        timestamp = try container.decode(Date.self, forKey: .timestamp)
        shouldNotify = try container.decodeIfPresent(Bool.self, forKey: .shouldNotify) ?? false
        notificationType = try container.decodeIfPresent(String.self, forKey: .notificationType)
        notificationTitle = try container.decodeIfPresent(String.self, forKey: .notificationTitle)
        notificationBody = try container.decodeIfPresent(String.self, forKey: .notificationBody)
    }

    // Standard memberwise initializer (without notification fields - defaults to no notification)
    init(dailyTotalMl: Int, dailyGoalMl: Int, lastDrinkTime: Date?, urgency: HydrationUrgency, deficitMl: Int, timestamp: Date) {
        self.dailyTotalMl = dailyTotalMl
        self.dailyGoalMl = dailyGoalMl
        self.lastDrinkTime = lastDrinkTime
        self.urgency = urgency
        self.deficitMl = deficitMl
        self.timestamp = timestamp
        self.shouldNotify = false
        self.notificationType = nil
        self.notificationTitle = nil
        self.notificationBody = nil
    }

    // Full memberwise initializer (with notification fields)
    init(dailyTotalMl: Int, dailyGoalMl: Int, lastDrinkTime: Date?, urgency: HydrationUrgency, deficitMl: Int, timestamp: Date,
         shouldNotify: Bool, notificationType: String?, notificationTitle: String?, notificationBody: String?) {
        self.dailyTotalMl = dailyTotalMl
        self.dailyGoalMl = dailyGoalMl
        self.lastDrinkTime = lastDrinkTime
        self.urgency = urgency
        self.deficitMl = deficitMl
        self.timestamp = timestamp
        self.shouldNotify = shouldNotify
        self.notificationType = notificationType
        self.notificationTitle = notificationTitle
        self.notificationBody = notificationBody
    }
}
