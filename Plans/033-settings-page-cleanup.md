# Plan: GitHub Issue #22 - Settings Page Cleanup

**Status:** ✅ Complete (2026-01-22)

## Summary

Four changes:
1. Replace hardcoded `Bottle.sample` with live BLE data from `BLEManager`
2. Remove unused `useOunces` preference
3. Remove version string from About section
4. Keep device name after disconnect (so it shows last connected device)

## Changes

### 1. Replace Bottle.sample with BLEManager Properties

**Remove line 13:**
```swift
let bottle = Bottle.sample
```

**Update lines 170-191** - Replace:
```swift
Section("Bottle Configuration") {
    HStack {
        Text("Name")
        Spacer()
        Text(bottle.name)
            .foregroundStyle(.secondary)
    }

    HStack {
        Text("Capacity")
        Spacer()
        Text("\(bottle.capacityMl)ml")
            .foregroundStyle(.secondary)
    }

    HStack {
        Text("Daily Goal")
        Spacer()
        Text("\(bottle.dailyGoalMl)ml")
            .foregroundStyle(.secondary)
    }
}
```

With:
```swift
Section("Bottle Configuration") {
    HStack {
        Text("Device")
        Spacer()
        Text(bleManager.connectedDeviceName ?? "Not connected")
            .foregroundStyle(.secondary)
    }

    HStack {
        Text("Capacity")
        Spacer()
        Text("\(bleManager.bottleCapacityMl)ml")
            .foregroundStyle(.secondary)
    }

    HStack {
        Text("Daily Goal")
        Spacer()
        Text("\(bleManager.dailyGoalMl)ml")
            .foregroundStyle(.secondary)
    }
}
```

BLEManager already exposes these `@Published` properties - no changes needed there.

### 2. Remove useOunces Preference

**Remove line 14:**
```swift
@State private var useOunces = false
```

**Remove lines 375-381** (the Toggle in Preferences section):
```swift
Toggle(isOn: $useOunces) {
    HStack {
        Image(systemName: "ruler")
            .foregroundStyle(.orange)
        Text("Use Ounces (oz)")
    }
}
```

### 3. Remove Version String

**Remove lines 393-400** (the Version row in About section):
```swift
HStack {
    Text("Version")
    Spacer()
    Text("1.0.0 (Pull-to-Refresh)")
        .foregroundStyle(.secondary)
        .font(.caption)
}
```

### 4. Keep Device Name After Disconnect

In BLEManager.swift, **remove** the lines that clear `connectedDeviceName` on disconnect:

- Line 350: `connectedDeviceName = nil` (connection timeout)
- Line 456: `connectedDeviceName = nil` (connection failed)
- Line 471: `connectedDeviceName = nil` (disconnected)

This way, the last connected device name persists in memory. The device name (e.g., "Aquavate-A3F2") is based on MAC address and never changes.

**Update SettingsView** to show "Not connected" only when `connectedDeviceName` is truly nil (never connected):
```swift
Text(bleManager.connectedDeviceName ?? "Not connected")
```

## Files to Modify

- [SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift) - Changes 1, 2, 3
- [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) - Change 4 (keep device name)

## Verification

1. Build iOS app: `xcodebuild -project ios/Aquavate/Aquavate.xcodeproj -scheme Aquavate build`
2. Run in simulator - open Settings tab
3. Verify:
   - **Bottle Configuration**: Shows "Device" with device name (or "Not connected"), Capacity and Daily Goal from BLE
   - **Preferences**: Only shows Notifications toggle (no Use Ounces)
   - **About**: Only shows GitHub Repository link (no Version row)
