//
//  BackupManager.swift
//  Aquavate
//
//  Created by Claude Code on 29/01/2026.
//

import Foundation
import CoreData

@MainActor
class BackupManager: ObservableObject {

    // MARK: - Published State

    @Published var isExporting = false
    @Published var isImporting = false

    // MARK: - Export

    /// Create a full backup as JSON Data and a suggested filename.
    func createBackup() async throws -> (data: Data, filename: String) {
        isExporting = true
        defer { isExporting = false }

        let context = PersistenceController.shared.container.newBackgroundContext()

        let backup = try await context.perform {
            let bottleRequest: NSFetchRequest<CDBottle> = CDBottle.fetchRequest()
            let bottles = try context.fetch(bottleRequest)

            let drinkRequest: NSFetchRequest<CDDrinkRecord> = CDDrinkRecord.fetchRequest()
            let drinks = try context.fetch(drinkRequest)

            let motionRequest: NSFetchRequest<CDMotionWakeEvent> = CDMotionWakeEvent.fetchRequest()
            let motionEvents = try context.fetch(motionRequest)

            let backpackRequest: NSFetchRequest<CDBackpackSession> = CDBackpackSession.fetchRequest()
            let backpackSessions = try context.fetch(backpackRequest)

            let backupBottles = bottles.map { bottle in
                BackupBottle(
                    id: bottle.id ?? UUID(),
                    name: bottle.name ?? "My Bottle",
                    capacityMl: bottle.capacityMl,
                    dailyGoalMl: bottle.dailyGoalMl,
                    isCalibrated: bottle.isCalibrated,
                    scaleFactor: bottle.scaleFactor,
                    tareWeightGrams: bottle.tareWeightGrams
                )
            }

            let backupDrinks = drinks.map { drink in
                BackupDrinkRecord(
                    id: drink.id ?? UUID(),
                    bottleId: drink.bottleId ?? UUID(),
                    timestamp: drink.timestamp ?? Date.distantPast,
                    amountMl: drink.amountMl,
                    bottleLevelMl: drink.bottleLevelMl,
                    drinkType: drink.drinkType,
                    firmwareRecordId: drink.firmwareRecordId,
                    syncedToHealth: drink.syncedToHealth,
                    healthKitSampleUUID: drink.healthKitSampleUUID
                )
            }

            let backupMotion = motionEvents.map { event in
                BackupMotionWakeEvent(
                    id: event.id ?? UUID(),
                    bottleId: event.bottleId ?? UUID(),
                    timestamp: event.timestamp ?? Date.distantPast,
                    durationSec: event.durationSec,
                    wakeReason: event.wakeReason,
                    sleepType: event.sleepType,
                    drinkTaken: event.drinkTaken
                )
            }

            let backupBackpack = backpackSessions.map { session in
                BackupBackpackSession(
                    id: session.id ?? UUID(),
                    bottleId: session.bottleId ?? UUID(),
                    startTimestamp: session.startTimestamp ?? Date.distantPast,
                    durationSec: session.durationSec,
                    timerWakeCount: session.timerWakeCount,
                    exitReason: session.exitReason
                )
            }

            let settings = BackupSettings(
                dailyGoalMl: UserDefaults.standard.object(forKey: "lastKnownDailyGoalMl") as? Int,
                hydrationRemindersEnabled: UserDefaults.standard.bool(forKey: "hydrationRemindersEnabled"),
                backOnTrackNotificationsEnabled: UserDefaults.standard.bool(forKey: "backOnTrackNotificationsEnabled"),
                dailyReminderLimitEnabled: UserDefaults.standard.object(forKey: "dailyReminderLimitEnabled") as? Bool ?? true,
                healthKitSyncEnabled: UserDefaults.standard.bool(forKey: "healthKitSyncEnabled")
            )

            let appVersion = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "unknown"

            return AquavateBackup(
                formatVersion: AquavateBackup.currentFormatVersion,
                appVersion: appVersion,
                exportDate: Date(),
                bottles: backupBottles,
                drinkRecords: backupDrinks,
                motionWakeEvents: backupMotion,
                backpackSessions: backupBackpack,
                settings: settings
            )
        }

        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let data = try encoder.encode(backup)

        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "yyyy-MM-dd"
        let filename = "Aquavate-Backup-\(dateFormatter.string(from: Date())).json"

        return (data, filename)
    }

    // MARK: - Parse / Preview

    /// Parse a backup file and return the decoded backup and a summary for preview.
    func parseBackup(from data: Data) throws -> (backup: AquavateBackup, summary: BackupSummary) {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601

        // Check version compatibility first
        struct VersionCheck: Decodable { let formatVersion: Int }
        let versionCheck = try decoder.decode(VersionCheck.self, from: data)

        guard versionCheck.formatVersion <= AquavateBackup.currentFormatVersion else {
            throw BackupError.newerVersion(versionCheck.formatVersion)
        }

        let backup = try decoder.decode(AquavateBackup.self, from: data)

        let summary = BackupSummary(
            formatVersion: backup.formatVersion,
            appVersion: backup.appVersion,
            exportDate: backup.exportDate,
            bottleCount: backup.bottles.count,
            drinkRecordCount: backup.drinkRecords.count,
            motionWakeEventCount: backup.motionWakeEvents.count,
            backpackSessionCount: backup.backpackSessions.count,
            hasSettings: true
        )

        return (backup, summary)
    }

    // MARK: - Import

