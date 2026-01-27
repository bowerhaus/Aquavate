# Plan: Fix NVS Write Failures Causing Lost Drinks

## Issue Summary

GitHub Issue #76 (reopened): Drinks are being lost due to NVS write failures. The drink detection works correctly, but when NVS writes fail, the `recalculateDailyTotals()` function overwrites the in-memory totals with stale NVS data.

**Note:** The original baseline hysteresis fix from plan 057 was already implemented. This is a **different issue**.

## Root Cause Analysis

From the user's logs:
```
=== DRINK DETECTED: 161.1ml (POUR) ===
ERROR: Failed to write drink record to NVS
ERROR: Failed to write daily state to NVS
Drinks: Recalculated total = 328ml (2 drinks)   <-- Should be 489ml!
```

The problem is in [firmware/src/drinks.cpp:256-263](firmware/src/drinks.cpp#L256-L263):

```cpp
storageSaveDrinkRecord(record);           // Return value IGNORED

g_daily_state.last_recorded_adc = current_adc;
storageSaveDailyState(g_daily_state);     // Return value IGNORED

recalculateDailyTotals();                 // Overwrites in-memory with stale NVS!
```

**Why NVS writes might fail:**
- Flash write errors (transient or wear-related)
- NVS metadata corruption from interrupted previous writes
- **Handle contention**: `storage.cpp` keeps `g_preferences` open persistently (line 35) while `storage_drinks.cpp` opens/closes 12+ separate handles per session - all to the same "aquavate" namespace

**Potential future fix for root cause:**
- Unify NVS access through a single persistent handle, or
- Close `g_preferences` before drink storage operations, or
- Use separate namespaces for calibration vs drink data

## Proposed Fix

### 1. Handle NVS Failures Gracefully

Modify the drink detection code to:
- Check return values from storage functions
- If NVS save fails, update in-memory totals directly instead of calling `recalculateDailyTotals()`
- Add retry logic for transient failures

### Changes to [firmware/src/drinks.cpp](firmware/src/drinks.cpp)

**Before (lines 256-263):**
```cpp
storageSaveDrinkRecord(record);

g_daily_state.last_recorded_adc = current_adc;
storageSaveDailyState(g_daily_state);

recalculateDailyTotals();
```

**After:**
```cpp
bool record_saved = storageSaveDrinkRecord(record);

g_daily_state.last_recorded_adc = current_adc;
bool state_saved = storageSaveDailyState(g_daily_state);

if (record_saved) {
    // Record saved successfully - recalculate from NVS (authoritative)
    recalculateDailyTotals();
} else {
    // NVS write failed - update in-memory totals directly to preserve the drink
    Serial.println("WARNING: NVS write failed, updating in-memory totals");
    g_cached_daily_total_ml += (uint16_t)delta_ml;
    g_cached_drink_count++;
}
```

### 2. Add Retry Logic with ESP-IDF Error Codes

The Arduino `Preferences` wrapper doesn't expose error codes, but we can use the **ESP-IDF NVS API directly** to get detailed error information.

**Logging strategy** (using existing debug level system):
- **ERROR/WARNING messages**: Unconditional (`Serial.printf`) - always important
- **Retry attempts**: Use `DEBUG_PRINTF(g_debug_drink_tracking, ...)` macro - shows at debug level 1+

Debug levels: 0=OFF, 1=Events (drink_tracking), 2=+Gestures, 3=+Weight, 4=+Accel, 9=All

Modify [firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp):

**Add includes at top:**
```cpp
#include <nvs.h>
#include <nvs_flash.h>
#include "config.h"  // For DEBUG_PRINTF macro
```

**In `storageSaveDrinkRecord()` - replace simple putBytes with retry + error codes:**
```cpp
// Try up to 3 times with ESP-IDF API for error codes
nvs_handle_t nvs_handle;
esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
if (err != ESP_OK) {
    Serial.printf("ERROR: nvs_open failed: 0x%x\n", err);  // Unconditional - critical
    return false;
}

size_t written = 0;
esp_err_t last_err = ESP_OK;
for (int retry = 0; retry < 3 && written != sizeof(DrinkRecord); retry++) {
    if (retry > 0) {
        // Conditional - shows at debug level 1+ (drink tracking)
        DEBUG_PRINTF(g_debug_drink_tracking, "NVS write retry %d/3 (last error: 0x%x)...\n", retry + 1, last_err);
        delay(10);
    }
    last_err = nvs_set_blob(nvs_handle, key, &record_with_id, sizeof(DrinkRecord));
    if (last_err == ESP_OK) {
        last_err = nvs_commit(nvs_handle);
        if (last_err == ESP_OK) {
            written = sizeof(DrinkRecord);
        }
    }
}
nvs_close(nvs_handle);

if (written != sizeof(DrinkRecord)) {
    Serial.printf("ERROR: NVS write failed after 3 retries (error: 0x%x)\n", last_err);  // Unconditional
    return false;
}
```

**Common ESP-IDF NVS error codes to log:**
- `0x1102` (ESP_ERR_NVS_NOT_INITIALIZED) - NVS not initialized
- `0x1105` (ESP_ERR_NVS_NOT_ENOUGH_SPACE) - Partition full
- `0x1108` (ESP_ERR_NVS_REMOVE_FAILED) - Flash write failed
- `0x110b` (ESP_ERR_NVS_INVALID_STATE) - NVS corrupted

Apply similar pattern to `storageSaveDailyState()` and `storageSaveBufferMetadata()`.

### 3. Display Warning Screen on NVS Failure

When NVS writes fail after all retries, display a warning on the e-paper for 3 seconds so the user knows something went wrong.

**Add to [firmware/src/display.cpp](firmware/src/display.cpp):**
```cpp
void displayNVSWarning() {
    if (!g_display || !g_initialized) return;

    Serial.println("Display: Showing NVS warning");

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Warning text
    printCentered("Storage", 35, 3);
    printCentered("Error", 70, 3);

    g_display->display();  // Full refresh
    delay(3000);           // Show for 3 seconds

    // Redraw main screen
    drawMainScreen();
}
```

**Add declaration to [firmware/include/display.h](firmware/include/display.h):**
```cpp
void displayNVSWarning();
```

**Call from drinks.cpp when NVS fails:**
```cpp
if (!record_saved) {
    Serial.println("WARNING: NVS write failed, updating in-memory totals");
    g_cached_daily_total_ml += (uint16_t)delta_ml;
    g_cached_drink_count++;
    displayNVSWarning();  // Show warning to user
}
```

### 4. Add NVS Health Diagnostic (Optional)

Add a function to check NVS health at boot:

**New function in storage_drinks.cpp:**
```cpp
bool storageDiagnoseNVS() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        Serial.printf("NVS DIAG: Failed to open (0x%x)\n", err);
        return false;
    }

    // Try a test write/read cycle
    uint8_t test = 0xAA;
    err = nvs_set_u8(nvs_handle, "_nvs_test", test);
    if (err == ESP_OK) err = nvs_commit(nvs_handle);

    uint8_t read_back = 0;
    if (err == ESP_OK) err = nvs_get_u8(nvs_handle, "_nvs_test", &read_back);

    nvs_erase_key(nvs_handle, "_nvs_test");
    nvs_close(nvs_handle);

    if (err == ESP_OK && read_back == 0xAA) {
        Serial.println("NVS DIAG: Write/read test PASSED");
        return true;
    } else {
        Serial.printf("NVS DIAG: Write/read test FAILED (0x%x)\n", err);
        return false;
    }
}
```

## Files to Modify

1. [firmware/src/drinks.cpp](firmware/src/drinks.cpp) - Handle NVS failures gracefully, call warning display (lines 256-263)
2. [firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp) - Add ESP-IDF NVS API with retry logic and error codes
3. [firmware/include/storage_drinks.h](firmware/include/storage_drinks.h) - Add diagnostic function declaration
4. [firmware/src/display.cpp](firmware/src/display.cpp) - Add `displayNVSWarning()` function
5. [firmware/include/display.h](firmware/include/display.h) - Add `displayNVSWarning()` declaration

## Verification

1. Build firmware: `~/.platformio/penv/bin/platformio run`
2. Manual testing:
   - Take drinks and verify they're recorded even if NVS has issues
   - Monitor serial output for any NVS errors
   - Check that daily totals accumulate correctly
3. Simulate NVS failure:
   - Could add a debug flag to force NVS write failure for testing

## Risk Assessment

- **Low risk**: Changes are defensive - they handle error cases that currently cause data loss
- **No regression**: Normal NVS operation is unchanged
- **Fallback**: If NVS fails, drinks are still tracked in memory for the current session

## Limitations

- If NVS persistently fails, drinks tracked in memory will be lost when the device goes to sleep (RTC memory preserves baseline but not drink records)
- A more robust solution would backup failed drink records to RTC memory, but this adds complexity
- The immediate fix prioritizes not losing drinks during the current session over persistence guarantees
