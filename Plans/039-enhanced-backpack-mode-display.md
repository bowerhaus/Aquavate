# Plan: Enhanced Backpack Mode Display (Issue #38)

## Summary
Replace the simple "Zzzz.." indicator with a user-friendly "Backpack Mode" screen that explains how to wake the bottle. Optimize display refresh to avoid unnecessary updates when re-entering extended sleep.

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
