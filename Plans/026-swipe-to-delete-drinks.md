# Plan 026: Swipe-to-Delete Drinks and Enhanced Reset Daily

**Status:** ✅ Complete and Tested (2026-01-20)

## Overview

Add swipe-to-delete functionality for drink records in HomeView and HistoryView, and enhance the Reset Daily command to also delete today's CoreData records.

## Requirements

1. **Swipe-to-delete drinks** - Standard iOS swipe-left gesture on drink list items
   - HomeView: Recent Drinks section
   - HistoryView: Selected day's drink list
   - iOS-only deletion (CoreData) - firmware records preserved
   - No confirmation dialog

2. **Reset Daily enhancement** - When triggered, delete today's records from both:
   - CoreData (iOS app)
   - Firmware (via existing RESET_DAILY command)
   - Historical records preserved

3. **Update iOS-UX-PRD.md** - Document new interactions

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Firmware sync on delete | iOS-only | Firmware can only delete last record; timestamp dedup prevents re-sync |
| Reset Daily scope | Today only | Preserve historical records; CLEAR_HISTORY exists for full wipe |
| Confirmation dialog | None | Standard iOS pattern; data recoverable via firmware re-sync |
| Multi-device sync | Accept re-sync | Deleted records may reappear on new device; user can delete again |

## Known Limitation

**Deleted records may reappear when syncing to a new device/app:**
- Swipe-to-delete only removes records from iOS CoreData
- Firmware retains all drink records
- If user reinstalls app or syncs from another device, deleted drinks will re-sync
- User can simply delete them again if needed
- This keeps the implementation simple without requiring firmware changes

## Implementation

### 1. PersistenceController.swift

**Add two new methods after `deleteAllDrinkRecords()` (line ~229):**

```swift
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
            print("[CoreData] Deleted drink record: \(id)")
        }
    } catch {
        print("[CoreData] Error deleting drink record: \(error)")
    }
}

/// Delete all drink records from today
func deleteTodaysDrinkRecords() {
    let startOfDay = Calendar.current.startOfDay(for: Date())
    let request: NSFetchRequest<CDDrinkRecord> = CDDrinkRecord.fetchRequest()
    request.predicate = NSPredicate(format: "timestamp >= %@", startOfDay as CVarArg)

    do {
        let records = try viewContext.fetch(request)
        let count = records.count
        for record in records {
            viewContext.delete(record)
        }
        try viewContext.save()
        print("[CoreData] Deleted \(count) today's drink records")
    } catch {
        print("[CoreData] Error deleting today's drink records: \(error)")
    }
}
```

### 2. HistoryView.swift

**Add computed property for CDDrinkRecord access (after line ~57):**

```swift
// Get CDDrinkRecords for selected date (for deletion)
private var drinksForSelectedDateCD: [CDDrinkRecord] {
    let calendar = Calendar.current
    let targetDay = calendar.startOfDay(for: selectedDate)

    return recentDrinksCD
        .filter { calendar.startOfDay(for: $0.timestamp ?? .distantPast) == targetDay }
        .sorted { ($0.timestamp ?? .distantPast) > ($1.timestamp ?? .distantPast) }
}

// Delete drinks at given offsets
private func deleteDrinks(at offsets: IndexSet) {
    for index in offsets {
        let cdRecord = drinksForSelectedDateCD[index]
        if let id = cdRecord.id {
            PersistenceController.shared.deleteDrinkRecord(id: id)
        }
    }
}
```

**Modify List/ForEach (lines 94-98) to use CDDrinkRecords and add .onDelete:**

```swift
List {
    Section {
        ForEach(drinksForSelectedDateCD) { cdRecord in
            DrinkListItem(drink: cdRecord.toDrinkRecord())
        }
        .onDelete(perform: deleteDrinks)
    } header: {
        // ... unchanged
    }
}
```

### 3. HomeView.swift

**Add computed property and delete handler (after line ~81):**

```swift
// CDDrinkRecords for recent drinks (for deletion)
private var recentDrinksCD: [CDDrinkRecord] {
    Array(todaysDrinksCD.prefix(5))
}

// Delete recent drinks at given offsets
private func deleteRecentDrinks(at offsets: IndexSet) {
    for index in offsets {
        let cdRecord = recentDrinksCD[index]
        if let id = cdRecord.id {
            PersistenceController.shared.deleteDrinkRecord(id: id)
        }
    }
}
```

**Replace Recent Drinks VStack (lines 214-237) with List:**

