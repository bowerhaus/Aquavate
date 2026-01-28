# Plan: Daily Goal Setting from iOS App

**STATUS: COMPLETE** - All device testing passed

## Summary

Add user-configurable daily hydration goal setting from the iOS app Settings screen, with BLE transfer and NVS persistence on the bottle firmware.

**GitHub Issue:** #83

**User choices:**
- UI: Stepper with ±250ml steps
- Range: 1000-4000ml
- Offline: Require connection (stepper disabled when disconnected)

## Critical Finding

**The daily goal is NOT currently persisted to NVS** - it resets to 2500ml on every power cycle. The BLE infrastructure exists (`BLE_BottleConfig` includes `daily_goal_ml`), but `bleSaveBottleConfig()` only saves calibration data, not the goal.

## Implementation

### Phase 1: Firmware - NVS Persistence (COMPLETE)

**1.1 Add storage functions** in [storage.cpp](../firmware/src/storage.cpp):
```cpp
static const char* KEY_DAILY_GOAL = "daily_goal_ml";

bool storageSaveDailyGoal(uint16_t goal_ml);
uint16_t storageLoadDailyGoal();  // Returns DRINK_DAILY_GOAL_DEFAULT_ML if not set
```

**1.2 Add declarations** in [storage.h](../firmware/include/storage.h)

**1.3 Update BLE service** in [ble_service.cpp](../firmware/src/ble_service.cpp):
- `bleSaveBottleConfig()` - call `storageSaveDailyGoal()`
- `bleLoadBottleConfig()` - call `storageLoadDailyGoal()`

**1.4 Add constants** in [config.h](../firmware/include/config.h):
```cpp
#define DRINK_DAILY_GOAL_MIN_ML     1000
#define DRINK_DAILY_GOAL_MAX_ML     4000
#define DRINK_DAILY_GOAL_DEFAULT_ML 2500
```

### Phase 2: iOS - Settings UI (COMPLETE)

**2.1 Update SettingsView** in [SettingsView.swift](../ios/Aquavate/Aquavate/Views/SettingsView.swift):
- Replace read-only goal display with Stepper (when connected)
- Show read-only when disconnected (stepper disabled)
- Range: 1000-4000ml, step: 250ml

**2.2 Add goal setter** in [BLEManager.swift](../ios/Aquavate/Aquavate/Services/BLEManager.swift):
- `setDailyGoal(_ ml: Int)` with 0.5s debounce
- Clamp to valid range
- Write to bottle via existing `writeBottleConfig()`

---

### Phase 3: iOS - Fix Read-Modify-Write for Bottle Config (COMPLETE)

**Problem discovered during testing:** When setting the daily goal, calibration data gets corrupted because `writeBottleConfig()` sends hardcoded `scaleFactor: 1.0` and `tareWeightGrams: 0` instead of preserving actual calibration values.

The firmware (with Issue #84 fix) validates incoming `scale_factor` and rejects values outside 100-800 ADC/g range. Since `1.0` is invalid, the firmware **rejects the entire write** - so the goal is never saved to the bottle.

**3.1 Add calibration storage properties** in [BLEManager.swift](../ios/Aquavate/Aquavate/Services/BLEManager.swift) (around line 154):

```swift
// Calibration values (not user-editable, stored for read-modify-write pattern)
// These are read from the device on connection and preserved when writing config updates
private var lastKnownScaleFactor: Float = 0
private var lastKnownTareWeightGrams: Int32 = 0
```

**3.2 Update handleBottleConfigUpdate()** in [BLEManager.swift](../ios/Aquavate/Aquavate/Services/BLEManager.swift):

```swift
// Store calibration values for read-modify-write pattern
// These are preserved when writing config updates to avoid corrupting calibration
lastKnownScaleFactor = config.scaleFactor
lastKnownTareWeightGrams = config.tareWeightGrams
```

**3.3 Fix writeBottleConfig()** in [BLEManager.swift](../ios/Aquavate/Aquavate/Services/BLEManager.swift):

- Added validation: Require valid calibration data (scaleFactor 100-800) before writing
- Use stored calibration values instead of hardcoded defaults
- Log includes scaleFactor and tareWeight for debugging

**3.4 Reset calibration on disconnect** in [BLEManager.swift](../ios/Aquavate/Aquavate/Services/BLEManager.swift):

```swift
// Reset calibration values to prevent stale data on reconnect
// These will be refreshed when bottle config is read after reconnection
lastKnownScaleFactor = 0
lastKnownTareWeightGrams = 0
```

---

## Files Modified

| File | Changes |
|------|---------|
| [firmware/src/storage.cpp](../firmware/src/storage.cpp) | Add `storageSaveDailyGoal()`, `storageLoadDailyGoal()` (DONE) |
| [firmware/include/storage.h](../firmware/include/storage.h) | Add function declarations (DONE) |
| [firmware/src/ble_service.cpp](../firmware/src/ble_service.cpp) | Update save/load to include daily goal (DONE) |
| [firmware/include/config.h](../firmware/include/config.h) | Add goal min/max/default constants (DONE) |
| [ios/.../SettingsView.swift](../ios/Aquavate/Aquavate/Views/SettingsView.swift) | Add Stepper UI (DONE) |
| [ios/.../BLEManager.swift](../ios/Aquavate/Aquavate/Services/BLEManager.swift) | Add `setDailyGoal()` method (DONE), fix read-modify-write (DONE) |

## Verification

1. **Build iOS app**: `cd ios/Aquavate && xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`

2. **Test on device**:
   - Connect to calibrated bottle
   - Check logs show valid `scaleFactor` on connect (should be 200-600 range)
   - Change daily goal in Settings
   - Verify logs show goal write with preserved `scaleFactor`
   - Verify human figure updates in HomeView
   - Disconnect and reconnect - verify goal persisted on bottle

3. **Verify calibration preserved**:
   - After goal change, reconnect iOS and verify `scaleFactor` logged is same as before
   - Optional: Use firmware serial `s` command (IOS_MODE=0 build) to verify `scale_factor` unchanged
