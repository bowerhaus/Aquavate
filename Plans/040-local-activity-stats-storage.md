# Plan: Local Activity Stats Storage (Issue #36 Comment)

## Status: Complete ✅

Implemented 2026-01-24. Activity stats (motion wake events, backpack sessions) now persist in CoreData. Users can view this data even when disconnected, with a "Last synced X ago" timestamp.

## Overview
Store activity stats (motion wake events, backpack sessions) locally in CoreData so users can view this data even when the bottle is disconnected. This mirrors the existing pattern used for drink records.

## Current State
- **ActivityStatsView** shows "Connect to bottle to view activity stats" when disconnected
- Data lives only in BLEManager's in-memory arrays (`motionWakeEvents`, `backpackSessions`)
- Data is lost when app restarts or bottle disconnects

## Implementation

### 1. Add CoreData Entities
**File:** [Aquavate.xcdatamodel/contents](ios/Aquavate/Aquavate/CoreData/Aquavate.xcdatamodeld/Aquavate.xcdatamodel/contents)

Add two new entities:

**CDMotionWakeEvent:**
| Attribute | Type | Description |
|-----------|------|-------------|
| id | UUID | App's internal ID |
| timestamp | Date | When wake occurred |
| durationSec | Int16 | How long device stayed awake |
| wakeReason | Int16 | 0=motion, 1=timer, 2=power_on |
| sleepType | Int16 | 0=normal, 1=extended |
| drinkTaken | Bool | Whether a drink was taken during this wake |
| bottleId | UUID | Reference to bottle |

Uniqueness constraint: `(timestamp, bottleId)`

**CDBackpackSession:**
| Attribute | Type | Description |
|-----------|------|-------------|
| id | UUID | App's internal ID |
| startTimestamp | Date | When session started |
| durationSec | Int32 | Total time in backpack mode |
| timerWakeCount | Int16 | Number of timer wakes during session |
| exitReason | Int16 | 0=motion_detected, 1=still_active, 2=power_cycle |
| bottleId | UUID | Reference to bottle |

Uniqueness constraint: `(startTimestamp, bottleId)`

Add relationships to CDBottle:
- `motionWakeEvents` (to-many, cascade delete)
- `backpackSessions` (to-many, cascade delete)

Also add to CDBottle:
- `lastActivitySyncDate: Date` - when activity stats were last synced

### 2. Add Persistence Methods
**File:** [PersistenceController.swift](ios/Aquavate/Aquavate/CoreData/PersistenceController.swift)

Add methods following the existing `saveDrinkRecords` pattern:

```swift
func saveMotionWakeEvents(_ events: [BLEMotionWakeEvent], for bottleId: UUID)
func saveBackpackSessions(_ sessions: [BLEBackpackSession], for bottleId: UUID)
func clearActivityStats(for bottleId: UUID)  // Clear before re-sync
func updateLastActivitySyncDate(for bottleId: UUID)
```

### 3. Update BLEManager
**File:** [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)

After activity fetch completes (in the existing fetch flow):
1. Clear existing activity stats for bottle (fresh sync each time)
2. Save motion wake events to CoreData
3. Save backpack sessions to CoreData
4. Update `lastActivitySyncDate` on bottle

### 4. Update ActivityStatsView
**File:** [ActivityStatsView.swift](ios/Aquavate/Aquavate/Views/ActivityStatsView.swift)

Changes:
- Add `@FetchRequest` for CDMotionWakeEvent and CDBackpackSession
- Remove dependency on `bleManager.motionWakeEvents` and `bleManager.backpackSessions`
- When disconnected: show cached data from CoreData with "Last synced: X ago" header
- When connected: trigger fetch, which updates CoreData, which updates view via @FetchRequest
- Keep pull-to-refresh for manual refresh when connected

### 5. Update Preview Provider
Add sample activity data to `PersistenceController.preview` for SwiftUI previews.

## Files to Modify
1. [Aquavate.xcdatamodel/contents](ios/Aquavate/Aquavate/CoreData/Aquavate.xcdatamodeld/Aquavate.xcdatamodel/contents) - Add entities
2. [PersistenceController.swift](ios/Aquavate/Aquavate/CoreData/PersistenceController.swift) - Add save methods
3. [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) - Save after fetch
4. [ActivityStatsView.swift](ios/Aquavate/Aquavate/Views/ActivityStatsView.swift) - Use CoreData

## Verification
1. Build the iOS app successfully
2. Connect to bottle and fetch activity stats - verify data saves to CoreData
3. Disconnect bottle - verify cached data still displays
4. Reconnect and pull-to-refresh - verify data updates
5. Force quit and relaunch app while disconnected - verify data persists
