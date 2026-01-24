# Plan: Fix Duplicate HealthKit Entries (Issue #37)

## Problem

Multiple HealthKit records are being created for the same drink event, inflating daily water intake totals.

## Root Cause

Race condition in `syncDrinksToHealthKit()` due to **fire-and-forget async pattern**:

```swift
// BLEManager.swift:1260-1262
Task {
    await syncDrinksToHealthKit()  // Not awaited - multiple can run concurrently!
}
```

When two Tasks run concurrently:
1. Both call `getUnsyncedDrinkRecords()` (returns same records)
2. Both call `logWater()` for same drinks
3. Both create HealthKit entries (duplicates)

## Solution: Add Boolean Lock

Add a simple lock to prevent concurrent execution of `syncDrinksToHealthKit()`.

### Files to Modify

| File | Changes |
|------|---------|
| [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) | Add lock flags, protect sync function |
| [HealthKitManager.swift](ios/Aquavate/Aquavate/Services/HealthKitManager.swift) | Add timestamp dedup check (defense in depth) |

### Step 1: Add Lock Properties (BLEManager.swift ~line 156)

```swift
/// Whether HealthKit sync is currently in progress
private var isHealthKitSyncInProgress = false

/// Whether a sync was requested while one was in progress
private var pendingHealthKitSync = false
```

### Step 2: Protect syncDrinksToHealthKit() (BLEManager.swift:1294)

Replace the existing function with lock-protected version:

```swift
private func syncDrinksToHealthKit() async {
    // Prevent concurrent execution
    guard !isHealthKitSyncInProgress else {
        logger.info("[HealthKit] Sync already in progress, queuing re-sync")
        pendingHealthKitSync = true
        return
    }

    guard let healthKitManager = healthKitManager,
          healthKitManager.isEnabled && healthKitManager.isAuthorized else {
        return
    }

    isHealthKitSyncInProgress = true
    defer {
        isHealthKitSyncInProgress = false
        if pendingHealthKitSync {
            pendingHealthKitSync = false
            Task { await self.syncDrinksToHealthKit() }
        }
    }

    // ... existing sync logic unchanged ...
}
```

### Step 3: Add Timestamp Dedup Check (HealthKitManager.swift) - Defense in Depth

Add function to check if sample already exists:

```swift
func waterSampleExists(for date: Date) async -> Bool {
    guard isHealthKitAvailable else { return false }

    let startDate = date.addingTimeInterval(-0.5)
    let endDate = date.addingTimeInterval(0.5)
    let predicate = HKQuery.predicateForSamples(withStart: startDate, end: endDate)

    return await withCheckedContinuation { continuation in
        let query = HKSampleQuery(sampleType: waterType, predicate: predicate, limit: 1, sortDescriptors: nil) { _, samples, _ in
            continuation.resume(returning: !(samples?.isEmpty ?? true))
        }
        healthStore.execute(query)
    }
}
```

Then in `syncDrinksToHealthKit()`, before each write:

```swift
if await healthKitManager.waterSampleExists(for: timestamp) {
    logger.info("[HealthKit] Sample already exists, skipping")
    PersistenceController.shared.markDrinkSyncedToHealth(id: drinkId, healthKitUUID: UUID())
    continue
}
```

## Verification

1. **Build**: Ensure app compiles without errors
2. **Manual test**:
   - Take a drink from bottle
   - Rapidly trigger sync (pull-to-refresh multiple times)
   - Check Apple Health app - should see exactly one entry
3. **Check logs**: Look for "[HealthKit] Sync already in progress" messages

## Why This Approach

- **Simple**: Boolean lock fits existing `@MainActor` pattern
- **Safe**: Queues rather than drops sync requests
- **Defense in depth**: Timestamp check catches crash recovery edge cases
- **Minimal changes**: Only modifies sync path, no impact on BLE protocol
