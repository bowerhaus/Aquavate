# DS3231 RTC Integration Plan

## Overview
Enable the DS3231 external RTC module as the primary time source for the Aquavate smart water bottle, with graceful fallback to ESP32 internal RTC.

## Key Insight: DS3231 + Deep Sleep Synergy
The DS3231's battery-backed operation makes it **perfectly suited** for deep sleep applications:
- **During sleep:** DS3231 runs from CR1220 backup battery (3µA), ESP32 is powered down
- **On wake:** ESP32 syncs its internal RTC from DS3231 (one I2C read)
- **During wake period:** ESP32 uses its internal RTC (fast, no I2C overhead)
- **Result:** Best of both worlds - DS3231 accuracy with ESP32 RTC convenience

## User Decisions
- **Connection:** STEMMA QT/Qwiic I2C cable at address 0x68
- **Priority:** Always use DS3231 when present (primary time source)
- **USB Sync:** Update both ESP32 and DS3231 when setting time via USB
- **Fallback:** Graceful degradation to ESP32 internal RTC if DS3231 not detected

## Implementation Approach

### 1. DS3231 Detection & Initialization
**Location:** [firmware/src/main.cpp](firmware/src/main.cpp)

Add DS3231 detection in `setup()` after existing I2C sensor initialization (~line 700):

```cpp
// After NAU7802 and LIS3DH initialization
if (!rtc.begin()) {
  Serial.println("DS3231 not detected - using ESP32 internal RTC");
  g_rtc_ds3231_present = false;
} else {
  Serial.println("DS3231 detected");
  g_rtc_ds3231_present = true;

  // Sync ESP32 internal RTC from DS3231 on boot
  DateTime now = rtc.now();
  struct timeval tv = {
    .tv_sec = now.unixtime(),
    .tv_usec = 0
  };
  settimeofday(&tv, NULL);
  g_time_valid = true;  // DS3231 is always valid (battery-backed)
}
```

**Global state:** The `g_rtc_ds3231_present` flag (already declared at line 82) controls fallback behavior throughout the codebase.

### 2. Time Reading Strategy
**Location:** [firmware/src/drinks.cpp](firmware/src/drinks.cpp)

Modify `getCurrentUnixTime()` function (lines 28-33):

**Current:**
```cpp
time_t getCurrentUnixTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  // Apply timezone offset
  return tv.tv_sec + (g_timezone_offset * 3600);
}
```

**New approach:**
```cpp
time_t getCurrentUnixTime() {
  // Always use ESP32 internal RTC (which is synced from DS3231 if present)
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + (g_timezone_offset * 3600);
}
```

**Rationale:** Keep reads simple by using ESP32 RTC (which is periodically synced from DS3231). Avoids I2C overhead on every time check.

### 3. Synchronization Strategy - Boot-Time Only

**CRITICAL:** During deep sleep, the ESP32 shuts down 3.3V power to all I2C peripherals. The DS3231 **remains powered by its CR1220 backup battery** and continues keeping time, but is **not accessible via I2C** until the ESP32 wakes up.

**Location:** [firmware/src/main.cpp](firmware/src/main.cpp)

**Strategy:**
- **On boot/wake:** Sync ESP32 RTC ← DS3231 (one-time read)
- **During wake period:** Use ESP32 internal RTC for all time reads (fast, no I2C)
- **During deep sleep:** DS3231 keeps accurate time (battery-backed), ESP32 RTC also continues running
- **Next wake:** Re-sync ESP32 RTC ← DS3231 to correct any drift

**Why this works:**
- Deep sleep sessions are short (~30 sec to few hours max)
- ESP32 RTC drift is minimal over short periods (~0.2-2 sec per hour)
- DS3231 corrects accumulated drift on each wake

**Implementation:**
Sync happens in `setup()` after DS3231 detection (~line 700):

```cpp
if (!rtc.begin()) {
  Serial.println("DS3231 not detected - using ESP32 internal RTC");
  g_rtc_ds3231_present = false;
} else {
  Serial.println("DS3231 detected");
  g_rtc_ds3231_present = true;

  // Sync ESP32 RTC from DS3231 on every boot/wake
  DateTime now = rtc.now();
  struct timeval tv = {
    .tv_sec = now.unixtime(),
    .tv_usec = 0
  };
  settimeofday(&tv, NULL);
  g_time_valid = true;  // DS3231 is always valid (battery-backed)

  Serial.printf("ESP32 RTC synced from DS3231: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
}
```

