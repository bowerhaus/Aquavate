# Plan: Enhanced Backpack Mode Display (Issue #38)

## Status: COMPLETE ✅

**Completed:** 2026-01-25

## Summary
Replace the simple "Zzzz.." indicator with a user-friendly "Backpack Mode" screen that explains how to wake the bottle. Optimize display refresh to avoid unnecessary updates when re-entering extended sleep.

**Update (2026-01-25):** Replaced 60-second timer wake with double-tap detection for improved battery efficiency. See "Tap-to-Wake Enhancement" section below.

## Files to Modify

| File | Changes |
|------|---------|
| [firmware/src/main.cpp](firmware/src/main.cpp) | Add RTC flag, modify `enterExtendedDeepSleep()` logic |
| [firmware/src/display.cpp](firmware/src/display.cpp) | Add `displayBackpackMode()` function |
| [firmware/include/display.h](firmware/include/display.h) | Declare `displayBackpackMode()` |

## Implementation Steps

### Step 1: Add RTC Memory Flag (main.cpp)
Add a new RTC variable near line 70 to track whether the backpack screen has been shown:
```cpp
RTC_DATA_ATTR bool rtc_backpack_screen_shown = false;
```

### Step 2: Create Backpack Mode Display Function (display.cpp)

Add a new public function to render the backpack mode screen:

```cpp
void displayBackpackMode() {
    g_display_ptr->clearBuffer();
    g_display_ptr->setTextColor(EPD_BLACK);

    // Title: "Backpack Mode" centered, textSize=2
    g_display_ptr->setTextSize(2);
    const char* title = "Backpack Mode";
    int title_width = strlen(title) * 12;  // 12px per char at textSize=2
    g_display_ptr->setCursor((250 - title_width) / 2, 30);
    g_display_ptr->print(title);

    // Instructions: multi-line, textSize=1, centered
    g_display_ptr->setTextSize(1);
    const char* line1 = "Place me on a flat surface";
    const char* line2 = "and I will wake up";
    const char* line3 = "within a minute";

    int w1 = strlen(line1) * 6;
    int w2 = strlen(line2) * 6;
    int w3 = strlen(line3) * 6;

    g_display_ptr->setCursor((250 - w1) / 2, 60);
    g_display_ptr->print(line1);
    g_display_ptr->setCursor((250 - w2) / 2, 75);
    g_display_ptr->print(line2);
    g_display_ptr->setCursor((250 - w3) / 2, 90);
    g_display_ptr->print(line3);

    g_display_ptr->display();
}
```

Text layout (250x122 display):
- Title "Backpack Mode" at y=30 (13 chars × 12px = 156px wide, centered)
- Line 1 at y=60 (26 chars × 6px = 156px wide)
- Line 2 at y=75 (18 chars × 6px = 108px wide)
- Line 3 at y=90 (15 chars × 6px = 90px wide)

### Step 3: Add Declaration (display.h)

Add after line 37:
```cpp
void displayBackpackMode();
```

### Step 4: Modify enterExtendedDeepSleep() (main.cpp)

Replace lines 352-365 with conditional logic:

```cpp
#if EXTENDED_SLEEP_INDICATOR
    // Only show backpack mode screen on first entry (not on re-entry after timer wake)
    if (!rtc_backpack_screen_shown) {
        Serial.println("Displaying Backpack Mode screen...");
        displayBackpackMode();
        rtc_backpack_screen_shown = true;
        delay(1000);  // Brief pause to ensure display updates
    } else {
        Serial.println("Backpack Mode screen already shown - skipping display refresh");
    }
#endif
```

### Step 5: Reset Flag When Returning to Normal Mode (main.cpp)

In the timer wake handler (around line 851), add flag reset:
```cpp
// Return to normal mode
Serial.println("Extended sleep: Motion stopped - returning to normal mode");
g_in_extended_sleep_mode = false;
g_time_since_stable_start = millis();
rtc_backpack_screen_shown = false;  // Reset for next backpack mode entry
```

