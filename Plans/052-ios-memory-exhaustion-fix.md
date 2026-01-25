# Plan: Fix iOS App Memory Exhaustion (Issue #28)

**Status:** ✅ COMPLETE (2026-01-25)

**Summary:** Fixed iOS app memory exhaustion that caused termination after ~31 minutes. All memory leaks identified and resolved. App tested 60+ minutes with stable persistent memory (~43 MiB, only 0.31 MiB increase).

## Problem Summary
The iOS app is terminated by iOS after ~31 minutes due to memory exhaustion. The 31-minute timeline suggests gradual memory accumulation, likely from operations running on the 60-second HydrationReminderService timer.

## Root Causes Identified

### 1. **WatchConnectivity `transferUserInfo` Queue Accumulation** (HIGH PRIORITY)
- **Location**: [WatchConnectivityManager.swift:140](ios/Aquavate/Aquavate/Services/WatchConnectivityManager.swift#L140)
- **Issue**: `sendNotificationToWatch()` uses `transferUserInfo()` which queues data indefinitely if the Watch isn't receiving it
- **Impact**: Called every time a hydration reminder is sent (~every evaluation cycle when behind pace)
- **No check** for `outstandingUserInfoTransfers` before queueing more

### 2. **HomeView & HistoryView @FetchRequest Fetch ALL Records** (MEDIUM PRIORITY)
- **Locations**:
  - [HomeView.swift:26-30](ios/Aquavate/Aquavate/Views/HomeView.swift#L26-L30) - fetches all, filters to today
  - [HistoryView.swift:23-27](ios/Aquavate/Aquavate/Views/HistoryView.swift#L23-L27) - fetches all, filters to last 7 days
- **Issue**: Both `@FetchRequest` have no predicate, so CoreData loads ALL drink records into memory
- **Note**: The computed property filters work correctly for display, but CoreData has already loaded all records into RAM
- **Impact**: Memory grows linearly with historical data (weeks/months of drink records)

### 3. **NotificationManager Strong Self Capture** (MEDIUM PRIORITY)
- **Location**: [NotificationManager.swift:215-219](ios/Aquavate/Aquavate/Services/NotificationManager.swift#L215-L219)
- **Issue**: Completion handler creates `Task { @MainActor in self.remindersSentToday += 1 }` - captures `self` strongly
- **Impact**: If notifications accumulate before callbacks complete, closure references pile up

### 4. **BLEManager Delete Timeout Task** (LOW PRIORITY)
- **Location**: [BLEManager.swift:1480-1488](ios/Aquavate/Aquavate/Services/BLEManager.swift#L1480-L1488)
- **Issue**: Task sleeps for 5 seconds capturing `self` - if multiple deletes triggered rapidly, Tasks accumulate
- **Impact**: Limited (only affects delete operations)

## Recommended Fixes

### Fix 1: Limit WatchConnectivity Queue Size
```swift
// In WatchConnectivityManager.sendNotificationToWatch()
// Add before transferUserInfo:
guard let session = session, session.outstandingUserInfoTransfers.count < 5 else {
    print("[WatchConnectivity] Queue full, skipping notification")
    return
}
```

### Fix 2: Add Predicates to @FetchRequest in HomeView and HistoryView

**HomeView** - fetch only today's records:
```swift
@FetchRequest(
    sortDescriptors: [NSSortDescriptor(keyPath: \CDDrinkRecord.timestamp, ascending: false)],
    predicate: NSPredicate(format: "timestamp >= %@", Calendar.current.startOfAquavateDay(for: Date()) as CVarArg),
    animation: .default
)
private var todaysDrinksCD: FetchedResults<CDDrinkRecord>
```

**HistoryView** - fetch only last 7 days:
```swift
@FetchRequest(
    sortDescriptors: [NSSortDescriptor(keyPath: \CDDrinkRecord.timestamp, ascending: false)],
    predicate: NSPredicate(format: "timestamp >= %@", Calendar.current.date(byAdding: .day, value: -7, to: Date())! as CVarArg),
    animation: .default
)
private var recentDrinksCD: FetchedResults<CDDrinkRecord>
```

**Note**: Predicate dates are evaluated once at view creation. This is acceptable because:
- SwiftUI recreates views when switching tabs
- App backgrounding/foregrounding triggers view recreation
- Can simplify by removing the now-redundant computed property filters

### Fix 3: Use `[weak self]` in NotificationManager Callback
```swift
// In scheduleHydrationReminder():
notificationCenter.add(request) { [weak self] error in
    if let error = error {
        print("[Notifications] Failed to schedule: \(error.localizedDescription)")
    } else {
        Task { @MainActor [weak self] in
            guard let self = self else { return }
            self.remindersSentToday += 1
            UserDefaults.standard.set(self.remindersSentToday, forKey: "remindersSentToday")
            // ...
        }
    }
}
```

### Fix 4: Cancel Previous Delete Timeout Task
```swift
// In BLEManager:
private var pendingDeleteTask: Task<Void, Never>?

func deleteDrinkRecord(...) {
    // Cancel any previous timeout task
    pendingDeleteTask?.cancel()

    pendingDeleteTask = Task { @MainActor in
        try? await Task.sleep(nanoseconds: 5_000_000_000)
        guard !Task.isCancelled else { return }
        // ... timeout handling
    }
}
```

## Files to Modify

1. [ios/Aquavate/Aquavate/Services/WatchConnectivityManager.swift](ios/Aquavate/Aquavate/Services/WatchConnectivityManager.swift) - Add queue limit check
2. [ios/Aquavate/Aquavate/Views/HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift) - Add predicate to @FetchRequest, remove redundant computed property
3. [ios/Aquavate/Aquavate/Views/HistoryView.swift](ios/Aquavate/Aquavate/Views/HistoryView.swift) - Add predicate to @FetchRequest, remove redundant computed property
4. [ios/Aquavate/Aquavate/Services/NotificationManager.swift](ios/Aquavate/Aquavate/Services/NotificationManager.swift) - Add weak self captures
5. [ios/Aquavate/Aquavate/Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) - Cancel previous delete Task

## Verification

1. Run app with Xcode Instruments "Allocations" profiler
2. Monitor heap growth over 35+ minutes
3. Verify memory stabilizes (no continuous growth)
4. Test with Watch paired and unpaired scenarios
5. Test pull-to-refresh and delete operations

## Testing Notes

- User has Apple Watch paired for testing
- Can verify WatchConnectivity queue behavior with Watch connected/disconnected
- Use Xcode Instruments "Allocations" to monitor heap growth over 35+ minutes

## Completion Notes (2026-01-25)

**Test Results:**
- App ran 60+ minutes without memory exhaustion or crash
- Xcode Instruments "Allocations" confirmed persistent memory stable at ~43 MiB
- Only 0.31 MiB increase over the test period (acceptable normal growth)
- Previous behavior: app killed after ~31 minutes

**Additional Fix Applied:**
- Fix 6: Optimized HistoryView to compute totals directly from CoreData FetchedResults
  - Removed intermediate `drinks: [DrinkRecord]` computed property that created temporary objects
  - `totalForDate()` and `drinksForSelectedDateCD` now work directly with CoreData objects
  - Further reduces memory allocations during view updates
