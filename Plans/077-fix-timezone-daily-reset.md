# Plan: Fix Timezone / Daily Reset & BLE Sync Corruption ✅ COMPLETE

**GitHub Issue:** [#120](https://github.com/bowerhaus/Aquavate/issues/120)
**Branch:** `fix-timezone-daily-reset`

## Context

The device runs with `g_timezone_offset = 0` (default — no way to set it from iOS, only serial). This causes two linked bugs:

1. **Daily reset at wrong time**: `getTodayResetTimestamp()` calls `getCurrentUnixTime()` (pure UTC) then `gmtime_r()` and resets at `tm_hour == 0` UTC = **1am BST** in summer. User sees yesterday's total from midnight BST until 1am.

2. **BLE sync corruption during the UTC/BST midnight gap**: Between 23:00 UTC (midnight BST) and 00:00 UTC (1am BST), a user connecting iOS triggers a BLE sync immediately. The main loop checks `current_tm.tm_hour == DRINK_DAILY_RESET_HOUR` at exactly 00:00 UTC and calls `drinksResetDaily()` — which marks records as deleted — while the BLE RTOS task is still iterating those same records to send chunks. The chunk ends up with a `recordCount` that no longer matches the data, failing iOS's size validation and reporting "corrupted data error".

The fix: iOS sends timezone offset (hours) alongside the UTC timestamp on every connection. Firmware stores it and uses it **only** to shift the reset boundary — all record timestamps remain true UTC.

## Files to Modify

| File | Change |
|------|--------|
| `firmware/include/ble_service.h` | Extend `BLE_SetTimeCommand` (5→6 bytes: add `int8_t tz_offset_hours`) |
| `firmware/src/ble_service.cpp` | Read and store `tz_offset_hours` in SET_TIME handler |
| `firmware/src/drinks.cpp` | Fix `getTodayResetTimestamp()` and `getSecondsUntilRollover()` to use local midnight expressed as UTC; strip offset from `getCurrentUnixTime()` |
| `firmware/src/main.cpp` | Fix rollover wake hour check (~line 979) and sleep-display time (~line 1188) to use local time; add sync-guard |
| `ios/Aquavate/Aquavate/Services/BLEStructs.swift` | Update `BLECommand.setTime()` to include timezone offset byte |
| `ios/Aquavate/Aquavate/Services/BLEManager.swift` | Update `syncDeviceTime()` to send current timezone offset |

## Caller Audit: getCurrentUnixTime()

`getCurrentUnixTime()` currently returns `UTC + g_timezone_offset * 3600`. Removing the offset makes it return true UTC. Each caller needs to be checked:

| Caller | Needs local time? | Action |
|--------|------------------|--------|
| `drinks.cpp:203` record.timestamp | No — store UTC | ✓ correct after fix |
| `drinks.cpp:57` getTodayResetTimestamp | Yes — boundary math | Fix in Step 3 |
| `drinks.cpp:85` getSecondsUntilRollover | Yes — boundary math | Fix in Step 3 |
| `main.cpp:973` rollover wake hour check | Yes — local hour | Fix in Step 4 |
| `main.cpp:1188` e-paper sleep display time | Yes — local hour | Fix in Step 4 |
| `ble_service.cpp:953` currentState.timestamp | No — send UTC | ✓ correct after fix |
| `activity_stats.cpp:99,151,162,176` session timestamps | No — store UTC | ✓ correct after fix |

Note: The many `tv.tv_sec + (g_timezone_offset * 3600)` patterns in display.cpp and main.cpp bypass `getCurrentUnixTime()` entirely — they are already correct and untouched.

## Implementation Steps

### Step 1 — Extend BLE_SetTimeCommand (firmware/include/ble_service.h)
```cpp
struct __attribute__((packed)) BLE_SetTimeCommand {
    uint8_t  command;           // BLE_CMD_SET_TIME (0x10)
    uint32_t timestamp;         // UTC Unix timestamp
    int8_t   tz_offset_hours;   // Local timezone offset in whole hours (e.g. +1 for BST)
};
// Size: 6 bytes (was 5)
```

### Step 2 — Handle tz_offset_hours in SET_TIME handler (firmware/src/ble_service.cpp, ~line 183)

> **Note: firmware and iOS must be deployed together** — the 6-byte SET_TIME is not backward compatible with old firmware, and old iOS will not send timezone offset to new firmware. Flash firmware first, then install the new iOS build.

The existing check `value.length() == sizeof(BLE_SetTimeCommand)` will automatically match 6 bytes (the new size). Read and store the timezone offset:
```cpp
if (value.length() == sizeof(BLE_SetTimeCommand) && value[0] == BLE_CMD_SET_TIME) {
    BLE_SetTimeCommand timeCmd;
    memcpy(&timeCmd, value.data(), sizeof(BLE_SetTimeCommand));
    // ... existing RTC set logic ...
    g_timezone_offset = timeCmd.tz_offset_hours;
    storageSaveTimezone(g_timezone_offset);
}
```

### Step 3 — Fix daily reset boundary to use local midnight (firmware/src/drinks.cpp)

**Change `getCurrentUnixTime()` (drinks.cpp:37-42)** to remove the offset addition — it should return true UTC:
```cpp
uint32_t getCurrentUnixTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)tv.tv_sec;  // true UTC; callers add offset as needed
}
```

**Fix `getTodayResetTimestamp()` (drinks.cpp:56-75)** to compute local midnight as a UTC timestamp:
```cpp
static uint32_t getTodayResetTimestamp() {
    uint32_t current_utc = getCurrentUnixTime();
    uint32_t local_time = current_utc + (g_timezone_offset * 3600);
    time_t local_t = local_time;
    struct tm local_tm;
    gmtime_r(&local_t, &local_tm);

    struct tm reset_tm = local_tm;
    reset_tm.tm_hour = DRINK_DAILY_RESET_HOUR;
    reset_tm.tm_min = 0;
    reset_tm.tm_sec = 0;
    uint32_t local_midnight_adjusted = (uint32_t)mktime(&reset_tm);
    uint32_t reset_utc = local_midnight_adjusted - (g_timezone_offset * 3600);

    if (local_tm.tm_hour < DRINK_DAILY_RESET_HOUR) {
        reset_utc -= 24 * 3600;
    }

    return reset_utc;
}
```

Apply the same pattern to `getSecondsUntilRollover()` (lines ~80-110).

### Step 4 — Fix main.cpp callers that need local time

**4a. Rollover wake check (main.cpp:973-979)** — must compare local hour, not UTC:
```cpp
uint32_t current_utc = getCurrentUnixTime();
time_t local_t = current_utc + (g_timezone_offset * 3600);
struct tm current_tm;
gmtime_r(&local_t, &current_tm);
if (current_tm.tm_hour == DRINK_DAILY_RESET_HOUR && current_tm.tm_min <= 10) {
```

Add a sync-guard to prevent the race condition (reset fires during BLE sync):
```cpp
extern bool g_ble_sync_in_progress;  // tracked in ble_service.cpp
if (!g_ble_sync_in_progress && current_tm.tm_hour == DRINK_DAILY_RESET_HOUR && current_tm.tm_min <= 10) {
```
Verify `g_ble_sync_in_progress` is exported from ble_service.cpp; if not, add it there.

**4b. Sleep-display time (main.cpp:1188-1192)** — must use local time for e-paper:
```cpp
uint32_t current_utc = getCurrentUnixTime();
time_t local_t = current_utc + (g_timezone_offset * 3600);
struct tm current_tm;
gmtime_r(&local_t, &current_tm);
time_hour = current_tm.tm_hour;
time_minute = current_tm.tm_min;
```

### Step 5 — iOS: send timezone offset in BLECommand.setTime (BLEStructs.swift)

```swift
static func setTime(timestamp: UInt32, tzOffsetHours: Int8) -> Data {
    var data = Data(count: 6)
    data.withUnsafeMutableBytes { ptr in
        guard let base = ptr.baseAddress else { return }
        base.storeBytes(of: UInt8(BLE_CMD_SET_TIME), as: UInt8.self)
        base.storeBytes(of: timestamp, toByteOffset: 1, as: UInt32.self)
        base.storeBytes(of: tzOffsetHours, toByteOffset: 5, as: Int8.self)
    }
    return data
}
```

### Step 6 — iOS: send offset in syncDeviceTime (BLEManager.swift, ~line 1665)

```swift
func syncDeviceTime() {
    let currentTimestamp = UInt32(Date().timeIntervalSince1970)
    let tzOffsetHours = Int8(TimeZone.current.secondsFromGMT() / 3600)
    let data = BLECommand.setTime(timestamp: currentTimestamp, tzOffsetHours: tzOffsetHours)
    peripheral.writeValue(data, for: characteristic, type: .withResponse)
    logger.info("Sent SET_TIME: timestamp=\(currentTimestamp), tz=\(tzOffsetHours)h")
}
```

## What This Fixes

| Problem | Before | After |
|---------|--------|-------|
| Daily reset time | 1am BST (UTC midnight) | Midnight BST (local midnight) |
| Sync corruption | Race: reset fires during sync at 00:00 UTC | Reset fires at 23:00 UTC (already done before user wakes and syncs) |
| Display time (e-paper) | UTC = 1 hour behind in BST | Correct local time via stored offset |
| Timezone offset | Static 0, serial-only | Auto-updated from iOS on every connection |
| Record timestamps | True UTC (unchanged, correct) | True UTC (unchanged) |

## Verification

1. Build firmware: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. Connect iOS app — check serial log shows `tz=+1h` (BST)
3. Serial log: confirm `getTodayResetTimestamp()` boundary is at 23:00 UTC when in BST (+1)
4. Simulate midnight crossing: use `t` serial command to set time to 22:55 BST, watch daily reset fire at 23:00 UTC (midnight BST), confirm no spurious reset at 00:00 UTC
5. During active BLE sync, confirm no "Invalid drink data chunk" errors in iOS console
6. Build iOS: `cd ios/Aquavate && xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`
