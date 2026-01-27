# Plan: LittleFS Storage for Drink Records (NVS Fragmentation Fix)

**Status:** Complete
**GitHub Issue:** #76
**Branch:** `nvs-write-failure-handling`
**Date:** 2026-01-27

## Summary

Replace NVS-based drink record storage with LittleFS file storage to eliminate fragmentation issues. NVS will continue to be used for calibration, settings, and daily state (with existing retry logic preserved).

## Problem

NVS doesn't support in-place updates - every write marks the old entry as deleted and writes to a new location. After ~136 drink record writes, the partition fragments and returns `ESP_ERR_NVS_NOT_ENOUGH_SPACE` (0x1105), causing drinks to be lost.

## Solution

Store drink records in a LittleFS file with fixed-size slots that support true in-place overwrites. No fragmentation.

---

## Implementation Steps

### 1. Create Custom Partition Table

Create `firmware/partitions.csv`:
```csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x140000,
app1,     app,  ota_1,   0x150000,0x140000,
littlefs, data, spiffs,  0x290000,0x10000,
```

- NVS: 20KB (unchanged - calibration, settings, daily state)
- LittleFS: 64KB (plenty for drink records)

### 2. Update platformio.ini

Change partition reference in `[env:adafruit_feather]`:
```ini
board_build.partitions = partitions.csv
```

### 3. Rewrite storage_drinks.cpp with LittleFS

Replace NVS-based drink record storage with LittleFS file I/O:

```cpp
#include <LittleFS.h>

#define DRINK_FILE "/drinks.bin"
#define META_FILE "/meta.bin"

// Fixed-size record file layout:
// - Record N is at offset N * sizeof(DrinkRecord)
// - Supports true in-place overwrites

bool storageInitDrinkFS() {
    if (!LittleFS.begin(true)) {  // true = format if needed
        Serial.println("ERROR: LittleFS mount failed");
        return false;
    }
    Serial.println("LittleFS mounted");
    return true;
}
```

### 4. Update Header File

Modify `firmware/include/storage_drinks.h`:
- Add `bool storageInitDrinkFS();` declaration
- Keep all existing function signatures (API unchanged)

### 5. Initialize LittleFS at Startup

In `firmware/src/main.cpp` setup():
```cpp
// Initialize LittleFS for drink storage
if (!storageInitDrinkFS()) {
    Serial.println("WARNING: Drink storage unavailable");
}
```

### 6. Keep NVS for Non-Drink Data

These stay in NVS (with existing retry logic):
- Calibration data (`storage.cpp`)
- Daily state (`storageSaveDailyState` / `storageLoadDailyState`)
- Settings/debug level

---

## Files to Modify

| File | Changes |
|------|---------|
| `firmware/partitions.csv` | **New** - custom partition table with LittleFS |
| `firmware/platformio.ini` | Point to custom partition table |
| `firmware/src/storage_drinks.cpp` | Replace NVS drink storage with LittleFS file I/O |
| `firmware/include/storage_drinks.h` | Add `storageInitDrinkFS()` declaration |
| `firmware/src/main.cpp` | Call `storageInitDrinkFS()` at startup |

---

## Data Migration

**On first boot after update:**
- LittleFS partition is auto-formatted (empty)
- Old NVS drink records are lost (136 records)
- Calibration and settings preserved in NVS
- Fresh start with no fragmentation

No migration code needed - starting fresh is cleaner given the fragmentation issues.

---

## What Stays in NVS (with retries)

| Data | Location | Retry Logic |
|------|----------|-------------|
| Calibration (zero, scale) | `storage.cpp` | Existing |
| Daily state (baseline ADC) | `storage_drinks.cpp` | Keep 3-retry with ESP-IDF API |
| Debug level | `storage.cpp` | Existing |

---

## Verification

1. **Build**: `~/.platformio/penv/bin/platformio run`
   - Check IRAM usage (should be similar to before)
   - Verify no compile errors

2. **Upload and test**:
   - First boot should show "LittleFS mounted"
   - Calibration should be preserved
   - Take several drinks, verify recording works
   - Reboot, verify drinks persist
   - Take 10+ drinks rapidly, verify no fragmentation errors

3. **Long-term**: Monitor for any storage errors over multiple days

---

## Risk Assessment

- **Low risk**: LittleFS is mature, included in ESP32 Arduino core
- **Data loss**: Only historical drink records (136) - calibration preserved
- **Rollback**: Can revert to NVS-only by changing partition table back

---

## Supersedes

This plan supersedes [058-nvs-write-failure-handling.md](058-nvs-write-failure-handling.md), which addressed the same NVS fragmentation issue but with a less robust solution (retries + graceful degradation rather than eliminating the root cause).