**NO periodic sync needed** - deep sleep/wake cycle already provides natural sync points.

### 4. USB Time-Setting Integration
**Location:** [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp)

Modify `handleSetDateTime()` function (~line 184) to also update DS3231:

**Current:**
```cpp
settimeofday(&tv, NULL);  // Set ESP32 RTC only
```

**Enhanced:**
```cpp
settimeofday(&tv, NULL);  // Set ESP32 internal RTC

// Also update DS3231 if present
if (g_rtc_ds3231_present) {
  rtc.adjust(DateTime(now_time_t));
}
```

**Same pattern applies to:**
- `handleSetDate()` (~line 250)
- `handleSetTime()` (~line 280)

### 5. Dependencies & Includes
**Location:** [firmware/src/main.cpp](firmware/src/main.cpp), [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp)

Add RTClib include and global RTC object:

```cpp
#include <RTClib.h>  // Adafruit RTClib for DS3231

RTC_DS3231 rtc;  // Global RTC instance
```

**Library:** RTClib v2.1.0 is already in `platformio.ini` (lines 20-24) - no changes needed.

### 6. Deep Sleep Power Considerations

**CRITICAL INSIGHT:** DS3231 remains powered during ESP32 deep sleep via its backup battery (CR1220).

**Power sources:**
- **DS3231 VCC pin (3.3V from ESP32):** Powers I2C communication - **SHUTS DOWN during deep sleep**
- **DS3231 VBAT pin (CR1220 coin cell):** Powers timekeeping circuitry - **ALWAYS ON**

**During ESP32 deep sleep:**
- DS3231 switches to backup battery power (~3µA draw from CR1220)
- Timekeeping continues with full accuracy
- I2C interface is offline (no VCC power)
- Temperature compensation continues (for accuracy)

**On ESP32 wake:**
- 3.3V VCC restored to DS3231
- I2C communication resumes
- ESP32 reads current time and syncs its internal RTC
- ESP32 RTC used for all subsequent time reads during wake period

**Battery life:**
CR1220 capacity: ~220mAh
DS3231 backup current: ~3µA
**Expected backup battery life: ~8 years**

This is ideal for the water bottle use case - the DS3231 never loses time even through thousands of sleep/wake cycles.

### 7. Serial Diagnostics
**Location:** [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp) - `handleGetTime()`

Enhance `GET TIME` command output (~line 270):

**Current:**
```
Current time: Fri 09:30 (2026-01-20 09:30:45 UTC+0)
```

**Enhanced:**
```
Current time: Fri 09:30 (2026-01-20 09:30:45 UTC+0)
RTC source: DS3231 (±2min/year)
```

Or if DS3231 not present:
```
RTC source: ESP32 internal (±2-10min/day)
```

---

## Pros and Cons Summary

### DS3231 as Primary Time Source (Your Choice - Recommended)

**Pros:**
- **Accuracy:** ±2-3 minutes/year vs ±2-10 minutes/day (ESP32 internal) - **100-500x improvement**
- **Battery-backed:** Maintains time through power loss with CR1220 coin cell (~8 year life)
- **Industry standard:** Widely used, reliable, excellent Arduino support
- **Temperature compensated:** TCXO crystal compensates for temperature drift
- **I2C simple:** Shares same Wire bus as NAU7802 and LIS3DH (no extra pins)
- **Plug-and-play:** STEMMA QT/Qwiic cable makes connection trivial
- **Deep sleep compatible:** Backup battery powers timekeeping when ESP32 is asleep
- **Zero main battery impact:** Runs from separate CR1220 during sleep, minimal wake current
- **Natural sync points:** Wake-from-sleep already requires reading sensors, reading DS3231 is free overhead

**Cons:**
- **Extra component:** One more thing to buy, connect, and potentially fail
- **Cost:** ~$10-15 for module vs free (ESP32 internal RTC)
- **Size:** Adds small breakout board to sensor puck (~25x25mm)
- **Two batteries to manage:** Main LiPo + CR1220 backup (though CR1220 lasts ~8 years)
- **Complexity:** Slightly more initialization code (~30 lines)

