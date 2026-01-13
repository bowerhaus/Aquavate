# Plan: RTC Time Setting for Standalone Bottle Operation

## Current State Analysis

**RTC Integration Status:**
- DS3231 RTC hardware planned but NOT yet integrated
- RTClib v2.1.0 library already declared in platformio.ini
- I2C address (0x68) defined in [aquavate.h:28](firmware/include/aquavate.h#L28)
- No functional RTC code exists yet
- Current time tracking uses `millis()` only (relative timing)

**Requirement:**
Enable time setting WITHOUT iOS app for standalone bottle operation. This is a fallback when the app is not available to track daily water intake totals.

## Alternative Approaches

### Option A: WiFi NTP Sync (Recommended for User Convenience)

**How it works:**
1. User configures WiFi credentials via Serial/USB connection during initial setup
2. On first boot (or when RTC is unset), ESP32 connects to WiFi
3. Fetch time from NTP server (pool.ntp.org)
4. Set DS3231 RTC with accurate time
5. Disconnect WiFi (battery preservation)
6. Store WiFi credentials in NVS for future re-sync if needed

**Pros:**
- ✅ Automatic and accurate (to the second)
- ✅ No manual date entry required
- ✅ Timezone support (can configure offset)
- ✅ Can periodically re-sync if RTC drifts
- ✅ ESP32 has built-in WiFi hardware (no cost increase)
- ✅ Simple user experience once configured

**Cons:**
- ❌ Requires WiFi network access
- ❌ Adds WiFi library dependencies (~70-100KB flash)
- ❌ Slightly higher power consumption during sync (mitigated by brief connection)
- ❌ Requires one-time credential configuration step
- ❌ Won't work in locations without WiFi

**Configuration Options:**
- Serial command interface (USB cable): `SET_WIFI "SSID" "password" timezone_offset`
- Alternative: Web-based captive portal (ESP32 creates AP, user connects and enters credentials)

---

### Option B: Serial/USB Time Setting

**How it works:**
1. Connect bottle to computer via USB cable
2. Send time setting command via serial monitor
3. Parse command and set RTC
4. Disconnect cable

**Command format:**
```
SET_TIME 2026-01-12 14:30:00 GMT
```

**Pros:**
- ✅ No WiFi dependency
- ✅ Minimal code/flash impact (~2KB)
- ✅ Works anywhere with USB cable
- ✅ Simple implementation
- ✅ No network security concerns

**Cons:**
- ❌ Requires USB cable connection
- ❌ Manual date/time entry (error-prone)
- ❌ Need computer access each time
- ❌ Poor user experience (typing dates manually)
- ❌ No automatic timezone handling

**Implementation complexity:** Low

---

### Option C: BLE Time Sync (Without Full App)

**How it works:**
1. Implement minimal BLE time service on ESP32
2. User connects with generic BLE app (e.g., nRF Connect, LightBlue)
3. Write Unix timestamp to BLE characteristic
4. ESP32 sets RTC from received timestamp

**Pros:**
- ✅ No WiFi needed
- ✅ Works with any BLE-capable phone
- ✅ Consistent with overall BLE architecture
- ✅ Can be phone's current time (one-tap sync)
- ✅ Could be integrated into future iOS app

**Cons:**
- ❌ Still requires smartphone (defeats "standalone" somewhat)
- ❌ User needs third-party BLE app
- ❌ More complex than serial (BLE service implementation)
- ❌ Timezone handling on phone side

**Implementation complexity:** Medium

---

### Option D: Button-Based Manual Entry (Hardware Addition Required)

**How it works:**
1. Add 2-3 extra buttons to hardware
2. Display date/time entry UI on e-paper
3. User increments/decrements values
4. Confirm and set RTC

**Pros:**
- ✅ Completely standalone
- ✅ No external dependencies

**Cons:**
- ❌ **Requires hardware changes** (extra buttons not in current design)
- ❌ Very tedious user experience
- ❌ Complex UI state machine
- ❌ Defeats "minimal hardware" design goal
- ❌ Poor e-paper refresh UX (slow)

**Recommendation:** Not viable without hardware redesign

---

## Recommendation Matrix

| Use Case | Best Option |
|----------|-------------|
| **Home use with WiFi** | Option A (WiFi NTP) |
| **Travel/no WiFi** | Option B (Serial) as fallback |
| **Future iOS app integration** | Option C (BLE) becomes primary, Option A as fallback |
| **No computer/phone access** | Option D (not practical with current hardware) |

---

## User Requirements (Confirmed ✓)

**UPDATED:** User wants USB/Serial time setting, NOT WiFi.

Based on your responses:
1. **Primary method:** Serial/USB time setting (no WiFi dependency)
2. **Configuration:** Serial commands via USB cable
3. **Time sync:** Manual via USB when needed (no automatic weekly sync)
4. **Timezone:** Fixed UTC offset stored in NVS (e.g., GMT+0, EST-5)
5. **Hardware:** Use ESP32 internal RTC (no external DS3231 chip available yet)

---

## Implementation Plan

### Architecture Overview

**Key Components:**
- **ESP32 Internal RTC** - Built-in timekeeping (no external chip needed!)
- **Serial Commands** (`serial_commands.cpp`/`serial_commands.h`) - USB interface for time setting
- **Storage Extension** (`storage.cpp`/`storage.h`) - Timezone offset persistence in NVS
- **Main Integration** (`main.cpp`) - Time display and command handling

**Design Principles:**
- **Use ESP32's internal RTC** - Built-in `time.h` and `gettimeofday()`/`settimeofday()`
- **Serial/USB time setting** - Manual time sync via USB cable (no WiFi, no network dependency)
- **Simple and reliable** - No network timeouts, no WiFi power consumption
- **User controls sync frequency** - Set time as often as needed via USB
- Use existing storage patterns (Preferences library already in use)

**Important: ESP32 Internal RTC Accuracy**

The ESP32 has a built-in RTC that survives deep sleep, but with accuracy limitations:
- **Clock source:** Internal 150kHz RC oscillator (default)
- **Accuracy:** ±5% tolerance, ~1000ppm/°C temperature coefficient
- **Practical drift:** ~2-10 minutes per day (varies with temperature)
- **Deep sleep compatible:** Yes, RTC runs during deep sleep ✓

**User responsibility:** With USB time setting, the user manually syncs time as needed. For daily water tracking, syncing once per week via USB is recommended to compensate for drift.

**Advantages of USB approach:**
- No WiFi complexity or power consumption
- Works anywhere (no network required)
- Simple, deterministic (no network timeouts)
- User has full control over when time is set

Sources:
- [ESP32's Built-in RTC](https://www.pleasedontcode.com/blog/esp32s-built-in-rtc-real-time-clock)
- [ESP32 Deep Sleep Time Accuracy](https://esp32.com/viewtopic.php?t=9109)
- [ESP-IDF System Time Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/system_time.html)

---

### Critical Files to Modify/Create

#### New Files (1 module, 2 files total):

1. **[firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp)** (create)
   - Command parser for `SET_TIME 2026-01-12 14:30:00 -5`
   - Additional commands: `GET_TIME`, `SET_TIMEZONE -5`
   - Date/time parsing (YYYY-MM-DD HH:MM:SS format)
   - Timezone validation (-12 to +14)
   - Sets ESP32 internal RTC via `settimeofday()`
   - User feedback with clear error messages
   - ~150 lines

2. **[firmware/include/serial_commands.h](firmware/include/serial_commands.h)** (create)
   - `serialCommandsInit()`, `serialCommandsUpdate()` (call in loop)
   - Callback typedef: `OnTimeSetCallback`
   - Command buffer management (128 bytes)
   - ~40 lines

#### Modified Files (3 existing files):

3. **[firmware/include/storage.h](firmware/include/storage.h)** (modify)
   - Add new functions:
     - `bool storageSaveTimezone(int8_t utc_offset);`
     - `int8_t storageLoadTimezone();`
     - `bool storageSaveTimeValid(bool valid);`
     - `bool storageLoadTimeValid();`
   - +6 lines

4. **[firmware/src/storage.cpp](firmware/src/storage.cpp)** (modify)
   - Implement timezone and time_valid flag save/load using existing `g_preferences` object
   - New NVS keys: `timezone_offset`, `time_valid`
   - Follow existing pattern (see lines 48-72 for CalibrationData example)
   - Uses `putChar()`, `putBool()`, `getChar()`, `getBool()`
   - +30 lines

5. **[firmware/src/main.cpp](firmware/src/main.cpp)** (modify)
    - Add global state: `int8_t g_timezone_offset`, `bool g_time_valid`
    - In `setup()`:
      - Load timezone offset from NVS (default: 0 UTC)
      - Load time_valid flag from NVS
      - Initialize serial command handler
      - Print current time if valid (or warning if not set)
    - In `loop()`:
      - Call `serialCommandsUpdate()` every iteration
    - Add helper functions:
      - `onTimeSet()` - Callback when SET_TIME command received
      - `formatTimeForDisplay()` - Format as "Wed 2pm" (day + 12-hour format)
      - `getDayName()` - Convert weekday number to 3-letter string
      - `updateMainScreen()` - Modify existing function to include time display
    - +80 lines

---

### Time Setting Flow

```
1. BOOT / WAKE
   ├─> Load timezone offset from NVS (default: 0 UTC)
   ├─> Load time_valid flag from NVS (has time ever been set?)
   ├─> Get current time from ESP32 internal RTC (gettimeofday())
   └─> IF time_valid == false
       └─> Print warning: "Time not set - use SET_TIME command"

2. USER SETS TIME VIA USB (one-time or as needed)
   ├─> User connects USB cable
   ├─> User opens Serial Monitor (115200 baud)
   ├─> User sends command: SET_TIME 2026-01-12 14:30:00 -5
   ├─> Parse date/time (YYYY-MM-DD HH:MM:SS)
   ├─> Parse timezone offset (optional, -12 to +14)
   ├─> Create struct timeval from parsed values
   ├─> Call settimeofday() to set ESP32 internal RTC
   ├─> Save timezone offset to NVS
   ├─> Set time_valid flag to true in NVS
   └─> Print confirmation: "Time set: 2026-01-12 14:30:00 EST"

3. DURING WAKE PERIOD
   ├─> Check serial for commands every loop iteration
   │   ├─> SET_TIME → Parse and set RTC (see step 2)
   │   ├─> GET_TIME → Print current RTC time with timezone
   │   └─> SET_TIMEZONE → Update timezone offset only
   └─> Display current time on e-paper (small, bottom right)

4. BEFORE DEEP SLEEP
   └─> No action needed (ESP32 internal RTC continues in deep sleep)
   └─> NOTE: Time will drift ~2-10 minutes/day - user re-syncs as needed
```

---

### Serial Command Interface

**Commands:**
```bash
# Set time and timezone (from computer's current time)
> SET_TIME 2026-01-12 14:30:00 -5
Time set: 2026-01-12 14:30:00 EST (-5)
Timezone saved to NVS

# Set time with UTC (no timezone offset)
> SET_TIME 2026-01-12 19:30:00 0
Time set: 2026-01-12 19:30:00 UTC

# Set time using current timezone (keeps existing offset)
> SET_TIME 2026-01-12 14:30:00
Time set: 2026-01-12 14:30:00 EST (-5)
Using saved timezone: -5

# Check current time
> GET_TIME
Current time: 2026-01-12 14:35:45 EST (-5)
Time valid: Yes
RTC drift: ~3-5 minutes/day (resync recommended weekly)

# Change timezone without changing time
> SET_TIMEZONE -8
Timezone set: -8 hours (PST)
Saved to NVS

# First boot (time not set)
> GET_TIME
WARNING: Time not set!
Current RTC: 1970-01-01 00:00:15 (epoch)
Use SET_TIME command to set time
```

**Time Format:**
- Date: YYYY-MM-DD (e.g., 2026-01-12)
- Time: HH:MM:SS (24-hour format, e.g., 14:30:00)
- Timezone: -12 to +14 (hours from UTC)

**Validation:**
- Year: 2026-2099 (reasonable range)
- Month: 1-12
- Day: 1-31 (validated for month)
- Hour: 0-23
- Minute: 0-59
- Second: 0-59
- Timezone offset: -12 to +14

---

### Display Feedback

**Main Screen Enhancement (top of screen, centered left of battery icon):**
```
┌────────────────────────────┐
│  Wed 2pm        [Batt 85%] │  ← Time display (day + hour)
│                            │
│  [Bottle graphic]  830ml   │
│                            │
│                            │
└────────────────────────────┘
```

**If time not set:**
```
┌────────────────────────────┐
│  --- --         [Batt 85%] │  ← Placeholder when time invalid
│                            │
│  [Bottle graphic]  830ml   │
│                            │
│                            │
└────────────────────────────┘
```

**Format Specification:**
- **Day:** 3-letter abbreviation (Mon, Tue, Wed, Thu, Fri, Sat, Sun)
- **Hour:** 12-hour format with am/pm (e.g., 2pm, 9am, 12pm)
- **Spacing:** One space between day and hour (e.g., "Wed 2pm")
- **Placeholder:** "--- --" when time_valid is false
- **Position:** Top left, aligned to leave space for battery icon on right

---

### Power Budget Analysis

**USB Time Setting Power Impact:**
- **Zero ongoing power consumption** - no WiFi, no network communication
- Time setting only occurs when USB cable connected (device is being charged)
- No battery impact from time sync
- Serial command processing: <1mA when active, negligible impact

**Battery Life:**
- Unchanged from baseline (no WiFi sync overhead)
- Deep sleep: ~200µA baseline
- Expected runtime: 1-2 weeks (user requirement) ✓

---

### Library Dependencies

**No new libraries required!** All needed libraries are built into ESP32 Arduino:
- ✓ Arduino time.h (built-in) - Internal RTC interface
- ✓ sys/time.h (built-in) - `gettimeofday()`, `settimeofday()` for RTC access

**Note:** No WiFi, no NTP, no network libraries needed. RTClib in platformio.ini is for future DS3231 support - not needed for this implementation.

**Code Size Estimate:**
- Current firmware: 413KB (31.8% of 1.3MB flash)
- New code: ~3KB (Serial commands + Storage, very minimal)
- **Total: ~416KB (32.0%)** - well within limits ✓

**RAM Estimate:**
- Current: ~22KB (6.8% of 327KB)
- New: ~150 bytes (command buffer + timezone + flags)
- **Total: ~22.2KB (6.8%)** - negligible increase ✓

---

### Testing Strategy

**Phase 1: Internal RTC Validation**
1. Connect USB, open Serial Monitor (115200 baud)
2. Check ESP32 internal RTC with `gettimeofday()`
3. Verify time_valid flag (should be false on first boot)
4. Read time (should show epoch 1970-01-01 initially)

**Phase 2: Set Time via Serial**
```
> SET_TIME 2026-01-13 09:30:00 -5
```
Expected: "Time set: 2026-01-13 09:30:00 EST (-5)"

**Phase 3: Verify Time Persistence**
```
> GET_TIME
```
Expected: Current time with timezone displayed
Verify: time_valid flag saved to NVS

**Phase 4: RTC Persistence (Deep Sleep)**
1. Set time successfully
2. Enter deep sleep for 5 minutes (or simulate with delay)
3. Wake up / reconnect USB
4. Check `GET_TIME` - should show correct time (+5 minutes ±drift)
5. Note: Expect some drift with internal RTC

**Phase 5: Timezone Changes**
```
> SET_TIMEZONE -8
```
Expected: "Timezone set: -8 hours (PST)"
Verify: GET_TIME shows updated timezone, NVS saves value

**Phase 6: Error Handling**
- Malformed date: `SET_TIME 2026-13-01 12:00:00` → "Invalid month"
- Invalid time: `SET_TIME 2026-01-12 25:00:00` → "Invalid hour"
- Bad timezone: `SET_TIME 2026-01-12 12:00:00 +20` → "Timezone must be -12 to +14"

**Phase 7: Display Integration**
- Set time via USB (e.g., `SET_TIME 2026-01-15 14:30:00 -5` for Wednesday 2:30pm)
- Verify time appears on e-paper display at top left
- Format should be "Wed 2pm" (day + 12-hour format)
- Verify "--- --" shown when time_valid is false
- Test various times:
  - Morning: 9am, 10am, 11am
  - Noon/midnight: 12pm, 12am
  - Afternoon: 1pm, 2pm, 11pm
  - Different days: Mon, Tue, Wed, Thu, Fri, Sat, Sun

**Phase 8: Drift Measurement (Optional)**
- Set time, note exact time
- Wait 24 hours (keep powered, allow deep sleep cycles)
- Check time again, measure drift
- Expected: 2-10 minutes drift per day with internal RTC
- Document drift for user (recommend weekly USB re-sync)

---

### Edge Cases Handled

| Scenario | Behavior |
|----------|----------|
| **Power loss (battery dead)** | Internal RTC resets to epoch, time_valid flag false, display `--:--` until time set |
| **Invalid date** | Reject command, print "Invalid date: <reason>" (e.g., month 13, day 32) |
| **Invalid time** | Reject command, print "Invalid time: <reason>" (e.g., hour 25, minute 60) |
| **Invalid timezone offset** | Reject command, print "Timezone must be -12 to +14" |
| **Malformed serial command** | Print usage example: "SET_TIME YYYY-MM-DD HH:MM:SS [timezone]" |
| **Time never set** | Display `--:--` on e-paper, print warning on serial |
| **Deep sleep** | Time continues running (with drift), no special handling needed |
| **USB disconnected during command** | Partial command ignored (buffer cleared on next iteration) |

---

### Implementation Sequence

**Day 1: Serial Commands & Storage**
1. Create [serial_commands.h](firmware/include/serial_commands.h) with command interface
2. Create [serial_commands.cpp](firmware/src/serial_commands.cpp) with parser
   - Implement `parseDateTime()` - parse YYYY-MM-DD HH:MM:SS
   - Implement `parseTimezone()` - parse integer offset
   - Implement `validateDateTime()` - validate ranges
   - Use `settimeofday()` to set ESP32 RTC
3. Add timezone/time_valid functions to [storage.h](firmware/include/storage.h)
4. Implement save/load in [storage.cpp](firmware/src/storage.cpp) using existing Preferences pattern
5. Test basic SET_TIME, GET_TIME commands via Serial Monitor

**Day 2: Main Integration & Display**
1. Modify [main.cpp](firmware/src/main.cpp):
   - Add global state: `g_timezone_offset`, `g_time_valid`
   - Load timezone and time_valid flag from NVS in setup()
   - Initialize serial command handler
   - Print time status on boot
2. Implement callback function `onTimeSet()` when SET_TIME received
3. Add helper function `formatTime()` for display
4. Test end-to-end: boot → SET_TIME → GET_TIME → persist

**Day 3: Display & Time Formatting**
1. Implement `formatTimeForDisplay()` - returns "Wed 2pm" format string
   - Use `strftime()` or manual formatting for day name
   - Convert 24-hour to 12-hour format (14:00 → 2pm)
   - Handle am/pm suffix (lowercase)
2. Implement `getDayName()` - convert weekday 0-6 to Mon/Tue/Wed/etc
3. Modify main screen drawing code:
   - Add time display at top left (before battery icon)
   - Use appropriate font size (readable but not dominant)
   - Show "--- --" when time_valid is false
4. Test display update when time is set
5. Verify time persists through deep sleep cycles and updates on screen

**Day 4: Testing & Refinement**
1. Edge case testing (invalid dates, times, timezones)
2. Error message validation (clear, helpful)
3. Timezone changes (SET_TIMEZONE command)
4. Power cycle testing (NVS persistence)
5. Deep sleep testing (RTC persistence)
6. **Optional:** Drift measurement (24 hour test)

**Total Estimated Time: 3-4 days** (simpler than WiFi approach!)

---

### Future Enhancements (Post-MVP)

1. **WiFi NTP Sync** (2-3 days)
   - Add WiFi NTP as alternative to USB (for users who want automatic sync)
   - Requires WiFi credential configuration
   - Higher power consumption but more convenient

2. **BLE Time Sync from iOS App** (1 day firmware + 1 day iOS)
   - iOS app sends current time via BLE characteristic
   - More convenient than USB cable
   - Requires iOS app development

3. **Computer Time Sync Script** (2 hours)
   - Python/bash script to automatically set bottle time from computer
   - Reads computer's current time, sends SET_TIME command via serial
   - One-click sync for users

4. **External DS3231 RTC Chip** (hardware + 1 day firmware)
   - Add DS3231 RTC module (I2C, already defined at 0x68 in aquavate.h)
   - Swap internal RTC backend for DS3231 (RTClib)
   - Improves accuracy from ±10 min/day to ±5 sec/month
   - Requires hardware purchase and assembly

5. **Full Date Display** (1 hour)
   - Show full date (e.g., "Wed 1/15") instead of just "Wed 2pm"
   - More context for water tracking

6. **Time Format Options** (1 hour)
   - Option to show minutes (e.g., "Wed 2:30pm" instead of "Wed 2pm")
   - Command: `SET_TIME_FORMAT full` or `SET_TIME_FORMAT hour_only`

---

### Verification Checklist

After implementation, verify:
- [ ] ESP32 internal RTC accessible via `gettimeofday()` and `settimeofday()`
- [ ] SET_TIME command parses date/time correctly (YYYY-MM-DD HH:MM:SS format)
- [ ] Timezone offset correctly applied (test with +5, -5, +0)
- [ ] Time set via USB persists across power cycles
- [ ] Timezone persists in NVS across power cycles
- [ ] time_valid flag persists in NVS
- [ ] GET_TIME displays current time with timezone
- [ ] SET_TIMEZONE updates offset without changing time
- [ ] Serial commands work correctly (SET_TIME, GET_TIME, SET_TIMEZONE)
- [ ] Error messages clear and helpful (malformed commands, invalid dates/times)
- [ ] Date validation works (reject month 13, day 32, etc.)
- [ ] Time validation works (reject hour 25, minute 60, etc.)
- [ ] Internal RTC time persists through deep sleep cycles
- [ ] **Drift measured (optional):** <10 minutes per day with internal RTC
- [ ] time_valid flag prevents displaying time when not set
- [ ] E-paper shows "--- --" when time not set (top left)
- [ ] E-paper shows "Wed 2pm" format when time valid (top left, before battery)
- [ ] Day name correctly displayed (Mon, Tue, Wed, Thu, Fri, Sat, Sun)
- [ ] 12-hour format works (1-12, with am/pm suffix)
- [ ] Noon shows as "12pm", midnight as "12am"
- [ ] Time display does not overlap with battery icon
- [ ] Code compiles for both board targets without warnings
- [ ] Flash usage <35% and RAM usage <10%

---

## Summary

This implementation provides:
- ✓ **Manual time setting** via USB Serial commands (simple, deterministic)
- ✓ **Zero power overhead** (no WiFi, no network communication)
- ✓ **Works anywhere** (no WiFi network required)
- ✓ **User-controlled sync** (set time as often as needed via USB)
- ✓ **Simple implementation** (~3KB code, 2 files + modifications)
- ✓ **Robust error handling** (validates dates, times, timezones)
- ✓ **User-friendly feedback** on serial and e-paper
- ✓ **Modular architecture** following existing codebase patterns
- ✓ **No new library dependencies** (uses built-in ESP32 time.h and sys/time.h)
- ✓ **No external hardware required** (ESP32 internal RTC, no DS3231 chip needed)
- ✓ **Minimal testing complexity** (no network timeouts or WiFi issues)

**Key Trade-offs:**
- ESP32 internal RTC drifts 2-10 minutes/day (vs DS3231's ±5 sec/month)
- User must manually sync time via USB (recommended weekly)
- Requires USB cable connection to set time
- Sufficient accuracy for daily water intake tracking
- Future upgrade paths:
  - Add WiFi NTP for automatic sync (if user wants convenience)
  - Add BLE time sync from iOS app
  - Add DS3231 chip for better accuracy (hardware change)

The design seamlessly integrates into the existing Aquavate firmware, respects battery-powered constraints, and provides a solid foundation for standalone bottle operation without iOS app dependency.

Sources:
- [ESP32's Built-in RTC](https://www.pleasedontcode.com/blog/esp32s-built-in-rtc-real-time-clock)
- [ESP32 Deep Sleep Time Accuracy Forum](https://esp32.com/viewtopic.php?t=9109)
- [ESP-IDF System Time Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/system_time.html)