```swift
// Recent drinks list
VStack(alignment: .leading, spacing: 12) {
    Text("Recent Drinks")
        .font(.headline)
        .padding(.horizontal)

    if todaysDrinksCD.isEmpty {
        Text("No drinks recorded today")
            .font(.subheadline)
            .foregroundStyle(.secondary)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 32)
    } else {
        List {
            ForEach(recentDrinksCD) { cdRecord in
                DrinkListItem(drink: cdRecord.toDrinkRecord())
            }
            .onDelete(perform: deleteRecentDrinks)
            .listRowInsets(EdgeInsets(top: 4, leading: 16, bottom: 4, trailing: 16))
        }
        .listStyle(.plain)
        .scrollDisabled(true)
        .frame(height: CGFloat(min(recentDrinksCD.count, 5)) * 60)
    }
}
```

### 4. SettingsView.swift

**Update Reset Daily button (lines 280-288) to also delete CoreData records:**

```swift
Button {
    bleManager.sendResetDailyCommand()
    PersistenceController.shared.deleteTodaysDrinkRecords()
} label: {
    HStack {
        Image(systemName: "arrow.uturn.backward")
            .foregroundStyle(.orange)
        Text("Reset Daily Total")
    }
}
```

### 5. DebugView.swift

**Update Reset Daily button (lines 126-130):**

```swift
Button("Reset Daily Total") {
    addLog("Sent RESET_DAILY command", type: .success)
    bleManager.sendResetDailyCommand()
    PersistenceController.shared.deleteTodaysDrinkRecords()
    addLog("Deleted today's CoreData records", type: .info)
}
.disabled(!bleManager.connectionState.isConnected)
```

### 6. iOS-UX-PRD.md

**Add to changelog (top of file):**
```markdown
- **v1.3 (2026-01-20):** Added swipe-to-delete for drink records (Section 4). Updated Reset Daily to also clear today's CoreData records.
```

**Update Gestures table (around line 830):**
```markdown
| Swipe left | Drink list items | Delete drink record (iOS only) |
```

**Add new subsection under Section 4 (Interaction Patterns):**

```markdown
### Swipe-to-Delete (Drink Records)

**Purpose:** Allow users to remove individual drink records from iOS storage

**Availability:**
- HomeView: Recent Drinks list
- HistoryView: Selected day's drink list

**Behavior:**
- Standard iOS swipe-left gesture reveals red "Delete" button
- Single tap on Delete removes the record immediately
- No confirmation dialog (follows iOS standard pattern)
- Deletion is iOS-only (CoreData) - firmware records remain but won't re-sync due to timestamp deduplication

**Visual:**
```
┌───────────────────────────────┐
│  💧  200ml        420ml       │ ← Swipe left
│      2:30 PM      remaining   │
└───────────────────────────────┘
                      ┌─────────┐
                      │ Delete  │ ← Red background
                      └─────────┘
```
```

**Update Reset Daily row in Command Actions table (around line 494):**
```markdown
| Reset Daily Total | Sends RESET_DAILY (0x05) + deletes today's CoreData records | None (instant) |
```

## Files to Modify

| File | Changes |
|------|---------|
| [PersistenceController.swift](ios/Aquavate/Aquavate/CoreData/PersistenceController.swift) | Add `deleteDrinkRecord(id:)` and `deleteTodaysDrinkRecords()` |
| [HistoryView.swift](ios/Aquavate/Aquavate/Views/HistoryView.swift) | Add `.onDelete` modifier, helper methods |
| [HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift) | Convert Recent Drinks to List with `.onDelete` |
| [SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift) | Add CoreData deletion to Reset Daily |
| [DebugView.swift](ios/Aquavate/Aquavate/Views/DebugView.swift) | Add CoreData deletion to Reset Daily |
| [iOS-UX-PRD.md](docs/iOS-UX-PRD.md) | Document swipe-to-delete, update Reset Daily |

## Verification

1. **Swipe-to-delete in HistoryView:**
   - Open History tab, select a day with drinks
   - Swipe left on a drink item → Delete button appears
   - Tap Delete → Record removed, list updates
   - Verify record not in CoreData (check Debug view count)

2. **Swipe-to-delete in HomeView:**
   - Open Home tab with recent drinks visible
   - Swipe left on a drink item → Delete button appears
   - Tap Delete → Record removed, list updates
   - Daily total recalculates (when disconnected)

3. **Reset Daily with CoreData deletion:**
   - Ensure some drinks recorded today
   - Open Settings → Device Commands → Reset Daily Total
   - Verify: Daily total shows 0, Recent Drinks empty
   - Open History → Today shows no drinks
   - Debug view → Today's CoreData records count = 0

4. **Data recovery test:**
   - Delete some drinks via swipe
   - Connect to bottle and sync
   - Deleted drinks should NOT reappear (timestamp dedup)
