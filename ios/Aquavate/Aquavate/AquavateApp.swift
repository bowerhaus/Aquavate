//
//  AquavateApp.swift
//  Aquavate
//
//  Created by Andy Bower on 11/01/2026.
//

import SwiftUI
import CoreData

@main
struct AquavateApp: App {
    @State private var showSplash = true
    @StateObject private var bleManager = BLEManager()
    @StateObject private var healthKitManager = HealthKitManager()
    @Environment(\.scenePhase) private var scenePhase

    let persistenceController = PersistenceController.shared

    var body: some Scene {
        WindowGroup {
            if showSplash {
                SplashView()
                    .onAppear {
                        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                            withAnimation(.easeInOut(duration: 0.5)) {
                                showSplash = false
                            }
                        }
                    }
            } else {
                ContentView()
                    .environment(\.managedObjectContext, persistenceController.viewContext)
                    .environmentObject(bleManager)
                    .environmentObject(healthKitManager)
                    .onAppear {
                        // Wire up HealthKitManager to BLEManager for drink sync
                        bleManager.healthKitManager = healthKitManager
                    }
            }
        }
        .onChange(of: scenePhase) { oldPhase, newPhase in
            handleScenePhaseChange(from: oldPhase, to: newPhase)
        }
    }

    private func handleScenePhaseChange(from oldPhase: ScenePhase, to newPhase: ScenePhase) {
        switch newPhase {
        case .background:
            // Disconnect immediately when app goes to background to allow bottle to sleep
            if bleManager.connectionState.isConnected {
                bleManager.disconnect()
            }
        case .active:
            // User should use pull-to-refresh to connect
            // Don't auto-reconnect on foreground
            break
        case .inactive:
            // Transitional state, no action needed
            break
        @unknown default:
            break
        }
    }
}
