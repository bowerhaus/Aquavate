//
//  PersistenceController.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import CoreData

struct PersistenceController {
    static let shared = PersistenceController()

    static var preview: PersistenceController = {
        let result = PersistenceController(inMemory: true)
        let viewContext = result.container.viewContext

        // Create a sample bottle
        let bottle = CDBottle(context: viewContext)
        bottle.id = UUID()
        bottle.name = "My Bottle"
        bottle.capacityMl = 750
        bottle.dailyGoalMl = 2000
        bottle.batteryPercent = 85
        bottle.isCalibrated = true
        bottle.scaleFactor = 1.0
        bottle.tareWeightGrams = 250

        // Create sample drink records for today
        let now = Date()
        let calendar = Calendar.current

        let sampleDrinks: [(minutesAgo: Int, amountMl: Int16, levelMl: Int16, type: Int16)] = [
            (15, 180, 420, 0),    // 15 minutes ago, gulp
            (120, 150, 600, 0),   // 2 hours ago, gulp
            (240, 200, 750, 1),   // 4 hours ago, pour
            (360, 250, 700, 0),   // 6 hours ago, gulp
            (480, 180, 450, 0),   // 8 hours ago, gulp
        ]

        for drink in sampleDrinks {
            let record = CDDrinkRecord(context: viewContext)
            record.id = UUID()
            record.timestamp = calendar.date(byAdding: .minute, value: -drink.minutesAgo, to: now)
            record.amountMl = drink.amountMl
            record.bottleLevelMl = drink.levelMl
            record.drinkType = drink.type
            record.syncedToHealth = false
            record.bottleId = bottle.id
            record.bottle = bottle
        }

        // Add some drinks from yesterday
        let yesterdayDrinks: [(hoursAgo: Int, amountMl: Int16, levelMl: Int16)] = [
            (24, 220, 230),
            (28, 180, 450),
            (30, 200, 630),
            (33, 150, 750),
        ]

        for drink in yesterdayDrinks {
            let record = CDDrinkRecord(context: viewContext)
            record.id = UUID()
            record.timestamp = calendar.date(byAdding: .hour, value: -drink.hoursAgo, to: now)
            record.amountMl = drink.amountMl
            record.bottleLevelMl = drink.levelMl
            record.drinkType = 0
            record.syncedToHealth = false
            record.bottleId = bottle.id
            record.bottle = bottle
        }

        do {
            try viewContext.save()
        } catch {
            let nsError = error as NSError
            fatalError("Unresolved error \(nsError), \(nsError.userInfo)")
        }

        return result
    }()

    let container: NSPersistentContainer

    init(inMemory: Bool = false) {
        container = NSPersistentContainer(name: "Aquavate")

        if inMemory {
            container.persistentStoreDescriptions.first!.url = URL(fileURLWithPath: "/dev/null")
        }

        container.loadPersistentStores { (storeDescription, error) in
            if let error = error as NSError? {
                // Replace this implementation with code to handle the error appropriately.
                // fatalError() causes the application to generate a crash log and terminate.
                // You should not use this function in a shipping application, although it may
                // be useful during development.
                fatalError("Unresolved error \(error), \(error.userInfo)")
            }
        }

        // Configure merge policy to handle uniqueness constraint violations
        // NSMergeByPropertyObjectTrumpMergePolicy: In-memory changes trump persistent store
        // This means when we try to insert a duplicate (same timestamp+bottleId),
        // the existing record is updated instead of creating a duplicate
        container.viewContext.mergePolicy = NSMergeByPropertyObjectTrumpMergePolicy
        container.viewContext.automaticallyMergesChangesFromParent = true
    }

    // MARK: - Convenience Methods

    var viewContext: NSManagedObjectContext {
        container.viewContext
    }

    func save() {
        let context = container.viewContext
        if context.hasChanges {
            do {
                try context.save()
            } catch {
                let nsError = error as NSError
                print("[CoreData] Save error: \(nsError), \(nsError.userInfo)")
            }
        }
    }

    // MARK: - Bottle Operations

