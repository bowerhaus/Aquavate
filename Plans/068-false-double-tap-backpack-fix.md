# Fix: False Double-Tap Triggering Backpack Mode (Issue #103)

## Summary

Gate double-tap backpack entry on prior UPRIGHT_STABLE detection within the current wake cycle. This prevents false triggers from the physical impact of setting the bottle down on a surface, while preserving intentional double-tap entry.

## Root Cause Analysis

The issue is **NOT** the 180-second backpack timer. It's the **hardware double-tap detection** (added in Issue #99 / Plan 065) triggering from the physical impact of setting the bottle down on a surface.

### Evidence from screenshot
- 09:02 — 14s Normal → enters Backpack mode
- 09:02 — 20s Backpack → user taps to exit
- 09:03 — 4m 13s Normal

The 14-second session is too short for the 180s timer (needs 180s) and shorter than the 30s activity timeout. The only code path that enters backpack mode instantly is the **GESTURE_DOUBLE_TAP handler** at main.cpp:1310-1329.

### How it happens
1. Bottle wakes from normal sleep (motion/tilt)
2. User drinks, then sets bottle down on a hard surface (~14 seconds later)
3. The impact creates a classic double-tap pattern: initial contact → brief bounce (50-200ms) → re-contact
4. ADXL343 hardware registers this as a double-tap (3.0g threshold, 100-400ms timing window)
5. Main loop reads INT_SOURCE at main.cpp:1278, sees bit 5 set → `GESTURE_DOUBLE_TAP`
6. Handler at main.cpp:1326-1329 immediately calls `enterExtendedDeepSleep()`

### Why the 3.0g threshold isn't sufficient
Plan 065 assumed "Normal handling is 0.5-1.0g, accidental bumps are 1-2g." This is true for lateral handling, but **setting a bottle down on a hard surface easily exceeds 3g** — the deceleration from even a gentle placement creates sharp spikes. The bounce-and-re-contact timing naturally falls within the ADXL343's double-tap latency window (100-400ms).

## Proposed Fix

**Gate double-tap backpack entry on prior UPRIGHT_STABLE detection within the current wake cycle.**

The logic: if the bottle hasn't been placed down and settled (UPRIGHT_STABLE = upright + stable for 2+ seconds) during this wake cycle, then any double-tap is likely from the act of setting it down (false positive), not from a deliberate user tap.

### Changes to firmware/src/main.cpp

#### 1. Add tracking flag (near line 99, with other global state)
```cpp
bool g_has_been_upright_stable = false;  // Gate for double-tap backpack entry
```
This is a regular global (reset to false each boot/wake from deep sleep). No RTC persistence needed.

#### 2. Set flag when UPRIGHT_STABLE detected (line 1941-1943)
```cpp
if (gesture == GESTURE_UPRIGHT_STABLE) {
    g_has_been_upright_stable = true;  // Enable double-tap backpack entry
    g_time_since_stable_start = millis();
}
```

#### 3. Guard the double-tap handler (line 1326)
Change the guard from:
```cpp
if (!sleep_blocked) {
```
To:
```cpp
if (!sleep_blocked && g_has_been_upright_stable) {
```

Add an else-if log for the ignored case:
```cpp
} else if (!g_has_been_upright_stable) {
    Serial.println("Double-tap: Ignored - bottle not yet placed on surface this wake cycle");
}
```

### What this fixes
| Scenario | Before Fix | After Fix |
|----------|-----------|-----------|
| Pick up → drink → set down (impact) | FALSE double-tap → backpack mode | Ignored (no prior UPRIGHT_STABLE) |
| Bottle on desk (awake) → deliberate double-tap | Enters backpack mode | Enters backpack mode (UPRIGHT_STABLE already achieved) |
| Pick up → set down → wait 2s → deliberate double-tap | Enters backpack mode | Enters backpack mode (UPRIGHT_STABLE achieved after set-down) |
| Continuous motion (bag) | 180s timer enters backpack | Unchanged — 180s timer still works |

### Why not other approaches
- **Time-based cooldown**: Arbitrary duration, doesn't adapt to actual user behavior
- **Higher threshold**: Surface impacts at 4-5g still produce bounce patterns; diminishing returns
- **Disable double-tap entirely**: Removes a useful feature; too heavy-handed

## Files Modified
- firmware/src/main.cpp — 3 small changes (~5 lines total)

## Verification
1. Build firmware: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. Upload and test:
   - Pick up bottle → set down firmly → should NOT enter backpack mode
   - Set bottle on surface → wait 2+ seconds → double-tap → should enter backpack mode
   - Leave bottle in motion for 3+ minutes → should auto-enter backpack mode (180s timer unchanged)
3. Check serial output for "Double-tap: Ignored" messages when setting bottle down
