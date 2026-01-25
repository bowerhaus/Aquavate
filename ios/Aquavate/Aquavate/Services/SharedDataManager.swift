//
//  SharedDataManager.swift
//  Aquavate
//
//  Created by Claude Code on 22/01/2026.
//

import Foundation

/// Manages shared data between iPhone app and Apple Watch via App Groups
class SharedDataManager {
    static let shared = SharedDataManager()

    private let appGroupId = "group.com.bowerhaus.Aquavate"
    private let hydrationStateKey = "hydrationState"

    private var sharedDefaults: UserDefaults? {
        UserDefaults(suiteName: appGroupId)
    }

    private init() {}

    // MARK: - Hydration State

    /// Save current hydration state to shared container
    func saveHydrationState(_ state: HydrationState) {
        guard let defaults = sharedDefaults else {
            print("[SharedData] Failed to access App Group container")
            return
        }

        do {
            let data = try JSONEncoder().encode(state)
            defaults.set(data, forKey: hydrationStateKey)
            print("[SharedData] Saved hydration state: \(state.dailyTotalMl)ml, urgency=\(state.urgency)")
        } catch {
            print("[SharedData] Failed to encode hydration state: \(error)")
        }
    }

    /// Load hydration state from shared container
    func loadHydrationState() -> HydrationState? {
        guard let defaults = sharedDefaults,
              let data = defaults.data(forKey: hydrationStateKey) else {
            return nil
        }

        do {
            let state = try JSONDecoder().decode(HydrationState.self, from: data)
            return state
        } catch {
            print("[SharedData] Failed to decode hydration state: \(error)")
            return nil
        }
    }

    /// Clear all shared data (for testing)
    func clearAll() {
        guard let defaults = sharedDefaults else { return }
        defaults.removeObject(forKey: hydrationStateKey)
        print("[SharedData] Cleared all shared data")
    }
}
