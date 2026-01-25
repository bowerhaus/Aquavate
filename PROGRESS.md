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

- ✅ Midnight Rollover for HealthKit Alignment (Issue #47) - [Plan 049](Plans/049-midnight-rollover.md)
  - Changed daily reset from 4am to midnight to align with Apple Health
  - Updated firmware config.h and iOS BLEConstants.swift
  - Updated PRD.md and IOS-UX-PRD.md documentation

- ✅ Human Figure Fill Fix (Issue #59) - [Plan 048](Plans/048-human-figure-fill-fix.md)
  - Fixed white gap at top of head when goal reached/exceeded
  - SwiftUI `Spacer()` default minLength caused fill to not reach 100%
  - Changed to `Spacer(minLength: 0)` in HumanFigureProgressView.swift

---

## Branch Status

- `master` - Stable baseline

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
