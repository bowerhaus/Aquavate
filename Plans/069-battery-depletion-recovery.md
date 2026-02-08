# Fix: Auto-recovery after battery depletion

## Context

The bottle is a sealed autonomous device with no accessible reset button. When the battery dies and USB charger is reconnected, the ESP32 doesn't restart — the only workaround is re-uploading firmware (which forces a hard reset via DTR/RTS).

**Root cause:** The ADXL343 accelerometer loses its interrupt configuration when its supply voltage drops below ~2.0V, but the ESP32's RTC domain can survive on residual charge (~10μA). This means:

1. Device enters deep sleep — ADXL343 configured to drive INT pin HIGH on motion/tap
2. Battery voltage drops below ADXL343 minimum → accelerometer resets to defaults, interrupt config lost
3. ESP32 RTC domain still alive → still in deep sleep, waiting for GPIO 27 HIGH (EXT0 wake)
4. Charger reconnected, user tilts bottle — but ADXL343 INT pin stays LOW (no interrupt configured)
5. ESP32 never gets wake signal → stuck forever

Neither sleep mode has a fallback timer, so the device stays asleep indefinitely.

## Solution: Periodic health-check timer wake

Add a timer wake source to **both** deep sleep modes so the device periodically wakes up, even without motion or tap input. This ensures the device recovers automatically after power is restored.

## Changes

### 1. Add config constant — [config.h](firmware/src/config.h)

Add to the Power Management section (~line 134, after `TIME_SINCE_STABLE_THRESHOLD_SEC`):

```cpp
// Health-check wake: periodic timer wake in all deep sleep modes
// Ensures device auto-recovers after battery depletion + recharge
// Battery impact: ~1mAh/day (negligible vs 400-1000mAh LiPo)
#define HEALTH_CHECK_WAKE_INTERVAL_SEC  7200    // 2 hours
```

### 2. Add RTC flag to distinguish health-check from rollover — [main.cpp](firmware/src/main.cpp) (~line 73)

```cpp
RTC_DATA_ATTR bool rtc_health_check_wake = false;  // True if timer was set for health check (not rollover)
```

### 3. Modify `enterDeepSleep()` — [main.cpp](firmware/src/main.cpp:454)

Replace the current rollover-only timer logic (lines 504-517) with:

```cpp
// Configure timer wake: use minimum of rollover time and health-check interval
uint32_t timer_seconds = HEALTH_CHECK_WAKE_INTERVAL_SEC;
rtc_health_check_wake = true;

uint32_t seconds_until_rollover = getSecondsUntilRollover();
if (seconds_until_rollover > 0 && seconds_until_rollover < 24 * 3600) {
    uint32_t rollover_with_buffer = seconds_until_rollover + 60;
    if (rollover_with_buffer < timer_seconds) {
        timer_seconds = rollover_with_buffer;
        rtc_health_check_wake = false;  // This is a rollover wake
    }
    rtc_rollover_wake_pending = true;
} else {
    rtc_rollover_wake_pending = false;
}

uint64_t timer_us = (uint64_t)timer_seconds * 1000000ULL;
esp_sleep_enable_timer_wakeup(timer_us);
Serial.printf("Sleep timer set: %lu seconds (%s)\n", timer_seconds,
              rtc_health_check_wake ? "health check" : "rollover");
```

### 4. Modify `enterExtendedDeepSleep()` — [main.cpp](firmware/src/main.cpp:411)

Add timer wake **alongside** the existing EXT0 tap wake (before `esp_deep_sleep_start()`):

```cpp
// Add periodic health-check timer (ESP32 supports multiple wake sources)
uint64_t timer_us = (uint64_t)HEALTH_CHECK_WAKE_INTERVAL_SEC * 1000000ULL;
esp_sleep_enable_timer_wakeup(timer_us);
rtc_health_check_wake = true;
Serial.printf("Health-check timer set: %d seconds\n", HEALTH_CHECK_WAKE_INTERVAL_SEC);
```

### 5. Update timer wake handling in `setup()` — [main.cpp](firmware/src/main.cpp:678)

Replace the current timer wake case:

```cpp
} else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    if (rtc_health_check_wake) {
        Serial.println("Timer wake detected (health check)");
        // Health check: go through normal boot, will auto-sleep after inactivity timeout
        // If in backpack mode, preserve that state
        if (rtc_tap_wake_enabled) {
            // Was in extended sleep - stay in backpack mode context
            Serial.println("  (from backpack mode - will re-evaluate)");
        }
        g_time_since_stable_start = millis();
    } else {
        Serial.println("Timer wake detected (daily rollover)");
    }
}
```

## What happens on health-check wake

1. Device boots normally through `setup()`
2. All sensors initialize, display updates if needed
3. No user interaction → activity timeout expires after 30s
4. Device goes back to sleep with another health-check timer
5. Cycle continues — device is alive and will respond to motion/tap as normal

After battery depletion + charger reconnect: within at most 2 hours the device wakes, boots, and resumes normal operation.

## Verification

1. Build firmware: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. Upload and monitor serial output
3. Let device enter normal deep sleep — confirm serial shows "Sleep timer set: N seconds (health check)" or "(rollover)" with the shorter value
4. Let device enter extended/backpack sleep — confirm serial shows health-check timer alongside tap wake
5. Wait for a health-check wake (or temporarily set interval to 60s for testing) — confirm device wakes, boots normally, then re-sleeps after 30s
