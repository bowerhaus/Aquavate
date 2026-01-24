//
//  AquavateWatchApp.swift
//  AquavateWatch
//
//  Created by Claude Code on 22/01/2026.
//

import SwiftUI

@main
struct AquavateWatchApp: App {
    init() {
        // Request notification permission for iPhone-triggered local notifications
        Task {
            try? await WatchNotificationManager.shared.requestAuthorization()
        }
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(WatchSessionManager.shared)
        }
    }
}
