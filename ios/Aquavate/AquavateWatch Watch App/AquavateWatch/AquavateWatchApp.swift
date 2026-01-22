//
//  AquavateWatchApp.swift
//  AquavateWatch
//
//  Created by Claude Code on 22/01/2026.
//

import SwiftUI

@main
struct AquavateWatchApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(WatchSessionManager.shared)
        }
    }
}
