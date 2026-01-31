# Plan: Fix Backpack Mode Entry -- Sleep Mode Based on Bottle State

## Problem

When the bottle is laid horizontally (or in a bag in any orientation), backpack mode is never entered. The bottle keeps waking every 30-60 seconds, creating many normal sleep sessions.

## Root Cause

At [main.cpp:662](firmware/src/main.cpp#L662), every normal motion wake resets `g_time_since_stable_start = millis()`, wiping the backpack timer to zero. The 30s activity timeout fires long before the 180s threshold, so backpack mode is unreachable through repeated short wake cycles.

## Solution: Sleep Mode Matches Bottle State

**Core principle:** Normal sleep (motion wake) is only appropriate when the bottle is on a surface being used. If the bottle hasn't been UPRIGHT_STABLE for several consecutive wake cycles, it's not being used normally -- switch to extended sleep (double-tap wake).

### Mechanism

Track consecutive wake cycles where UPRIGHT_STABLE was never achieved. After a threshold number of non-stable cycles, enter extended sleep instead of normal sleep.

**A wake cycle is "stable" if any of these occur:**
- UPRIGHT_STABLE gesture achieved
- Drink detected
- BLE data activity (sync/commands)

**Combined with the existing 180s within-cycle timer**, this gives complete coverage:
- Counter: handles repeated-wake scenarios (horizontal on desk, in bag, car)
- Existing 180s timer: handles continuous motion (walking with backpack)

With wakes every 30-60s and a threshold of 4, backpack mode is entered in ~2-4 minutes.

## Implementation

### 1. New RTC variable and config constant

**[config.h](firmware/src/config.h)** -- add after line 134:
```cpp
#define SPURIOUS_WAKE_THRESHOLD 4   // Consecutive non-stable wakes before extended sleep
```

**[main.cpp](firmware/src/main.cpp)** -- add near existing RTC vars (~line 68-75):
```cpp
RTC_DATA_ATTR uint8_t rtc_spurious_wake_count = 0;
```

New global (RAM):
```cpp
bool g_wake_was_useful = false;
```

### 2. Wake handling changes

**[main.cpp:648-674](firmware/src/main.cpp#L648-L674)** -- modify the wake reason handling:

- On **motion wake** (EXT0, not tap): keep existing resets, initialize `g_wake_was_useful = false`. Preserve `rtc_spurious_wake_count` from RTC (don't reset it).
- On **tap wake**: reset `rtc_spurious_wake_count = 0` (user deliberately woke the bottle).
- On **power cycle**: reset `rtc_spurious_wake_count = 0`.

### 3. Mark wake as useful during loop

At the existing locations where user interaction is detected:

- **UPRIGHT_STABLE** (~line 1892): add `g_wake_was_useful = true; rtc_spurious_wake_count = 0;`
- **Drink detected** (~line 1654): add `g_wake_was_useful = true; rtc_spurious_wake_count = 0;`
- **BLE data activity** (~line 1193): add `g_wake_was_useful = true; rtc_spurious_wake_count = 0;`

### 4. Sleep entry decision

**[main.cpp:~1956](firmware/src/main.cpp#L1956)** -- in the normal sleep entry block (after `if (!sleep_blocked)`), before `enterDeepSleep()`:

```cpp
if (!g_wake_was_useful) {
    rtc_spurious_wake_count++;
    Serial.printf("Non-stable wake #%d/%d\n",
                  rtc_spurious_wake_count, SPURIOUS_WAKE_THRESHOLD);

    if (rtc_spurious_wake_count >= SPURIOUS_WAKE_THRESHOLD) {
        Serial.println("Bottle not in use - entering extended sleep (backpack mode)");
        g_in_extended_sleep_mode = true;
        rtc_spurious_wake_count = 0;
        enterExtendedDeepSleep();
        // Does not return
    }
} else {
    rtc_spurious_wake_count = 0;
}

// Existing normal sleep code follows
enterDeepSleep();
```

### 5. RTC persistence

`rtc_spurious_wake_count` uses `RTC_DATA_ATTR`, automatically persisting across deep sleep. No explicit save/restore needed.

## Files to Modify

| File | Changes |
|------|---------|
| [main.cpp](firmware/src/main.cpp) | Add RTC var + RAM flag (~2 lines), init in wake handling (~3 lines), mark useful at 3 locations (~6 lines), sleep entry check (~15 lines) |
| [config.h](firmware/src/config.h) | Add `SPURIOUS_WAKE_THRESHOLD` constant (~1 line) |

**Total: ~27 lines of changes across 2 files**

## Verification

1. **Build**: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. User uploads firmware and tests:
   - **Normal use**: Pick up, drink, put down upright → normal sleep (counter resets on UPRIGHT_STABLE)
   - **Horizontal on desk**: Lay bottle on side → extended sleep after ~4 non-stable wakes (2-4 min)
   - **Carry and return**: Carry bottle, put back upright → counter resets, normal sleep
   - **Double-tap exit**: Once in extended sleep, double-tap wakes normally
   - **Serial monitor**: Look for `Non-stable wake #N/4` messages
