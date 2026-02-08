//
//  BackupModels.swift
//  Aquavate
//
//  Created by Claude Code on 29/01/2026.
//

import Foundation

// MARK: - Top-Level Backup Container

struct AquavateBackup: Codable {
    let formatVersion: Int
    let appVersion: String
    let exportDate: Date
    let bottles: [BackupBottle]
    let drinkRecords: [BackupDrinkRecord]
    let motionWakeEvents: [BackupMotionWakeEvent]
    let backpackSessions: [BackupBackpackSession]
    let settings: BackupSettings

    static let currentFormatVersion = 1
}

// MARK: - Entity Backup Structs

struct BackupBottle: Codable {
    let id: UUID
    let name: String
    let capacityMl: Int16
    let dailyGoalMl: Int16
    let isCalibrated: Bool
    let scaleFactor: Float
    let tareWeightGrams: Int32
}

struct BackupDrinkRecord: Codable {
    let id: UUID
    let bottleId: UUID
    let timestamp: Date
    let amountMl: Int16
    let bottleLevelMl: Int16
    let drinkType: Int16
    let firmwareRecordId: Int32
    let syncedToHealth: Bool
    let healthKitSampleUUID: UUID?
}

struct BackupMotionWakeEvent: Codable {
    let id: UUID
    let bottleId: UUID
    let timestamp: Date
    let durationSec: Int32
    let wakeReason: Int16
    let sleepType: Int16
    let drinkTaken: Bool
}

struct BackupBackpackSession: Codable {
    let id: UUID
    let bottleId: UUID
    let startTimestamp: Date
    let durationSec: Int32
    let timerWakeCount: Int32
    let exitReason: Int16
}

struct BackupSettings: Codable {
    let dailyGoalMl: Int?
    let hydrationRemindersEnabled: Bool
    let backOnTrackNotificationsEnabled: Bool
    let dailyReminderLimitEnabled: Bool
    let healthKitSyncEnabled: Bool
}

// MARK: - Import Mode

enum ImportMode {
    case merge
    case replace
}

// MARK: - Import Result

struct ImportResult {
    let bottlesImported: Int
    let drinksImported: Int
    let motionEventsImported: Int
    let backpackSessionsImported: Int
    let settingsRestored: Bool
    let duplicatesSkipped: Int
}

// MARK: - Backup Summary

struct BackupSummary {
    let formatVersion: Int
    let appVersion: String
    let exportDate: Date
    let bottleCount: Int
    let drinkRecordCount: Int
    let motionWakeEventCount: Int
    let backpackSessionCount: Int
    let hasSettings: Bool
}

// MARK: - Backup Error

enum BackupError: LocalizedError {
    case newerVersion(Int)
    case corruptData
    case exportFailed(String)
    case importFailed(String)

    var errorDescription: String? {
        switch self {
        case .newerVersion(let version):
            return "This backup was created with a newer version of Aquavate (format v\(version)). Please update the app to import it."
        case .corruptData:
            return "The backup file is corrupt or not a valid Aquavate backup."
        case .exportFailed(let detail):
            return "Export failed: \(detail)"
        case .importFailed(let detail):
            return "Import failed: \(detail)"
        }
    }
}