    func getOrCreateDefaultBottle() -> CDBottle {
        let request: NSFetchRequest<CDBottle> = CDBottle.fetchRequest()
        request.fetchLimit = 1

        do {
            let results = try viewContext.fetch(request)
            if let bottle = results.first {
                return bottle
            }
        } catch {
            print("[CoreData] Error fetching bottle: \(error)")
        }

        // Create default bottle if none exists
        let bottle = CDBottle(context: viewContext)
        bottle.id = UUID()
        bottle.name = "My Bottle"
        bottle.capacityMl = 750
        bottle.dailyGoalMl = 2000
        bottle.batteryPercent = 0
        bottle.isCalibrated = false
        save()

        return bottle
    }

    // MARK: - Drink Record Operations

    func saveDrinkRecords(_ bleRecords: [BLEDrinkRecord], for bottleId: UUID) {
        let context = viewContext

        for bleRecord in bleRecords {
            let record = CDDrinkRecord(context: context)
            record.id = UUID()
            let drinkDate = Date(timeIntervalSince1970: TimeInterval(bleRecord.timestamp))
            record.timestamp = drinkDate
            record.amountMl = bleRecord.amountMl
            // Interpret UInt16 as signed Int16 (firmware sends signed values over BLE)
            record.bottleLevelMl = Int16(bitPattern: bleRecord.bottleLevelMl)
            record.drinkType = Int16(bleRecord.type)
            record.syncedToHealth = false
            record.bottleId = bottleId
            // Store firmware record ID for bidirectional sync
            record.firmwareRecordId = Int32(bleRecord.recordId)

            print("[CoreData] Creating record: id=\(bleRecord.recordId), timestamp=\(bleRecord.timestamp) (\(drinkDate)), amount=\(bleRecord.amountMl)ml")
        }

        do {
            try context.save()
            print("[CoreData] Saved \(bleRecords.count) drink records for bottle \(bottleId)")

            // Force SwiftUI to refresh @FetchRequest results by processing pending changes
            context.processPendingChanges()
        } catch {
            print("[CoreData] Save error: \(error)")
            context.rollback()
        }
    }

    func getTodaysDrinkRecords() -> [CDDrinkRecord] {
        let request: NSFetchRequest<CDDrinkRecord> = CDDrinkRecord.fetchRequest()
        // Use 4am boundary to match firmware DRINK_DAILY_RESET_HOUR
        let startOfDay = Calendar.current.startOfAquavateDay(for: Date())
        request.predicate = NSPredicate(format: "timestamp >= %@", startOfDay as CVarArg)
        request.sortDescriptors = [NSSortDescriptor(keyPath: \CDDrinkRecord.timestamp, ascending: false)]

        do {
            return try viewContext.fetch(request)
        } catch {
            print("[CoreData] Error fetching today's drinks: \(error)")
            return []
        }
    }

    func getTodaysTotalMl() -> Int {
        let todaysDrinks = getTodaysDrinkRecords()
        return todaysDrinks.reduce(0) { $0 + Int($1.amountMl) }
    }

    func getAllDrinkRecords() -> [CDDrinkRecord] {
        let request: NSFetchRequest<CDDrinkRecord> = CDDrinkRecord.fetchRequest()
        request.sortDescriptors = [NSSortDescriptor(keyPath: \CDDrinkRecord.timestamp, ascending: false)]

        do {
            return try viewContext.fetch(request)
        } catch {
            print("[CoreData] Error fetching all drinks: \(error)")
            return []
        }
    }

    func deleteAllDrinkRecords() {
        let request: NSFetchRequest<NSFetchRequestResult> = CDDrinkRecord.fetchRequest()
        let deleteRequest = NSBatchDeleteRequest(fetchRequest: request)

        do {
            try viewContext.execute(deleteRequest)
            try viewContext.save()
            print("[CoreData] Deleted all drink records")
        } catch {
            print("[CoreData] Error deleting drink records: \(error)")
        }
    }

    /// Delete a single drink record by ID
    func deleteDrinkRecord(id: UUID) {
        let request: NSFetchRequest<CDDrinkRecord> = CDDrinkRecord.fetchRequest()
        request.predicate = NSPredicate(format: "id == %@", id as CVarArg)
        request.fetchLimit = 1

        do {
            let results = try viewContext.fetch(request)
            if let record = results.first {
                viewContext.delete(record)
                try viewContext.save()
                viewContext.processPendingChanges()
                print("[CoreData] Deleted drink record: \(id)")
            }
        } catch {
            print("[CoreData] Error deleting drink record: \(error)")
        }
    }

