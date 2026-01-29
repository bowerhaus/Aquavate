# Fix: E-paper display redraws on every wake (Issue #88)

## Root Cause

After deep sleep, the ESP32 restarts and the static variable `g_daily_goal_ml` in `display.cpp` resets to the compile-time default (`DRINK_DAILY_GOAL_DEFAULT_ML` = 2500). When `displaySetDailyGoal(bleGetDailyGoalMl())` is then called with the NVS-persisted goal (e.g. 3000ml set via iOS), it sees 3000 != 2500 and sets `g_daily_goal_changed = true` — triggering a spurious redraw on every single wake.

Secondary issue: the `g_daily_goal_changed` flag is checked/cleared in two independent code paths (`displayCheckGoalChanged()` and inside `displayNeedsUpdate()`), which is confusing and non-deterministic.

## File to Modify

`firmware/src/display.cpp` — all four changes are in this single file.

## Changes

### 1. Add RTC variable for daily goal (after line 46)

Add `RTC_DATA_ATTR uint16_t rtc_display_daily_goal = 0;` after the existing `rtc_wake_count` declaration.

### 2. Save daily goal in `displaySaveToRTC()` (line ~666)

Add `rtc_display_daily_goal = g_daily_goal_ml;` before the magic number write, alongside the other state saves.

### 3. Restore daily goal in `displayRestoreFromRTC()` (line ~689)

Add `g_daily_goal_ml = rtc_display_daily_goal;` after restoring `battery_percent`, alongside the other state restores.

### 4. Remove dual-check from `displayNeedsUpdate()` (lines 555-560)

Remove the `g_daily_goal_changed` check block (the "2b" section) from `displayNeedsUpdate()`. The flag is already handled by `displayCheckGoalChanged()` in the main loop (main.cpp:1731), which explicitly does a `displayForceUpdate()`. Having two consumers of the same flag is confusing and non-deterministic.

## Why This Works

After the fix, the wake sequence is:
1. `displayInit()` — `g_daily_goal_ml` defaults to 2500
2. `displayRestoreFromRTC()` — **restores `g_daily_goal_ml` to the actual value** (e.g. 3000)
3. `displaySetDailyGoal(bleGetDailyGoalMl())` — NVS returns 3000, compares against 3000 from RTC — **no change** — no flag set — no spurious redraw

## Edge Cases

- **Power cycle** (no valid RTC): restore returns false, goal stays at default (2500). If user had a custom goal, `displaySetDailyGoal()` correctly detects the real change. A full redraw on power cycle is expected.
- **Goal change via iOS while awake**: BLE callback sets the flag, main loop detects via `displayCheckGoalChanged()`, forces update. Works correctly.
- **Goal at default (2500)**: RTC saves 2500, restores 2500, NVS returns 2500. No change.

## Verification

1. Build: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. Upload, set a custom daily goal via iOS app
3. Let bottle sleep, then wake it
4. Serial output should NOT show "Display: Daily goal changed" message
5. E-paper should NOT redraw unless actual content changed
