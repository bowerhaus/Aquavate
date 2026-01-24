//
//  AquavateWatchApp.swift
//  AquavateWatch
//
//  Created by Claude Code on 22/01/2026.
//

import SwiftUI

@main
struct AquavateWatchApp: App {
    @StateObject private var sessionManager = WatchSessionManager.shared

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(sessionManager)
        }
    }
}