    /// Get a drink record by its firmware record ID
    func getDrinkRecord(byFirmwareId firmwareRecordId: Int32) -> CDDrinkRecord? {
        let request: NSFetchRequest<CDDrinkRecord> = CDDrinkRecord.fetchRequest()
        request.predicate = NSPredicate(format: "firmwareRecordId == %d", firmwareRecordId)
        request.fetchLimit = 1

        do {
            return try viewContext.fetch(request).first
        } catch {
            print("[CoreData] Error fetching drink record by firmware ID: \(error)")
            return nil
        }
    }

    /// Delete all drink records from today
    func deleteTodaysDrinkRecords() {
        // Use 4am boundary to match firmware DRINK_DAILY_RESET_HOUR
        let startOfDay = Calendar.current.startOfAquavateDay(for: Date())
        let request: NSFetchRequest<CDDrinkRecord> = CDDrinkRecord.fetchRequest()
        request.predicate = NSPredicate(format: "timestamp >= %@", startOfDay as CVarArg)

        do {
            let records = try viewContext.fetch(request)
            let count = records.count
            for record in records {
                viewContext.delete(record)
            }
            try viewContext.save()
            viewContext.processPendingChanges()
            print("[CoreData] Deleted \(count) today's drink records")
        } catch {
            print("[CoreData] Error deleting today's drink records: \(error)")
        }
    }

    // MARK: - HealthKit Sync Operations

    /// Mark a drink record as synced to HealthKit and store the HealthKit sample UUID
    func markDrinkSyncedToHealth(id: UUID, healthKitUUID: UUID) {
        let request: NSFetchRequest<CDDrinkRecord> = CDDrinkRecord.fetchRequest()
        request.predicate = NSPredicate(format: "id == %@", id as CVarArg)
        request.fetchLimit = 1

        do {
            let results = try viewContext.fetch(request)
            if let record = results.first {
                record.syncedToHealth = true
                record.healthKitSampleUUID = healthKitUUID
                try viewContext.save()
                print("[CoreData] Marked drink \(id) as synced to HealthKit with UUID \(healthKitUUID)")
            }
        } catch {
            print("[CoreData] Error marking drink as synced to health: \(error)")
        }
    }

    /// Get the HealthKit sample UUID for a drink record before deletion
    func getHealthKitUUID(forDrinkId id: UUID) -> UUID? {
        let request: NSFetchRequest<CDDrinkRecord> = CDDrinkRecord.fetchRequest()
        request.predicate = NSPredicate(format: "id == %@", id as CVarArg)
        request.fetchLimit = 1

        do {
            let results = try viewContext.fetch(request)
            return results.first?.healthKitSampleUUID
        } catch {
            print("[CoreData] Error fetching HealthKit UUID: \(error)")
            return nil
        }
    }

    /// Get all drink records that haven't been synced to HealthKit
    func getUnsyncedDrinkRecords() -> [CDDrinkRecord] {
        let request: NSFetchRequest<CDDrinkRecord> = CDDrinkRecord.fetchRequest()
        request.predicate = NSPredicate(format: "syncedToHealth == NO")
        request.sortDescriptors = [NSSortDescriptor(keyPath: \CDDrinkRecord.timestamp, ascending: true)]

        do {
            return try viewContext.fetch(request)
        } catch {
            print("[CoreData] Error fetching unsynced drinks: \(error)")
            return []
        }
    }
}

// MARK: - CDDrinkRecord Extension for DrinkRecord Conversion

extension CDDrinkRecord {
    func toDrinkRecord() -> DrinkRecord {
        DrinkRecord(
            id: id ?? UUID(),
            timestamp: timestamp ?? Date(),
            amountMl: Int(amountMl),
            bottleLevelMl: Int(bottleLevelMl)
        )
    }
}

// MARK: - CDBottle Extension for Bottle Conversion

extension CDBottle {
    func toBottle(currentLevelMl: Int) -> Bottle {
        Bottle(
            name: name ?? "My Bottle",
            capacityMl: Int(capacityMl),
            currentLevelMl: currentLevelMl,
            dailyGoalMl: Int(dailyGoalMl),
            batteryPercent: Int(batteryPercent)
        )
    }
}