    /// Import a backup with the specified mode.
    func importBackup(_ backup: AquavateBackup, mode: ImportMode) async throws -> ImportResult {
        isImporting = true
        defer { isImporting = false }

        let context = PersistenceController.shared.container.newBackgroundContext()
        context.mergePolicy = NSMergeByPropertyObjectTrumpMergePolicy

        let result = try await context.perform {
            if mode == .replace {
                try self.deleteAllData(in: context)
            }

            var duplicatesSkipped = 0

            // Import bottles
            var bottlesImported = 0
            for backupBottle in backup.bottles {
                if mode == .merge {
                    if try self.entityExists(CDBottle.self, withId: backupBottle.id, in: context) {
                        duplicatesSkipped += 1
                        continue
                    }
                }
                let bottle = CDBottle(context: context)
                bottle.id = backupBottle.id
                bottle.name = backupBottle.name
                bottle.capacityMl = backupBottle.capacityMl
                bottle.dailyGoalMl = backupBottle.dailyGoalMl
                bottle.isCalibrated = backupBottle.isCalibrated
                bottle.scaleFactor = backupBottle.scaleFactor
                bottle.tareWeightGrams = backupBottle.tareWeightGrams
                bottle.batteryPercent = 0
                bottlesImported += 1
            }

            // Import drink records
            var drinksImported = 0
            for backupDrink in backup.drinkRecords {
                if mode == .merge {
                    if try self.entityExists(CDDrinkRecord.self, withId: backupDrink.id, in: context) {
                        duplicatesSkipped += 1
                        continue
                    }
                }
                let record = CDDrinkRecord(context: context)
                record.id = backupDrink.id
                record.bottleId = backupDrink.bottleId
                record.timestamp = backupDrink.timestamp
                record.amountMl = backupDrink.amountMl
                record.bottleLevelMl = backupDrink.bottleLevelMl
                record.drinkType = backupDrink.drinkType
                record.firmwareRecordId = backupDrink.firmwareRecordId
                record.syncedToHealth = backupDrink.syncedToHealth
                record.healthKitSampleUUID = backupDrink.healthKitSampleUUID
                drinksImported += 1
            }

            // Import motion wake events
            var motionEventsImported = 0
            for backupEvent in backup.motionWakeEvents {
                if mode == .merge {
                    if try self.entityExists(CDMotionWakeEvent.self, withId: backupEvent.id, in: context) {
                        duplicatesSkipped += 1
                        continue
                    }
                }
                let event = CDMotionWakeEvent(context: context)
                event.id = backupEvent.id
                event.bottleId = backupEvent.bottleId
                event.timestamp = backupEvent.timestamp
                event.durationSec = backupEvent.durationSec
                event.wakeReason = backupEvent.wakeReason
                event.sleepType = backupEvent.sleepType
                event.drinkTaken = backupEvent.drinkTaken
                motionEventsImported += 1
            }

            // Import backpack sessions
            var backpackSessionsImported = 0
            for backupSession in backup.backpackSessions {
                if mode == .merge {
                    if try self.entityExists(CDBackpackSession.self, withId: backupSession.id, in: context) {
                        duplicatesSkipped += 1
                        continue
                    }
                }
                let session = CDBackpackSession(context: context)
                session.id = backupSession.id
                session.bottleId = backupSession.bottleId
                session.startTimestamp = backupSession.startTimestamp
                session.durationSec = backupSession.durationSec
                session.timerWakeCount = backupSession.timerWakeCount
                session.exitReason = backupSession.exitReason
                backpackSessionsImported += 1
            }

            try context.save()

            return ImportResult(
                bottlesImported: bottlesImported,
                drinksImported: drinksImported,
                motionEventsImported: motionEventsImported,
                backpackSessionsImported: backpackSessionsImported,
                settingsRestored: true,
                duplicatesSkipped: duplicatesSkipped
            )
        }

        // Restore UserDefaults settings after CoreData save succeeds
        if let goal = backup.settings.dailyGoalMl {
            UserDefaults.standard.set(goal, forKey: "lastKnownDailyGoalMl")
        }
        UserDefaults.standard.set(backup.settings.hydrationRemindersEnabled, forKey: "hydrationRemindersEnabled")
        UserDefaults.standard.set(backup.settings.backOnTrackNotificationsEnabled, forKey: "backOnTrackNotificationsEnabled")
        UserDefaults.standard.set(backup.settings.dailyReminderLimitEnabled, forKey: "dailyReminderLimitEnabled")
        UserDefaults.standard.set(backup.settings.healthKitSyncEnabled, forKey: "healthKitSyncEnabled")

        // Merge background changes into the view context
        PersistenceController.shared.container.viewContext.processPendingChanges()

        return result
    }

    // MARK: - Private Helpers

    private func entityExists<T: NSManagedObject>(_ type: T.Type, withId id: UUID, in context: NSManagedObjectContext) throws -> Bool {
        let request = T.fetchRequest()
        request.predicate = NSPredicate(format: "id == %@", id as CVarArg)
        request.fetchLimit = 1
        return try context.count(for: request) > 0
    }

    private func deleteAllData(in context: NSManagedObjectContext) throws {
        let entityNames = ["CDDrinkRecord", "CDMotionWakeEvent", "CDBackpackSession", "CDBottle"]

        for entityName in entityNames {
            let fetchRequest = NSFetchRequest<NSFetchRequestResult>(entityName: entityName)
            let deleteRequest = NSBatchDeleteRequest(fetchRequest: fetchRequest)
            deleteRequest.resultType = .resultTypeObjectIDs

            let result = try context.execute(deleteRequest) as? NSBatchDeleteResult
            if let objectIDs = result?.result as? [NSManagedObjectID] {
                let changes = [NSDeletedObjectsKey: objectIDs]
                NSManagedObjectContext.mergeChanges(fromRemoteContextSave: changes, into: [PersistenceController.shared.container.viewContext])
            }
        }
    }
}