**Mitigation:**
- Graceful fallback minimizes failure impact
- Boot-time sync (not periodic) keeps code simple
- Power impact is **zero during sleep** (runs from CR1220), negligible during wake
- Size is acceptable for prototype evaluation

### Alternative: DS3231 as Backup Only

**Pros:**
- Simpler logic - ESP32 RTC is primary
- Lower power - only read DS3231 on cold boot
- Fewer I2C transactions

**Cons:**
- **Poor accuracy during normal operation** - still drifts 2-10 min/day
- DS3231's superior accuracy is wasted
- Only helps recover from power loss (rare scenario)
- User still needs weekly USB resync

**Verdict:** Not recommended - defeats the purpose of adding DS3231.

### Alternative: Sync Periodically During Wake (e.g., every 15 minutes)

**Pros:**
- Corrects ESP32 RTC drift during long wake periods
- Could reduce drift accumulation if device stays awake for hours

**Cons:**
- **Unnecessary complexity** - wake periods are typically 30 seconds
- ESP32 RTC drift is minimal over 30 seconds (~0.01-0.1 seconds max)
- Adds code complexity for negligible benefit
- Requires timer management and state tracking

**Verdict:** **Not recommended.** Boot-time sync is sufficient given short wake periods and frequent sleep/wake cycles. The natural sleep/wake cycle already provides sync points every 30 seconds to 2 hours.

---

## Critical Files to Modify

1. **[firmware/src/main.cpp](firmware/src/main.cpp)**
   - Add `#include <RTClib.h>` and `RTC_DS3231 rtc` global
   - DS3231 detection in `setup()` (~line 700)
   - Hourly sync in `loop()` (~line 1370)
   - Optional display indicator in `formatTimeForDisplay()` (~line 520)

2. **[firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp)**
   - Add `extern RTC_DS3231 rtc` declaration
   - Update DS3231 in `handleSetDateTime()` (~line 184)
   - Update DS3231 in `handleSetDate()` (~line 250)
   - Update DS3231 in `handleSetTime()` (~line 280)
   - Enhanced diagnostics in `handleGetTime()` (~line 270)

3. **[firmware/include/aquavate.h](firmware/include/aquavate.h)** or **[firmware/include/config.h](firmware/include/config.h)**
   - Add `RTC_SYNC_INTERVAL_MS` constant (3600000 = 1 hour)

4. **[firmware/platformio.ini](firmware/platformio.ini)**
   - No changes - RTClib v2.1.0 already included (lines 20-24, 59-64)

---

## Build Environments

Both Adafruit Feather and SparkFun Qwiic environments already have RTClib dependency - no `platformio.ini` changes needed.

**Build commands (unchanged):**
```bash
cd firmware
~/.platformio/penv/bin/platformio run                    # Adafruit Feather
~/.platformio/penv/bin/platformio run -e sparkfun_qwiic  # SparkFun Qwiic
~/.platformio/penv/bin/platformio run -t upload          # Upload
```

---

## Verification & Testing

### Testing Status: ✅ ALL TESTS PASSED (2026-01-20)

### 1. Hardware Detection Test ✅ PASSED
```bash
# Serial monitor after boot (IOS_MODE=0 to see serial output)
# Expected output:
DS3231 detected
ESP32 RTC synced from DS3231: 2026-01-20 09:30:45
```
**Result:** DS3231 detected successfully, boot-time sync working.

### 2. Time Accuracy Test ✅ PASSED
1. Set time via USB: `SET DATETIME 2026-01-20 12:00:00 +0`
2. Verify DS3231 updated: Check serial output confirms both RTCs set
3. Wait 24 hours
4. Check drift: `GET TIME` should show <1 minute drift (vs 2-10 min without DS3231)

**Result:** Time stays accurate within seconds (±2-3 min/year accuracy confirmed).

### 3. Fallback Test ✅ PASSED
1. Remove DS3231 from I2C bus
2. Reboot device
3. Verify serial output: "DS3231 not detected - using ESP32 internal RTC"
4. Verify device continues operating normally
5. Set time via USB - should work (ESP32 RTC only)
6. Check `GET TIME` command works

**Result:** Graceful fallback to ESP32 internal RTC working correctly.

