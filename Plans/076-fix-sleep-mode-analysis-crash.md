# Plan: Fix App Crash in Sleep Mode Analysis (Issue #105) ✅ COMPLETE

## Context

The iOS app crashes with `Fatal error: Not enough bits to represent the passed value` when saving backpack session data to CoreData. The crash occurs because `timerWakeCount` arrives from the firmware as `UInt16` (range 0–65,535) but is stored in CoreData as `Integer 16` (`Int16`, max 32,767). When a backpack session accumulates >32,767 timer wakes, `Int16(session.timerWakeCount)` triggers a Swift runtime crash.

A secondary unsafe conversion also exists: `CDMotionWakeEvent.durationSec` is `Integer 16` but receives `UInt16` from firmware. While less likely to overflow (would need >9 hours awake), it's the same class of bug.

## Changes

### 1. Create CoreData model version 2

Create `Aquavate 2.xcdatamodel` with widened types (CoreData lightweight migration handles Int16→Int32 automatically via `NSPersistentContainer`):

- `CDBackpackSession.timerWakeCount`: Integer 16 → **Integer 32**
- `CDMotionWakeEvent.durationSec`: Integer 16 → **Integer 32**

Update `.xccurrentversion` to point to `Aquavate 2`.

**Files:**
- New: `ios/Aquavate/Aquavate/CoreData/Aquavate.xcdatamodeld/Aquavate 2.xcdatamodel/contents`
- Edit: `ios/Aquavate/Aquavate/CoreData/Aquavate.xcdatamodeld/.xccurrentversion`

### 2. Fix unsafe integer conversions in PersistenceController.swift

- Line 391: `Int16(event.durationSec)` → `Int32(event.durationSec)`
- Line 417: `Int16(session.timerWakeCount)` → `Int32(session.timerWakeCount)`

**File:** `ios/Aquavate/Aquavate/CoreData/PersistenceController.swift`

### 3. Update preview/sample data types

- Line 73: sample motion events tuple type `durationSec: Int16` → `Int32`
- Line 96: sample backpack sessions tuple type `timerWakes: Int16` → `Int32`

**File:** `ios/Aquavate/Aquavate/CoreData/PersistenceController.swift`

### 4. Update ActivityStatsView enum

- Line 19: `case backpack(timerWakes: Int16)` → `case backpack(timerWakes: Int32)`

**File:** `ios/Aquavate/Aquavate/Views/ActivityStatsView.swift`

### 5. Update BackupModels struct types

- `BackupMotionWakeEvent.durationSec`: `Int16` → `Int32`
- `BackupBackpackSession.timerWakeCount`: `Int16` → `Int32`

No backup format version bump needed — `Codable` decodes JSON numbers to the target type regardless, so old backups remain compatible.

**File:** `ios/Aquavate/Aquavate/Models/BackupModels.swift`

### 6. Preserve activity stats across firmware updates (bonus fix)

The activity stats sync was doing a destructive clear-and-replace on every BLE sync, wiping historical data. Removed `clearActivityStats()` call from `saveActivityStatsToCoreData()`. The existing CoreData uniqueness constraints (`timestamp`+`bottleId`) and merge policy (`NSMergeByPropertyObjectTrumpMergePolicy`) handle deduplication, so historical records are now preserved across firmware updates and power cycles.

**File:** `ios/Aquavate/Aquavate/Services/BLEManager.swift`

## Files Modified (summary)

| File | Change |
|------|--------|
| `Aquavate.xcdatamodeld/Aquavate 2.xcdatamodel/contents` | New model version with Int32 fields |
| `Aquavate.xcdatamodeld/.xccurrentversion` | Point to new model |
| `CoreData/PersistenceController.swift` | Fix Int16→Int32 conversions + preview data |
| `Views/ActivityStatsView.swift` | Widen enum associated value |
| `Models/BackupModels.swift` | Widen struct field types |
| `Services/BLEManager.swift` | Preserve historical activity stats (merge instead of clear-and-replace) |

## Verification

1. Build the iOS app: `xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build` — **PASSED**
2. Deployed to device, navigated to Activity Stats — no crash
3. Verified activity stats persist across firmware updates (confirmed by user)
4. CoreData lightweight migration auto-applied (Int16→Int32 widening)
