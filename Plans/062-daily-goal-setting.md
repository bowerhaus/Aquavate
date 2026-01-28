# Plan: Daily Goal Setting from iOS App

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

### Phase 1: Firmware - NVS Persistence

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

### Phase 2: iOS - Settings UI

**2.1 Update SettingsView** in [SettingsView.swift](../ios/Aquavate/Aquavate/Views/SettingsView.swift):
- Replace read-only goal display with Stepper (when connected)
- Show read-only when disconnected (stepper disabled)
- Range: 1000-4000ml, step: 250ml

**2.2 Add goal setter** in [BLEManager.swift](../ios/Aquavate/Aquavate/Services/BLEManager.swift):
- `setDailyGoal(_ ml: Int)` with 0.5s debounce
- Clamp to valid range
- Write to bottle via existing `writeBottleConfig()`

## Files to Modify

| File | Changes |
|------|---------|
| [firmware/src/storage.cpp](../firmware/src/storage.cpp) | Add `storageSaveDailyGoal()`, `storageLoadDailyGoal()` |
| [firmware/include/storage.h](../firmware/include/storage.h) | Add function declarations |
| [firmware/src/ble_service.cpp](../firmware/src/ble_service.cpp) | Update save/load to include daily goal |
| [firmware/include/config.h](../firmware/include/config.h) | Add goal min/max/default constants |
| [ios/.../SettingsView.swift](../ios/Aquavate/Aquavate/Views/SettingsView.swift) | Add Stepper UI |
| [ios/.../BLEManager.swift](../ios/Aquavate/Aquavate/Services/BLEManager.swift) | Add `setDailyGoal()` method |

## Verification

1. **Firmware persistence**: Change goal via BLE, power cycle bottle, verify goal retained
2. **iOS UI**: Stepper visible when connected, disabled when disconnected
3. **Integration**: Change goal on iOS, verify HomeView human figure respects new goal
4. **Build**: `platformio run` (firmware), `xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`