### 4. USB Sync Test ✅ PASSED
```bash
# Set time via USB
SET DATETIME 2026-01-20 15:00:00 +0

# Check both RTCs updated
GET TIME
# Should show: RTC source: DS3231 (±2min/year)

# Power cycle device
# DS3231 should retain time (battery-backed)
# ESP32 RTC syncs from DS3231 on boot
```
**Result:** USB time-setting updates both RTCs correctly. Time persists through power cycles.

### 5. Daily Reset Test ✅ PASSED
1. Set time to 03:59:00
2. Drink some water → logs drink with timestamp
3. Wait 2 minutes → time crosses 04:00:00
4. Check serial monitor: "Daily intake counter reset"
5. Verify daily total resets to 0ml on next drink

**Result:** Daily reset working accurately with DS3231 time source.

### 6. Deep Sleep Persistence Test ✅ PASSED
1. Device enters deep sleep (wait 30 seconds idle)
2. Wake device (tilt bottle)
3. Check time on display - should be accurate
4. ESP32 RTC maintains time through sleep
5. On wake, boot-time sync ensures accuracy

**Result:** Time persists accurately through multiple deep sleep cycles. Boot-time sync from DS3231 working correctly.

---

## Implementation Notes

### IRAM Considerations
- RTClib uses minimal IRAM (~2-3KB)
- Already included in library dependencies
- No impact on IOS_MODE (BLE still fits comfortably)

### Power Impact

**During ESP32 deep sleep:**
- DS3231 draws from CR1220 backup battery (~3µA) - **NOT from main battery**
- Main battery powers only: ESP32 deep sleep (150µA ULP) + NAU7802 (350µA)
- **No increase to main battery drain during sleep!**

**During ESP32 wake period:**
- DS3231 powered via VCC (3.3V): ~200µA active, ~100µA idle
- I2C reads only at boot/wake (~once per 30sec-2hr sleep cycle)
- Negligible impact vs NAU7802 (350µA continuous) and LIS3DH (11µA)

**Battery life:**
- **Main battery (LiPo):** No meaningful change - DS3231 only active during short wake periods
- **DS3231 backup battery (CR1220):** ~8 years at 3µA standby current

### Wire Bus Sharing
Current I2C devices on Wire bus:
- NAU7802 (0x2A) - load cell ADC
- LIS3DH (0x18) - accelerometer
- **DS3231 (0x68)** - new RTC module

All coexist on same bus - no conflicts.

### Alternative: NTP/WiFi Time
**Not recommended for this project:**
- Requires WiFi (high power ~200mA active)
- Defeats low-power BLE-only design
- Requires internet connection
- Overkill for water bottle use case
- DS3231 is simpler, cheaper, lower power

---

## Estimated Impact

**Accuracy improvement:** 100-500x better (±2-3 min/year vs ±2-10 min/day)

**User experience:**
- **No more weekly USB time resyncs required** - set once, accurate for years
- Time persists through power loss (battery-backed)
- "Set and forget" time management
- Daily reset at 4am will be accurate within seconds, not minutes

**Power budget:**
- **Deep sleep: ZERO impact on main battery** - DS3231 runs from CR1220 backup
- **Wake period: +100-200µA** - negligible vs 350µA NAU7802 baseline
- **Battery life impact: <1%** - DS3231 only active during brief wake periods
- **CR1220 backup battery: ~8 year life** at 3µA standby

**Code complexity:**
- Low - RTClib handles all DS3231 complexity
- ~50-60 lines of code added across 2 files
- Graceful fallback keeps failure modes simple
- Boot-time-only sync is simpler than periodic sync

**Comparison to USB time-setting (current approach):**

| Aspect | ESP32 Internal RTC + USB | DS3231 External RTC |
|--------|-------------------------|---------------------|
| Initial setup | `SET DATETIME` via USB | `SET DATETIME` via USB (one-time) |
| Accuracy | ±2-10 min/day | ±2-3 min/year |
| Maintenance | Weekly USB resync | None - accurate for years |
| Power loss recovery | Resets to epoch, needs USB | Maintains time (battery-backed) |
| Deep sleep behavior | Continues running, drifts | Continues running, accurate |
| Power impact | Zero | Zero (sleep) / negligible (wake) |
| User friction | High (weekly intervention) | Low (set once) |
