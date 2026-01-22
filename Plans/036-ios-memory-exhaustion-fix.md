# Plan: Fix iOS App Memory Exhaustion

## Problem
The Aquavate iOS app is being killed by iOS after ~31 minutes due to memory exhaustion.

## Root Cause Analysis

After examining the codebase, I identified several issues that together cause memory pressure:

### Primary Issue: DebugView Continuous Re-rendering
**File:** `ios/Aquavate/Aquavate/Views/DebugView.swift:225-232`

```swift
.onReceive(bleManager.$connectionState) { state in
    addLog("Connection: \(state.rawValue)", type: state.isConnected ? .success : .info)
}
.onReceive(bleManager.$syncState) { state in
    if state != .idle {
        addLog("Sync: \(state.rawValue)", type: state == .complete ? .success : .info)
    }
}
```

These publishers fire on every BLE state change, triggering:
1. Log entry creation (with new UUID allocation each time)
2. View re-render (SwiftUI detects @State change)
3. Re-computation of CoreData queries in the "CoreData" section
4. Continuous memory allocation churn

### Secondary Issue: CoreData Fetches in View Body
**File:** `ios/Aquavate/Aquavate/Views/DebugView.swift:154-157`

```swift
let todaysRecords = PersistenceController.shared.getTodaysDrinkRecords()
let todaysTotal = PersistenceController.shared.getTodaysTotalMl()
let allRecords = PersistenceController.shared.getAllDrinkRecords()
```

These are computed on every view render, creating new NSManagedObject arrays each time.

### Tertiary Issue: DateFormatter Recreation
**File:** `ios/Aquavate/Aquavate/Views/HistoryView.swift:219-223, 231-240`

DateFormatters created inside functions are recreated on every call.

### Contributing Factor: Uncancelled Tasks in BLEManager
**File:** `ios/Aquavate/Aquavate/Services/BLEManager.swift:377-383, 692-697`

Task objects created for reconnection delays are not tracked or cancelled.

## Implementation Plan

### 1. Fix DebugView `.onReceive` Publishers
- Remove automatic logging on state changes, OR
- Add duplicate filtering to prevent logging identical states
- Make logging manual-only (user presses buttons to see state)

### 2. Optimize DebugView CoreData Section
- Move CoreData fetches to `@FetchRequest` properties like HomeView/HistoryView
- These are properly managed by SwiftUI and don't re-fetch on every render

### 3. Cache DateFormatters
- Create static DateFormatter instances in HistoryView/DayCard
- DateFormatter creation is expensive

### 4. Add Task Cancellation to BLEManager
- Store reconnection Tasks in a property
- Cancel existing task before creating new one
- Cancel on deinit

## Files to Modify

1. `ios/Aquavate/Aquavate/Views/DebugView.swift`
   - Remove `.onReceive` publishers (or add deduplication)
   - Convert CoreData queries to @FetchRequest

2. `ios/Aquavate/Aquavate/Views/HistoryView.swift`
   - Add static DateFormatter instances

3. `ios/Aquavate/Aquavate/Services/BLEManager.swift`
   - Add Task cancellation for reconnection logic

## Verification

1. Build the app and run on device
2. Let app run for >31 minutes with DebugView visible
3. Monitor memory in Xcode Instruments/Memory Graph
4. Verify app survives extended runtime
