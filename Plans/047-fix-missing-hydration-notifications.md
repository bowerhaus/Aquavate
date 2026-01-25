# Plan: Fix Missing Hydration Reminder Notifications

## Problem Summary

User reports:
1. "50ml behind target" appears on device display but no notification on phone
2. Reminders count appears to be incrementing but no notifications displayed

## Root Cause Analysis

The notification evaluation is **not triggered when drink sync completes**. Currently:

1. **Drink detected** → Firmware sends Current State via BLE
2. **iOS receives** → `handleCurrentStateUpdate()` updates `dailyTotalMl` (this is what shows "50ml behind")
3. **Drink sync completes** → `completeSyncTransfer()` calls:
   - `updateState()` - updates values, recalculates urgency
   - `checkBackOnTrack()` - sends "back on track" notification if applicable
   - `goalAchieved()` - sends goal notification if applicable
   - **Missing: `evaluateAndScheduleReminder()`** - hydration reminder never triggered!

4. **Periodic evaluation** → Runs every 60 seconds, but by then the moment may have passed

## The Fix

Add a call to `evaluateAndScheduleReminder()` in `completeSyncTransfer()` after updating state.

## Files to Modify

### `ios/Aquavate/Aquavate/Services/BLEManager.swift`

In `completeSyncTransfer()` (~line 1319), after the goal check, add:

```swift
// Evaluate if a hydration reminder should be sent
hydrationReminderService?.evaluateAndScheduleReminder()
```

This ensures that after every drink sync:
- If user is behind pace, a reminder notification is evaluated
- If user caught up, they get a "back on track" notification (already implemented)
- If goal achieved, they get a celebration (already implemented)

## Verification

1. Build iOS app with the fix
2. Connect to bottle
3. Ensure user is behind pace (50ml+ deficit)
4. Pour a small drink that keeps user behind pace
5. Verify notification is scheduled (check Xcode console for "[Notifications] Scheduled" log)
6. Verify notification appears on phone

## Notes

- The escalation logic (attention → overdue) remains unchanged - notifications only escalate, don't repeat
- This fix adds immediate evaluation after sync, complementing the 60-second periodic timer
