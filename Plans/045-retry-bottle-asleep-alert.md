# Plan 045: Add Retry Button to "Bottle is Asleep" Alert

**GitHub Issue:** #52
**Branch:** `retry-bottle-asleep-alert`

## Summary

Add a Retry button to the "Bottle is Asleep" alert so users can immediately retry the connection after waking the bottle, instead of manually pulling down to refresh.

## Current Behavior

Both HomeView and HistoryView show an alert with only an "OK" button:
```swift
.alert("Bottle is Asleep", isPresented: $showBottleAsleepAlert) {
    Button("OK", role: .cancel) { }
} message: {
    Text("Tilt your bottle to wake it up, then pull down to try again.")
}
```

## Implementation

### Files to Modify

1. `ios/Aquavate/Aquavate/Views/HomeView.swift` (~line 308)
2. `ios/Aquavate/Aquavate/Views/HistoryView.swift` (~line 184)

### Changes

Update the alert in both files to:

```swift
.alert("Bottle is Asleep", isPresented: $showBottleAsleepAlert) {
    Button("Retry") {
        Task {
            await handleRefresh()
        }
    }
    Button("Cancel", role: .cancel) { }
} message: {
    Text("Tilt your bottle to wake it up, then tap Retry.")
}
```

**Key points:**
- Retry button triggers `handleRefresh()` wrapped in a Task (async context)
- Cancel button has `role: .cancel` for standard dismissal styling
- Message updated: "pull down to try again" → "tap Retry"
- Retry button listed first (appears on right/primary position on iOS)

## Verification

1. Build the app in Xcode
2. With bottle asleep (not advertising), pull down to refresh on HomeView
3. Alert should appear with "Retry" and "Cancel" buttons
4. Tap Cancel → alert dismisses, no action
5. Pull down again, tap Retry → new connection attempt begins
6. Repeat test on HistoryView
