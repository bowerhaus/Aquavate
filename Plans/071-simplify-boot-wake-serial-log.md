# Plan: Simplify Boot/Wake Serial Log Output (Issue #108)

## Context

Every wake cycle produces ~90 lines of serial output, most of which is initialization detail only useful when debugging specific subsystems. Health-check wakes (every 2 hours) are particularly noisy — the device boots, confirms it's alive, and auto-sleeps after 30s. The goal is to cut output to ~20 lines while keeping all detail accessible at debug level 2+ via serial commands.

## Files to Modify

- `firmware/src/main.cpp` — changes 1, 3, 4, 5, 6
- `firmware/src/storage.cpp` — change 2
- `firmware/src/config.h` — change 7

---

## Change 1: Wrap ADXL343 config steps in DEBUG_PRINTF (~15 lines saved)

**`configureADXL343Interrupt()` (main.cpp:229-312)**
- Wrap per-step Serial prints (lines 250, 256, 261, 266, 272, 278, 283, 287, 291, 299, 303, 307) in `DEBUG_PRINTF(g_debug_accelerometer, ...)`
- Replace the header/footer block with a single unconditional summary:
  ```cpp
  Serial.printf("ADXL343: Interrupts configured (activity >%.1fg, tap >%.1fg)\n",
                ACTIVITY_WAKE_THRESHOLD * 0.0625f, TAP_WAKE_THRESHOLD * 0.0625f);
  ```

**`configureADXL343TapWake()` (main.cpp:315-379)**
- Same approach: wrap per-step prints (lines 335, 340, 345, 350, 355, 359, 363, 367, 371, 375) in `DEBUG_PRINTF(g_debug_accelerometer, ...)`
- Replace header/footer with a single unconditional summary:
  ```cpp
  Serial.printf("ADXL343: Tap wake configured (threshold >%.1fg, double-tap)\n",
                TAP_WAKE_THRESHOLD * 0.0625f);
  ```

**Post-wake accelerometer diagnostics (main.cpp:784-802):**
- Wrap the orientation dump and INT_SOURCE read in `DEBUG_PRINTF(g_debug_accelerometer, ...)`

## Change 2: Wrap Storage "Loaded/Saved" messages in DEBUG_PRINTF (~15 lines saved)

**storage.cpp** — All `"Storage: Loaded X = Y"` and `"Storage: Saved X = Y"` messages become `DEBUG_PRINTF(g_debug_calibration, ...)`. Specifically:
- `storageLoadCalibration()` lines 91, 100-107
- `storageSaveCalibration()` lines 63, 72-79
- `storageSaveTimezone()` / `storageLoadTimezone()` — lines 160-161, 172-173
- `storageSaveTimeValid()` / `storageLoadTimeValid()` — lines 184-185, 196-197
- `storageSaveLastBootTime()` / `storageLoadLastBootTime()` — lines 208-209, 220-221
- `storageSaveDisplayMode()` / `storageLoadDisplayMode()` — lines 232-233, 244-245
- `storageSaveSleepTimeout()` / `storageLoadSleepTimeout()` — lines 256-258, 280-282
- `storageSaveExtendedSleepTimer()` / `storageLoadExtendedSleepTimer()` — lines 293-295, 306-308
- `storageSaveExtendedSleepThreshold()` / `storageLoadExtendedSleepThreshold()` — lines 319-321, 332-334
- `storageSaveShakeToEmptyEnabled()` / `storageLoadShakeToEmptyEnabled()` — lines 345-346, 357-358
- `storageSaveDailyGoal()` / `storageLoadDailyGoal()` — lines 376-378, 397-399
- `storageResetCalibration()` — lines 129, 140

**Keep unconditional:** All error/warning messages (`"Storage: Not initialized"`, `"Storage: WARNING - ..."`, `"Storage: Failed to initialize NVS"`).

**Note:** storage.cpp needs `#include "config.h"` if not already present to access the DEBUG_PRINTF macros.

## Change 3: Remove duplicate calibration display in main.cpp (~5 lines saved)

**main.cpp:856-867** — After `storageLoadCalibration()`, main.cpp prints the same calibration values that storage.cpp already prints. Replace the 8-line block with a single summary:
```cpp
if (g_calibrated) {
    Serial.printf("Calibration: valid (scale=%.2f)\n", g_calibration.scale_factor);
} else {
    Serial.println("Calibration: not found - calibration required");
}
```

