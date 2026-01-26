# Plan: Fix iOS App Day Boundary Bug

## Problem
After a date change (midnight), the app shows drinks from the previous day combined with new day's drinks, even after refreshing from the bottle.

## Root Cause
**The `@FetchRequest` predicate in [HomeView.swift:29](ios/Aquavate/Aquavate/Views/HomeView.swift#L29) is static.**

```swift
@FetchRequest(
    sortDescriptors: [...],
    predicate: NSPredicate(format: "timestamp >= %@",
        Calendar.current.startOfAquavateDay(for: Date()) as CVarArg),  // ← Date() captured ONCE
    animation: .default
)
private var todaysDrinksCD: FetchedResults<CDDrinkRecord>
```

The `Date()` call is evaluated once when the view is initialized. If the app stays in memory across midnight (even backgrounded), the predicate never updates, continuing to show yesterday's drinks.

## Fix

**Update the `@FetchRequest.nsPredicate` dynamically** when:
1. The view appears (`.onAppear`)
2. The calendar day changes (via `UIApplication.significantTimeChangeNotification`)

### Changes to [HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift)

1. **Remove the static predicate** from `@FetchRequest` initializer (set predicate to `nil` initially)

2. **Add a helper method** to generate the current day's predicate:
   ```swift
   private func todaysPredicate() -> NSPredicate {
       NSPredicate(format: "timestamp >= %@",
           Calendar.current.startOfAquavateDay(for: Date()) as CVarArg)
   }
   ```

3. **Update predicate on appear**:
   ```swift
   .onAppear {
       todaysDrinksCD.nsPredicate = todaysPredicate()
   }
   ```

4. **Listen for significant time changes** (date change notification):
   ```swift
   .onReceive(NotificationCenter.default.publisher(
       for: UIApplication.significantTimeChangeNotification)) { _ in
       todaysDrinksCD.nsPredicate = todaysPredicate()
   }
   ```

## Secondary Consideration

The `HydrationReminderService.dailyReset()` method exists but is never called. However, this is less critical because:
- `syncCurrentStateFromCoreData()` is called on app activation (line 76 in AquavateApp.swift)
- It re-fetches from CoreData using `getTodaysDrinkRecords()` which dynamically evaluates `Date()`

The CoreData fetch in `HydrationReminderService` is correct; only the `@FetchRequest` UI binding is stale.

## Verification

1. Build the iOS app
2. Add some drinks before midnight
3. Wait for (or simulate) date change
4. Open app and verify only today's drinks appear
5. Pull-to-refresh and verify totals are correct
