# Plan: Add Import/Export (Issue #93)

## Summary

Add JSON backup export and import to the iOS app, accessible from Settings. Users can export all app data as a versioned JSON file and import it back via Merge (skip duplicates) or Replace (clear and restore) modes.

## Files to Create

1. **`ios/Aquavate/Aquavate/Models/BackupModels.swift`** — Codable structs for versioned JSON format
2. **`ios/Aquavate/Aquavate/Services/BackupManager.swift`** — Export/import logic (background CoreData operations)

## Files to Modify

1. **`ios/Aquavate/Aquavate/Views/SettingsView.swift`** — Add "Data" category row on main page + `DataSettingsPage`, `ImportPreviewSheet`, and `ShareSheet` private structs
2. **`ios/Aquavate/Aquavate/CoreData/PersistenceController.swift`** — Add entity count helper methods (optional, can use inline `context.count(for:)`)

## Implementation Steps

### Step 1: BackupModels.swift

Define the JSON schema as Codable structs:

- `AquavateBackup` — top-level container with `formatVersion: 1`, `appVersion`, `exportDate`, arrays of entity backups, and `settings`
- `BackupBottle` — id, name, capacityMl, dailyGoalMl, isCalibrated, scaleFactor, tareWeightGrams
- `BackupDrinkRecord` — id, bottleId, timestamp, amountMl, bottleLevelMl, drinkType, firmwareRecordId, syncedToHealth, healthKitSampleUUID (optional)
- `BackupMotionWakeEvent` — id, bottleId, timestamp, durationSec, wakeReason, sleepType, drinkTaken
- `BackupBackpackSession` — id, bottleId, startTimestamp, durationSec, timerWakeCount, exitReason
- `BackupSettings` — dailyGoalMl, hydrationRemindersEnabled, backOnTrackNotificationsEnabled, dailyReminderLimitEnabled, healthKitSyncEnabled
- `ImportMode` enum (merge/replace), `ImportResult`, `BackupSummary`, `BackupError`

**Excluded fields** (device-specific or ephemeral):
- `peripheralIdentifier` — BLE device ID, meaningless on a different phone
- `batteryPercent`, `lastSyncDate`, `lastActivitySyncDate` — ephemeral state

**HealthKit handling:** Both `syncedToHealth` and `healthKitSampleUUID` are preserved in the backup. This ensures same-device restore doesn't create HealthKit duplicates and the app retains the ability to manage (e.g. delete) existing HealthKit samples.

### Step 2: BackupManager.swift

`@MainActor class BackupManager: ObservableObject` following existing service pattern.

**Export (`createBackup()`):**
- Fetch all 4 entity types on a background context via `container.newBackgroundContext()`
- Map CoreData objects to Codable backup structs
- Capture UserDefaults settings
- Encode with `JSONEncoder` (ISO 8601 dates, pretty printed, sorted keys)
- Return `(Data, filename)` where filename = `Aquavate-Backup-YYYY-MM-DD.json`

**Preview (`parseBackup(from:)`):**
- Decode version first to check compatibility (reject newer format versions)
- Full decode to `AquavateBackup`
- Return backup + `BackupSummary` for preview UI

**Import (`importBackup(_:mode:)`):**
- Background context with `NSMergeByPropertyObjectTrumpMergePolicy`
- Replace mode: batch delete all entities first via `NSBatchDeleteRequest`
- Merge mode: check UUID existence before inserting each record, skip duplicates
- Save context, then restore UserDefaults settings on main thread
- Return `ImportResult` with counts

### Step 3: SettingsView.swift

**Main page — add "Data" category row** (between Reminder Options and About):
- Icon: `externaldrive.fill` (green)
- Title: "Data"
- Summary: e.g. "42 drink records" (uses count query)
- NavigationLink to `DataSettingsPage`

**DataSettingsPage** (private struct, matching existing sub-page pattern):
- "Your Data" section: count rows for bottles, drink records, wake events, backpack sessions
- "Export" section: Export Backup button with spinner + footer text
- "Import" section: Import Backup button with spinner + footer text
- Uses `.fileImporter(allowedContentTypes: [.json])` for import
- Uses `.sheet` for share via `UIActivityViewController` wrapper

**ImportPreviewSheet** (private struct, presented as `.sheet`):
- Shows backup info: export date, app version, format version
- Shows content counts with icons matching the data page
- Import Mode section with two buttons:
  - **Merge** — "Add new records, skip duplicates"
  - **Replace All** — destructive button, triggers confirmation alert before proceeding

**ShareSheet** (private struct):
- Simple `UIViewControllerRepresentable` wrapping `UIActivityViewController`

### Step 4: Build & Test

- Build: `cd ios/Aquavate && xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`
- Verify export produces valid JSON with correct filename and all entity data
- Verify import merge mode skips duplicates and adds new records
- Verify import replace mode clears all data before importing
- Verify error handling for corrupt files and newer format versions
- Verify share sheet presents correctly
- Verify file importer opens document picker for JSON files

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Export mechanism | `UIActivityViewController` wrapper | More reliable for file sharing with custom filenames than `ShareLink` |
| Import mechanism | `.fileImporter()` modifier | Native SwiftUI, constrained to `.json` type |
| Date encoding | ISO 8601 | Human readable, unambiguous, matches existing `HydrationState` pattern |
| Merge dedup | UUID match | Simple, reliable. CoreData uniqueness constraints are safety net |
| Replace scope | All data (bottles + all records) | Clean restore including calibration data from backup |
| HealthKit fields on import | Preserve from backup | Same-device restore is primary use case; avoids HealthKit duplicates |
| UI placement | New "Data" category row on main settings page | Follows Option 5 pattern, gives import/export its own sub-page |
| Threading | Background context for all CoreData ops | Keeps UI responsive during large imports/exports |
| Format versioning | Integer version in JSON, reject newer versions | Future-proofs the format with clear upgrade path |

## Edge Cases

- **Newer format version**: Rejected with "Please update the app" message
- **Corrupt JSON**: Decoder error caught and displayed
- **Empty backup**: Valid — shows "No new data to import"
- **100% duplicates in merge**: All skipped, clear message shown
- **App killed during import**: CoreData background save is atomic; settings applied after save
- **Large datasets**: Background context + progress spinner keeps UI responsive
