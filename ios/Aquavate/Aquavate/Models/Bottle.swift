//
//  Bottle.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import Foundation

struct Bottle {
    let name: String
    let capacityMl: Int
    let currentLevelMl: Int
    let dailyGoalMl: Int
    let batteryPercent: Int

    init(name: String, capacityMl: Int, currentLevelMl: Int, dailyGoalMl: Int, batteryPercent: Int) {
        self.name = name
        self.capacityMl = capacityMl
        self.currentLevelMl = currentLevelMl
        self.dailyGoalMl = dailyGoalMl
        self.batteryPercent = batteryPercent
    }
}

// MARK: - Sample Data
extension Bottle {
    static var sample: Bottle {
        Bottle(
            name: "My Bottle",
            capacityMl: 750,
            currentLevelMl: 420,
            dailyGoalMl: 2000,
            batteryPercent: 85
        )
    }
}
