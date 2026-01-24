# Daily Rollover Timer Wake Implementation

## Problem
When the bottle is asleep during the 4am daily rollover, the display doesn't refresh to show the reset daily intake (0ml) until the user manually interacts with the bottle.

## Solution
Add a timer wake source for the daily rollover time alongside the existing motion wake during normal deep sleep. The ESP32 supports multiple wake sources simultaneously - it will wake on whichever triggers first.

## Implementation Plan

### 1. Add RTC Memory Flag (main.cpp)
Add to RTC_DATA_ATTR section (~line 68):
```cpp
RTC_DATA_ATTR bool rtc_rollover_wake_pending = false;
```
This tracks whether we set a rollover timer to distinguish rollover wake from other timer wakes.

### 2. Add Helper Function (drinks.cpp)
Create `getSecondsUntilRollover()` to calculate seconds until next 4am:
- Uses existing `DRINK_DAILY_RESET_HOUR` constant
- Returns 0 if time not set (no timer wake will be added)
- If past 4am today, calculates tomorrow's 4am

Export in drinks.h.

### 3. Modify enterDeepSleep() (main.cpp, lines 390-442)
After existing EXT0 motion wake setup, add:
```cpp
// Timer wake for daily rollover
uint32_t seconds_until_rollover = getSecondsUntilRollover();
if (seconds_until_rollover > 0 && seconds_until_rollover < 24 * 3600) {
    esp_sleep_enable_timer_wakeup((seconds_until_rollover + 60) * 1000000ULL);
    rtc_rollover_wake_pending = true;
}
```
- Adds 60-second buffer to ensure we're past the 4am boundary
- Both wake sources active simultaneously - ESP32 wakes on first to trigger

### 4. Modify setup() Wake Handling (main.cpp, lines 558-595)
Add rollover wake detection:
- Check if `wakeup_reason == ESP_SLEEP_WAKEUP_TIMER && rtc_rollover_wake_pending`
- Verify we're within 5 minutes of 4am (sanity check)
- Clear `rtc_rollover_wake_pending` flag
- Distinguish from extended sleep timer wake using `g_in_extended_sleep_mode`

### 5. Add Rollover Display Refresh (main.cpp)
When rollover wake detected:
1. Recalculate daily totals (will be 0)
2. Force display update with reset daily total
3. Skip BLE advertising (no user present)
4. Return to deep sleep immediately

## Files to Modify

| File | Changes |
|------|---------|
| [main.cpp](firmware/src/main.cpp) | Add RTC flag, modify enterDeepSleep(), add rollover wake handling in setup() |
| [drinks.cpp](firmware/src/drinks.cpp) | Add getSecondsUntilRollover() function |
| [drinks.h](firmware/include/drinks.h) | Export getSecondsUntilRollover() declaration |

## Edge Cases Handled
- **Time not set:** No timer wake added (returns 0)
- **Extended sleep mode:** Check `g_in_extended_sleep_mode` to avoid confusion with extended sleep timer
- **Motion wake at 4am:** Check `rtc_rollover_wake_pending` flag
- **Power cycle:** RTC flag cleared, no false rollover detection

## Power Impact
- One additional wake per day (~1-2 seconds)
- No BLE advertising during rollover wake
- Estimated: < 0.1mAh per day additional drain

## Verification
1. Set device time to 3:58am, enter sleep, verify wake at ~4:01am with 0ml displayed
2. Test motion wake still works before 4am
3. Test extended sleep mode not affected
4. Build and verify IRAM usage stays within limits
