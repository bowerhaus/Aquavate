# Plan: Issue #57 - Show Last Known Bottle Level with "(recent)" Indicator

## Problem Summary

When the bottle is disconnected, the bottle level shows 0% because:
- `bottleLevelMl` is initialized to 0 in BLEManager
- Unlike `dailyGoalMl`, bottle level is not persisted to UserDefaults
- When the app restarts or loses connection, the in-memory value resets

## Implementation Approach

### 1. Persist Bottle Level to UserDefaults (BLEManager.swift)

Add persistence for `bottleLevelMl` similar to the existing `dailyGoalMl` pattern:

**File:** [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)

```swift
// Line ~102 - Add didSet to persist
@Published private(set) var bottleLevelMl: Int = 0 {
    didSet {
        UserDefaults.standard.set(bottleLevelMl, forKey: "lastKnownBottleLevelMl")
    }
}

// Add a new published property to track if we've ever received bottle data
@Published private(set) var hasReceivedBottleData: Bool = false {
    didSet {
        UserDefaults.standard.set(hasReceivedBottleData, forKey: "hasReceivedBottleData")
    }
}
```

In `init()`, restore from UserDefaults:
```swift
bottleLevelMl = UserDefaults.standard.integer(forKey: "lastKnownBottleLevelMl")
hasReceivedBottleData = UserDefaults.standard.bool(forKey: "hasReceivedBottleData")
```

In `handleCurrentStateUpdate()` (line ~1046), set the flag when we receive data:
```swift
bottleLevelMl = Int(state.bottleLevelMl)
hasReceivedBottleData = true  // Add this line
```

### 2. Update Bottle Level Display (HomeView.swift)

**File:** [HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift)

Modify the bottle level section (lines 238-261) to:
- Show "(recent)" suffix when disconnected but have previous data
- Hide the entire section if no connection has ever happened

```swift
// Add computed property
private var bottleLevelSuffix: String {
    if bleManager.connectionState.isConnected {
        return ""
    } else if bleManager.hasReceivedBottleData {
        return " (recent)"
    }
    return ""
}

// Wrap bottle level section in conditional
if bleManager.hasReceivedBottleData || bleManager.connectionState.isConnected {
    // Existing VStack for "Bottle Level"
    // Modify the percentage text to include suffix:
    Text("\(Int(bottleProgress * 100))%\(bottleLevelSuffix)")
}
```

## Files to Modify

| File | Changes |
|------|---------|
| [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) | Add persistence for `bottleLevelMl`, add `hasReceivedBottleData` flag |
| [HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift) | Conditionally show bottle level section, add "(recent)" suffix |

## Verification

1. **Fresh install scenario**: Launch app without ever connecting - bottle level section should not be visible
2. **Connect and disconnect**: Connect to bottle, note level, disconnect - should show "75% (recent)"
3. **Reconnect**: Connect again - "(recent)" suffix should disappear
4. **App restart**: Close app while disconnected, reopen - should still show last known level with "(recent)"
5. **Build verification**: Run `platformio run` to ensure firmware still builds (no firmware changes needed)
