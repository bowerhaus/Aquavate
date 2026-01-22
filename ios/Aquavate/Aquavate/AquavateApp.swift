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
    @StateObject private var notificationManager = NotificationManager()
    @StateObject private var hydrationReminderService = HydrationReminderService()
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
                    .environmentObject(notificationManager)
                    .environmentObject(hydrationReminderService)
                    .onAppear {
                        // Wire up HealthKitManager to BLEManager for drink sync
                        bleManager.healthKitManager = healthKitManager
                        // Wire up HydrationReminderService
                        hydrationReminderService.notificationManager = notificationManager
                        bleManager.hydrationReminderService = hydrationReminderService
                        // Initial sync to Watch with current CoreData state
                        hydrationReminderService.syncCurrentStateFromCoreData(dailyGoalMl: bleManager.dailyGoalMl)
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
            // Let BLEManager handle background behavior
            // Don't force disconnect - allow state restoration to work
            bleManager.appDidEnterBackground()

        case .active:
            // Attempt auto-reconnection when app becomes active
            bleManager.appDidBecomeActive()
            // Sync current state to Watch (in case drinks were synced while backgrounded)
            hydrationReminderService.syncCurrentStateFromCoreData(dailyGoalMl: bleManager.dailyGoalMl)

        case .inactive:
            // Transitional state, no action needed
            break

        @unknown default:
            break
        }
    }
}
