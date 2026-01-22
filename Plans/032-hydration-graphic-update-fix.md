# Plan: Fix Hydration Graphic Not Updating on Drink Deletion (Issue #20)

## Problem Summary

When deleting a drink record from the iOS app, the list updates correctly but the `HumanFigureProgressView` (the "filled man" graphic) does not reflect the new daily total.

## Root Cause Analysis

The issue is in [HomeView.swift:98-110](ios/Aquavate/Aquavate/Views/HomeView.swift#L98-L110). When a drink is deleted:

```swift
bleManager.deleteDrinkRecord(firmwareRecordId: UInt32(firmwareId)) { success in
    if success {
        Task {  // ← Problem: No @MainActor annotation
            // HealthKit delete...
            PersistenceController.shared.deleteDrinkRecord(id: id)
        }
    }
    continuation.resume()
}
```

**The bug**: The BLE completion handler is called from a CoreBluetooth background thread. The inner `Task { ... }` without `@MainActor` runs the CoreData deletion on an unspecified executor. Since CoreData's `viewContext` must be accessed from the main thread:

1. The deletion may execute from a background thread
2. CoreData change notifications don't propagate correctly to the `@FetchRequest`
3. The `HumanFigureProgressView` receives stale `displayDailyTotal` value

**Why the list updates**: The list uses `ForEach(todaysDrinksCD)` which iterates the `@FetchRequest` directly. SwiftUI's list diffing may detect the removal through a different mechanism than the view's body re-evaluation.

## Solution

Change the inner `Task` to `Task { @MainActor in ... }` to ensure the CoreData deletion happens on the main thread.

## Implementation

### File: [ios/Aquavate/Aquavate/Views/HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift)

**Change at line 98**: Add `@MainActor` to the Task:

```swift
// Before:
Task {
    if let hkUUID = healthKitUUID,

// After:
Task { @MainActor in
    if let hkUUID = healthKitUUID,
```

This single change ensures:
1. CoreData deletion runs on the main thread
2. `@FetchRequest` notifications propagate correctly
3. `displayDailyTotal` is recalculated when the view body re-evaluates
4. `HumanFigureProgressView` receives the updated value

## Verification

1. Build the iOS app in Xcode
2. Connect to the bottle (or use preview/simulator with mock data)
3. Add multiple drink records
4. Observe the hydration graphic fill level
5. Delete a drink record by swiping
6. **Verify**: Both the list AND the hydration graphic update to reflect the deletion
7. Check the daily total text matches the graphic fill level