### Step 6: Reset Flag on Power-On/Reset (main.cpp)

In setup(), after motion wake handling (around line 820), ensure flag is reset on fresh boot:
```cpp
if (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER) {
    rtc_backpack_screen_shown = false;
}
```

## Display Refresh Behavior (After Implementation)

| Scenario | Display Action |
|----------|----------------|
| First entry to extended sleep | Show "Backpack Mode" screen |
| Timer wake, still moving | No refresh (screen preserved) |
| Timer wake, motion stopped | Normal screen via `g_force_display_clear_sleep` |
| Power cycle / motion wake | Reset flag, ready for next backpack entry |

## Verification

1. **Build firmware**: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. **Test first entry**: Shake bottle continuously for 3+ minutes, observe "Backpack Mode" screen appears
3. **Test no refresh on re-entry**: Watch serial output - should see "skipping display refresh" on subsequent timer wakes while moving
4. **Test return to normal**: Place bottle flat, wait ~1 minute, verify normal screen returns
5. **Test power cycle**: Power off/on, then trigger backpack mode again - screen should appear (not skipped)

---

## Tap-to-Wake Enhancement (2026-01-25)

### Why Double-Tap?
The original implementation used 60-second timer wakes, which:
- Consumed battery with periodic wakes
- Required checking `isStillMovingConstantly()` each wake
- Added complexity to wake handling

Double-tap detection provides:
- **Maximum battery savings** - no periodic wakes at all
- **Simple implementation** - one wake source
- **Clear user feedback** - display shows tap instructions
- **Deliberate gesture** - won't trigger from backpack vibrations

### Additional Files Modified

| File | Changes |
|------|---------|
| [firmware/src/config.h](../firmware/src/config.h) | Add tap detection constants, remove `EXTENDED_SLEEP_TIMER_SEC` |
| [firmware/src/main.cpp](../firmware/src/main.cpp) | Add `configureADXL343TapWake()`, modify wake handling |
| [firmware/include/display.h](../firmware/include/display.h) | Add `displayTapWakeFeedback()` declaration |

### Config Constants Added (config.h)
```cpp
#define TAP_WAKE_THRESHOLD          0x30    // 3.0g threshold (firm tap)
#define TAP_WAKE_DURATION           0x10    // 10ms max duration
#define TAP_WAKE_LATENT             0x50    // 100ms between taps
#define TAP_WAKE_WINDOW             0xF0    // 300ms window for second tap
```

### Updated Display Text (display.cpp)
The backpack mode screen now shows:
- Title: "backpack mode" (textSize=3, lowercase)
- Instructions: "double-tap firmly / to wake up" (textSize=2)
- Note: "allow five seconds to wake" (textSize=1)

### Tap Wake Feedback (display.cpp)
New `displayTapWakeFeedback()` function shows "waking" immediately on tap detection, before sensor initialization completes.

### RTC Variable Added (main.cpp)
```cpp
RTC_DATA_ATTR bool rtc_tap_wake_enabled = false;
```

### Wake Handling Logic
- When entering backpack mode: configure ADXL343 for double-tap, set `rtc_tap_wake_enabled = true`
- On EXT0 wake: check `rtc_tap_wake_enabled` to distinguish tap vs motion wake
- After tap wake: restore motion detection, clear flag

### Code Removed
- `EXTENDED_SLEEP_TIMER_SEC` constant (dead code)
- `g_extended_sleep_timer_sec` variable (dead code)
- `isStillMovingConstantly()` function (no longer needed)
- Timer wake in `enterExtendedDeepSleep()` (replaced by tap)

### Updated Verification Steps
1. **Test backpack mode entry**: Keep bottle moving for 3 minutes, display shows backpack instructions
2. **Test tap wake**: Double-tap firmly, see "waking" feedback, then normal screen
3. **Test false positive rejection**: Single taps, bumps, shaking should NOT wake
4. **Test normal sleep regression**: Normal sleep still wakes on motion (not requiring tap)
