# Plan: GitHub Issue #67 - Adjust Hydration Notification Thresholds

## Summary
Update minimum deficit thresholds for hydration reminder notifications.

## Requirements
| Mode | Current | New |
|------|---------|-----|
| Production | 50ml | **150ml** |
| DEBUG, Early Notifications OFF | 50ml | **150ml** |
| DEBUG, Early Notifications ON | 10ml | **50ml** |

The "Early Notifications" toggle remains DEBUG-only.

## File to Modify
[ios/Aquavate/Aquavate/Services/HydrationReminderService.swift](ios/Aquavate/Aquavate/Services/HydrationReminderService.swift)

## Changes

### 1. Update test mode constant (line 21)
```swift
// Before:
static let testModeMinimumDeficit = 10

// After:
static let testModeMinimumDeficit = 50
```

### 2. Update production threshold (line 75)
```swift
// Before:
return testModeEnabled ? Self.testModeMinimumDeficit : 50

// After:
return testModeEnabled ? Self.testModeMinimumDeficit : 150
```

### 3. Update release build threshold (line 77)
```swift
// Before:
return 50

// After:
return 150
```

## Verification
1. Build the app in DEBUG mode
2. With Early Notifications OFF: Confirm no notification until 150ml behind pace
3. With Early Notifications ON: Confirm notification at 50ml behind pace
4. Build in Release mode: Confirm 150ml threshold applies