## Change 4: Wrap drink buffer metadata dump in DEBUG_PRINTF (~6 lines saved)

**main.cpp:1047-1052** — Wrap the entire `DRINK BUFFER STATUS` block in `DEBUG_PRINTF(g_debug_drink_tracking, ...)`:
```cpp
DEBUG_PRINTF(g_debug_drink_tracking, "\n=== DRINK BUFFER STATUS (LittleFS) ===\n");
DEBUG_PRINTF(g_debug_drink_tracking, "Record count: %d / %d (max)\n", meta.record_count, DRINK_MAX_RECORDS);
// ... etc
```

## Change 5: Consolidate time configuration to one line (~5 lines saved)

**main.cpp:905-942** — Replace the multi-line time configuration block with a single unconditional summary line:
```cpp
char time_str[64];
formatTimeForDisplay(time_str, sizeof(time_str));
Serial.printf("Time: %s (UTC%+d, %s)\n", time_str, g_timezone_offset,
              g_rtc_ds3231_present ? "DS3231" : "ESP32");
```
Keep the `"WARNING: Time not set!"` unconditional. Keep the cold-boot NVS restore logic (functional code) but wrap its Serial output in `DEBUG_PRINTF(g_debug_calibration, ...)`.

## Change 6: Consolidate sensor init two-liners (~4 lines saved)

Replace "Initializing X..." + "X found!" pairs with single-line summaries:

**main.cpp — Boot header (lines 711-727):**
```cpp
Serial.printf("Aquavate v%d.%d.%d | ", AQUAVATE_VERSION_MAJOR, AQUAVATE_VERSION_MINOR, AQUAVATE_VERSION_PATCH);
// board name on same line
```

**NAU7802 (lines 763-772):**
```cpp
Serial.println("NAU7802: OK");     // on success
Serial.println("NAU7802: FAILED"); // on failure
```

**ADXL343 (lines 775-816):**
```cpp
Serial.println("ADXL343: OK");     // on success (interrupt details from change 1)
Serial.println("ADXL343: FAILED"); // on failure
```

**DS3231 (lines 819-839):**
```cpp
Serial.printf("DS3231: OK (synced %04d-%02d-%02d %02d:%02d:%02d)\n", ...); // on success
Serial.println("DS3231: not detected (using ESP32 RTC)");                   // on failure
```

**E-Paper display (lines 731, 756):** Consolidate to `Serial.println("E-Paper: OK");`

**Battery (lines 750-754):** Consolidate to `Serial.printf("Battery: %.2fV (%d%%)\n", batteryV, batteryPct);`

**Remove:** `Serial.println("Initializing...");` (line 759) — redundant given per-sensor output.

**Other init messages** (lines 846, 957, 963, 1013, 1018, 1042, 1131, 1139, 1143, 1145): Consolidate or wrap verbose ones. Keep short confirmations.

## Change 7: Set DEBUG_BLE default to 0 in config.h (~12 lines saved)

**config.h:64** — Change:
```cpp
#define DEBUG_BLE                       0   // 0 = disable BLE debug, 1 = enable BLE debug
```
This silences all `[BLE]` init messages since they already use the conditional `BLE_DEBUG` / `BLE_DEBUG_F` macros. BLE debug remains opt-in via `d9` serial command.

---

## Expected Result

A typical health-check wake will produce ~20 lines:
```
=================================
Woke up from timer (health check)
=================================
Aquavate v0.1.0 | Adafruit ESP32 Feather V2
Battery: 4.19V (99%)
E-Paper: OK
NAU7802: OK
ADXL343: OK (interrupts configured)
DS3231: OK (synced 2026-02-08 13:02:49)
Calibration: valid (scale=435.96)
Time: Sun 1pm (UTC+0, DS3231)
Daily: 225ml (1 drinks), baseline=288705
BLE initialized (not advertising - health check)
Unsynced records: 0
Setup complete! Activity timeout: 30s
```

All detail remains accessible at debug level 2+ via `d2`-`d9` serial commands.

## Verification

1. Build firmware: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. User uploads to device and monitors serial output on:
   - Power-on/reset
   - Motion wake (pick up bottle)
   - Health-check timer wake
3. Verify ~20 lines of clean output in each case
4. Run `d9` serial command, trigger another wake, verify full verbose output still appears
5. Run `d0` serial command, verify quiet mode still works
