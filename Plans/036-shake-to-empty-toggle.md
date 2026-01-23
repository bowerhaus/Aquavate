# Plan: Shake-to-Empty Toggle Setting (Issue #32)

**Status:** Complete

## Overview
Add a toggle in iOS Settings to enable/disable the shake-to-empty gesture. The setting syncs to firmware via BLE and persists in NVS.

## Design Decision
**Create new "Device Settings" BLE characteristic** (not extend BottleConfig) because:
- Separates user preferences from calibration data
- Allows future settings without affecting calibration
- Clean extension of existing pattern

## BLE Protocol

**New Characteristic: Device Settings**
- UUID: `6F75616B-7661-7465-2D00-000000000006`
- Properties: READ/WRITE
- Size: 4 bytes

```c
struct BLE_DeviceSettings {
    uint8_t  flags;      // Bit 0: shake_to_empty_enabled (1=on, 0=off)
    uint8_t  reserved1;
    uint16_t reserved2;
};
```

## Implementation Tasks

### 1. Firmware Storage ✅
**Files:** [storage.h](../firmware/include/storage.h), [storage.cpp](../firmware/src/storage.cpp)
- Add NVS key `shake_empty_en`
- Add `storageSaveShakeToEmptyEnabled(bool)`
- Add `storageLoadShakeToEmptyEnabled()` → returns `true` (default enabled)

### 2. Firmware BLE Service ✅
**Files:** [ble_service.h](../firmware/include/ble_service.h), [ble_service.cpp](../firmware/src/ble_service.cpp)
- Add UUID `AQUAVATE_DEVICE_SETTINGS_UUID`
- Add `BLE_DeviceSettings` struct with flag constant
- Add `DeviceSettingsCallbacks` class (onRead loads from NVS, onWrite saves to NVS)
- Add `bleGetShakeToEmptyEnabled()` public function
- Register characteristic in `bleInit()`

### 3. Firmware Main Loop ✅
**File:** [main.cpp](../firmware/src/main.cpp) ~line 1037
- Check `bleGetShakeToEmptyEnabled()` before acting on `GESTURE_SHAKE_WHILE_INVERTED`
- Log when gesture ignored due to setting

### 4. iOS BLE Constants ✅
**File:** [BLEConstants.swift](../ios/Aquavate/Aquavate/Services/BLEConstants.swift)
- Add `deviceSettingsUUID` constant
- Add to `aquavateCharacteristics` array

### 5. iOS BLE Structs ✅
**File:** [BLEStructs.swift](../ios/Aquavate/Aquavate/Services/BLEStructs.swift)
- Add `BLEDeviceSettings` struct with:
  - `parse(from:)` static method
  - `toData()` serialization
  - `isShakeToEmptyEnabled` computed property
  - `create(shakeToEmptyEnabled:)` factory

### 6. iOS BLE Manager ✅
**File:** [BLEManager.swift](../ios/Aquavate/Aquavate/Services/BLEManager.swift)
- Add `@Published var isShakeToEmptyEnabled: Bool = true`
- Add `handleDeviceSettingsUpdate(_:)` for characteristic updates
- Add `setShakeToEmptyEnabled(_:)` public method
- Add to `checkDiscoveryComplete()` required characteristics
- Handle in `didUpdateValueFor` switch

### 7. iOS Settings UI ✅
**File:** [SettingsView.swift](../ios/Aquavate/Aquavate/Views/SettingsView.swift)
- Add "Gestures" section (inside the `if bleManager.connectionState.isConnected` block, after Device Commands)
- Add toggle for "Shake to Empty" with description
- Disabled when not connected

## Default Behavior
- **Firmware default**: Enabled (true) - standalone bottles work as before
- **First iOS connection**: Reads current setting from firmware
- **Setting persists**: Survives reboots, app reinstalls

## Verification
1. ✅ Build firmware with `platformio run`
2. Use nRF Connect to verify Device Settings characteristic appears (UUID ending in 0006)
3. Read characteristic - should return 0x01 (enabled)
4. Write 0x00 - shake gesture should be ignored
5. Reboot device - setting should persist
6. ✅ Build and run iOS app
7. Toggle in Settings - verify shake behavior changes
8. Kill and relaunch app - setting should read correctly from device
