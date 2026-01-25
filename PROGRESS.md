# Aquavate - Active Development Progress

**Last Updated:** 2026-01-25
**Current Branch:** `ios-memory-exhaustion-fix-v2`

---

## Current Task

**Fix iOS App Memory Exhaustion** - [Plan 052](Plans/052-ios-memory-exhaustion-fix.md) - GitHub Issue #28

iOS app was being killed after ~31 minutes due to memory exhaustion. Fixed memory leaks from WatchConnectivity queue accumulation, CoreData @FetchRequest loading all records, NotificationManager strong self captures, and BLEManager delete task accumulation.

### Progress

- [x] Plan approved and copied to Plans/052-ios-memory-exhaustion-fix.md
- [x] Created branch `ios-memory-exhaustion-fix-v2` from master
- [x] Fix 1: Limit WatchConnectivity transferUserInfo queue size (max 5 pending)
- [x] Fix 2: Add predicates to HomeView @FetchRequest (today's records only)
- [x] Fix 3: Add predicates to HistoryView @FetchRequest (last 7 days only)
- [x] Fix 4: Add weak self captures to NotificationManager callbacks
- [x] Fix 5: Cancel previous delete timeout Task in BLEManager
- [x] Fix 6: Optimize HistoryView to compute totals directly from CoreData (avoid intermediate objects)
- [x] Build verified - compiles successfully
- [x] Tested on device - 60+ minutes, persistent memory stable (~43 MiB), no crash

### Status

**READY FOR PR** - All fixes applied and tested. App ran 60+ minutes without memory exhaustion. Instruments confirmed persistent memory stable at ~43 MiB with only 0.31 MiB increase over the test period.

### Files Modified

1. `ios/Aquavate/Aquavate/Services/WatchConnectivityManager.swift` - Queue limit check (max 5 pending)
2. `ios/Aquavate/Aquavate/Views/HomeView.swift` - @FetchRequest predicate (today only)
3. `ios/Aquavate/Aquavate/Views/HistoryView.swift` - @FetchRequest predicate (7 days) + optimized totals
4. `ios/Aquavate/Aquavate/Services/NotificationManager.swift` - Weak self captures
5. `ios/Aquavate/Aquavate/Services/BLEManager.swift` - Cancel previous delete Task

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md. iOS memory exhaustion fix (Issue #28) is complete and tested. Ready for PR.
```

---

## Recently Completed

- ✅ Foreground Notification Fix (Issue #56) - [Plan 051](Plans/051-foreground-notification-fix.md)
  - Fixed hydration reminders not appearing when app is in foreground
  - Added UNUserNotificationCenterDelegate to NotificationManager
  - Implemented willPresent delegate to return banner/sound/badge options
  - Added didReceive delegate for handling notification taps

- ✅ Single-Tap Wake for Normal Sleep (Issue #63) - [Plan 050](Plans/050-single-tap-wake.md)
  - Added single-tap detection alongside motion wake for normal sleep mode
  - INT_ENABLE changed from 0x10 (activity only) to 0x50 (activity + single-tap)
  - Tap threshold: 3.0g (same as backpack mode double-tap)
  - Updated iOS "Bottle is Asleep" alert: "Tap or tilt your bottle to wake it up"
  - Fixed LIS3DH → ADXL343 references in documentation

- ✅ Enhanced Backpack Mode with Tap-to-Wake (Issue #38) - [Plan 039](Plans/039-enhanced-backpack-mode-display.md)
  - Backpack mode screen shows "backpack mode" with "double-tap firmly to wake up" instructions
  - RTC flag prevents redundant display refreshes on re-entry
  - Replaced 60-second timer wake with double-tap detection for better battery
  - ADXL343 reconfigured for double-tap interrupt in backpack mode
  - Immediate "waking" feedback on tap detection
  - Removed dead code: `EXTENDED_SLEEP_TIMER_SEC` constant and variable

---

## Branch Status

- `master` - Stable baseline
- `ios-memory-exhaustion-fix-v2` - Ready to merge (Issue #28 fix)

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
