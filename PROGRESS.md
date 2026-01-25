# Aquavate - Active Development Progress

**Last Updated:** 2026-01-25
**Current Branch:** `master` (after merge)

---

## Current Task

None - ready for next issue.

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md. Ready for next task.
```

---

## Recently Completed

- ✅ Enhanced Backpack Mode with Tap-to-Wake (Issue #38) - [Plan 039](Plans/039-enhanced-backpack-mode-display.md)
  - Backpack mode screen shows "backpack mode" with "double-tap firmly to wake up" instructions
  - RTC flag prevents redundant display refreshes on re-entry
  - Replaced 60-second timer wake with double-tap detection for better battery
  - ADXL343 reconfigured for double-tap interrupt in backpack mode
  - Immediate "waking" feedback on tap detection
  - Removed dead code: `EXTENDED_SLEEP_TIMER_SEC` constant and variable

- ✅ Midnight Rollover for HealthKit Alignment (Issue #47) - [Plan 049](Plans/049-midnight-rollover.md)
  - Changed daily reset from 4am to midnight to align with Apple Health
  - Updated firmware config.h and iOS BLEConstants.swift
  - Updated PRD.md and IOS-UX-PRD.md documentation

- ✅ Human Figure Fill Fix (Issue #59) - [Plan 048](Plans/048-human-figure-fill-fix.md)
  - Fixed white gap at top of head when goal reached/exceeded
  - SwiftUI `Spacer()` default minLength caused fill to not reach 100%
  - Changed to `Spacer(minLength: 0)` in HumanFigureProgressView.swift

- ✅ Bottle Level Recent Indicator (Issue #57) - [Plan 047](Plans/047-bottle-level-recent-indicator.md)
  - Shows last known bottle level with "(recent)" suffix when disconnected
  - Hides bottle level section entirely until first connection
  - Persists `bottleLevelMl` and `hasReceivedBottleData` to UserDefaults
  - Updated IOS-UX-PRD.md Section 2.4

- ✅ Daily Rollover Timer Wake - [Plan 046](Plans/046-daily-rollover-timer-wake.md)
  - Wake bottle at midnight daily rollover to refresh display with reset daily total
  - ESP32 timer wake source alongside motion wake (wakes on first to trigger)
  - Returns to sleep immediately after display refresh (no BLE advertising)
  - Updated PRD.md Section 2 Wake Triggers

---

## Known Issues

**Wake-on-tilt sensitivity (ADXL343):**
- Requires deliberate shake (~2 seconds) due to aggressive filtering
- Trade-off accepted for battery life benefits

---

## Branch Status

- `master` - Stable baseline

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
