# Plan: Change Daily Rollover to Midnight (Issue #47)

## Summary

Change the daily rollover time from 4am to midnight (0) to align with HealthKit's day boundaries. This is a simple constant change in 2 files.

## Background

The original 4am boundary was chosen to handle late-night drinking (drinks at 1am count as "yesterday"). However, this causes confusion when comparing Aquavate totals with Apple Health, which uses midnight. HealthKit alignment is more valuable for most users than the late-night edge case.

---

## Implementation

### Firmware

**[firmware/src/config.h](firmware/src/config.h)** line 240:
```cpp
#define DRINK_DAILY_RESET_HOUR    0    // Reset at midnight (was 4)
```

### iOS App

**[ios/Aquavate/Aquavate/Services/BLEConstants.swift](ios/Aquavate/Aquavate/Services/BLEConstants.swift)** line 149:
```swift
static let dailyResetHour = 0    // Midnight (was 4)
```

Also update the documentation comments in the Calendar extension (lines 152-159) to reflect midnight instead of 4am.

---

## Files to Modify

| File | Change |
|------|--------|
| [firmware/src/config.h](firmware/src/config.h) | Change `DRINK_DAILY_RESET_HOUR` from `4` to `0` |
| [ios/.../BLEConstants.swift](ios/Aquavate/Aquavate/Services/BLEConstants.swift) | Change `dailyResetHour` from `4` to `0`, update comments |

---

## Verification

1. **Build firmware**: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. **Build iOS app**: Open Xcode, build and run on device
3. **Test**:
   - Verify drinks at 11pm count as "today"
   - Verify drinks at 12:30am count as "tomorrow" (the new day)
   - Verify daily total resets at midnight
   - Verify bottle timer wake happens at midnight (not 4am)
   - Compare with Apple Health - totals should now align
