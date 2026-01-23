//
//  AquavateApp.swift
//  Aquavate
//
//  Created by Andy Bower on 11/01/2026.
//

import SwiftUI
import CoreData
import BackgroundTasks

@main
struct AquavateApp: App {
    // Background task identifier
    static let hydrationCheckTaskIdentifier = "com.bowerhaus.Aquavate.hydrationCheck"
    @State private var showSplash = true
    @StateObject private var bleManager = BLEManager()
    @StateObject private var healthKitManager = HealthKitManager()
    @StateObject private var notificationManager = NotificationManager()
    @StateObject private var hydrationReminderService = HydrationReminderService()
    @Environment(\.scenePhase) private var scenePhase

    let persistenceController = PersistenceController.shared

    init() {
        registerBackgroundTasks()
    }

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
            // Schedule background hydration check
            scheduleHydrationCheck()

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

    // MARK: - Background Tasks

    private func registerBackgroundTasks() {
        BGTaskScheduler.shared.register(
            forTaskWithIdentifier: Self.hydrationCheckTaskIdentifier,
            using: nil
        ) { task in
            self.handleHydrationCheck(task: task as! BGAppRefreshTask)
        }
        print("[Background] Registered hydration check task")
    }

    private func scheduleHydrationCheck() {
        let request = BGAppRefreshTaskRequest(identifier: Self.hydrationCheckTaskIdentifier)
        // Request to run in 15 minutes (iOS may delay this)
        request.earliestBeginDate = Date(timeIntervalSinceNow: 15 * 60)

        do {
            try BGTaskScheduler.shared.submit(request)
            print("[Background] Scheduled hydration check for ~15 minutes")
        } catch {
            print("[Background] Failed to schedule hydration check: \(error)")
        }
    }

    private func handleHydrationCheck(task: BGAppRefreshTask) {
        print("[Background] Hydration check task started")

        // Schedule the next check before doing work
        scheduleHydrationCheck()

        // Set expiration handler
        task.expirationHandler = {
            print("[Background] Hydration check task expired")
            task.setTaskCompleted(success: false)
        }

        // Perform hydration check on main actor
        Task { @MainActor in
            // Get current state from CoreData
            let todaysTotalMl = PersistenceController.shared.getTodaysTotalMl()
            let dailyGoalMl = UserDefaults.standard.integer(forKey: "dailyGoalMl")
            let effectiveGoal = dailyGoalMl > 0 ? dailyGoalMl : 2500

            // Calculate deficit using the same logic as HydrationReminderService
            let deficit = calculateDeficit(totalMl: todaysTotalMl, goalMl: effectiveGoal)

            print("[Background] Check: total=\(todaysTotalMl)ml, goal=\(effectiveGoal)ml, deficit=\(deficit)ml")

            // Check if goal reached and notification not yet sent
            let goalNotificationSent = UserDefaults.standard.bool(forKey: "goalNotificationSentToday")
            if todaysTotalMl >= effectiveGoal && !goalNotificationSent {
                notificationManager.scheduleGoalReachedNotification()
                UserDefaults.standard.set(true, forKey: "goalNotificationSentToday")
                print("[Background] Goal reached! Sent celebration notification")
            }
            // Check if we should send a reminder notification (only if behind pace)
            else if shouldSendBackgroundNotification(deficit: deficit, goalMl: effectiveGoal, totalMl: todaysTotalMl) {
                let urgency = calculateUrgency(deficit: deficit, goalMl: effectiveGoal)
                notificationManager.scheduleHydrationReminder(urgency: urgency, deficitMl: deficit)
                print("[Background] Sent \(urgency.description) notification")
            }

            task.setTaskCompleted(success: true)
            print("[Background] Hydration check task completed")
        }
    }

    private func calculateDeficit(totalMl: Int, goalMl: Int) -> Int {
        let now = Date()
        let calendar = Calendar.current
        let hour = calendar.component(.hour, from: now)
        let minute = calendar.component(.minute, from: now)

        let activeHoursStart = 7
        let activeHoursEnd = 22
        let activeHoursDuration = 15

        // Before active hours: expect 0
        if hour < activeHoursStart { return 0 }

        // After active hours: expect full goal
        let expectedIntake: Int
        if hour >= activeHoursEnd {
            expectedIntake = goalMl
        } else {
            // During active hours: proportional based on elapsed time
            let elapsedMinutes = (hour - activeHoursStart) * 60 + minute
            let totalActiveMinutes = activeHoursDuration * 60
            expectedIntake = (elapsedMinutes * goalMl) / totalActiveMinutes
        }

        return max(0, expectedIntake - totalMl)
    }

    private func calculateUrgency(deficit: Int, goalMl: Int) -> HydrationUrgency {
        let attentionThreshold = 0.20  // 20% behind = red
        let redThresholdMl = Int(Double(goalMl) * attentionThreshold)

        if deficit >= redThresholdMl {
            return .overdue
        } else if deficit > 0 {
            return .attention
        }
        return .onTrack
    }

    private func shouldSendBackgroundNotification(deficit: Int, goalMl: Int, totalMl: Int) -> Bool {
        // Don't notify if goal achieved
        guard totalMl < goalMl else { return false }

        // Don't notify if on track
        guard deficit > 0 else { return false }

        // Don't notify during quiet hours (10pm - 7am)
        let calendar = Calendar.current
        let hour = calendar.component(.hour, from: Date())
        if hour >= 22 || hour < 7 { return false }

        // Check if we can send more reminders today
        guard notificationManager.canSendReminder else { return false }

        // Check if notifications are enabled and authorized
        guard notificationManager.isEnabled && notificationManager.isAuthorized else { return false }

        return true
    }
}
