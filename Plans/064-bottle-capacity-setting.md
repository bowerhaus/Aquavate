# Plan: Add Bottle Capacity Setting from iOS App

## Scope Assessment

This is a **moderate feature** that follows the existing daily goal pattern for the settings plumbing, but also requires updates to the calibration flow. The BLE characteristic already includes a `bottle_capacity_ml` field — it needs NVS persistence, a UI control in iOS, and calibration logic updated to use the dynamic value instead of a hardcoded 830ml.

## Calibration Impact

The hardcoded 830ml is not just used for display — it's embedded in the calibration logic in ways that would break for different bottle sizes:

- **Scale factor calculation** (`calibration.cpp:291`): passes `CALIBRATION_BOTTLE_VOLUME_ML` (830) to compute `scale_factor = ADC_diff / water_weight_grams`. A different capacity means a different water weight, so the wrong scale factor gets calculated if the constant doesn't match reality.
- **ADC validation threshold** (`calibration.cpp:191`): expects `weight_delta > 300,000` ADC units, which is based on 830ml of water at ~400 ADC/gram. A 500ml bottle would produce ~180,000 and **fail calibration entirely**.
- **BLE full_bottle_adc recalculation** (`ble_service.cpp:640`): hardcodes `830.0f * scale_factor` when reconstructing calibration data from BLE config.
- **Display clamping/percentage** (`display.cpp:804,809`): hardcodes 830 for fill bar.

The iOS `CalibrationManager` already uses the dynamic `bottleCapacityMl` from BLE, so the iOS side is mostly correct.

## Changes Required

### Firmware

1. **`firmware/include/config.h`** — Add min/max/default constants, keep `CALIBRATION_BOTTLE_VOLUME_ML` as the default but it will no longer be used as a hardcoded value in calculations.

2. **`firmware/src/storage.cpp` + `firmware/include/storage.h`** — Add `storageSaveBottleCapacity()` / `storageLoadBottleCapacity()` following the daily goal pattern.

3. **`firmware/src/calibration.cpp`** — Use the stored bottle capacity instead of `CALIBRATION_BOTTLE_VOLUME_ML`:
   - Scale factor calculation must use the actual configured capacity
   - ADC validation threshold must scale proportionally with the configured capacity

4. **`firmware/src/ble_service.cpp`** — Wire up persistence and fix recalculation:
   - `bleLoadBottleConfig()`: load capacity from NVS
   - `bleSaveBottleConfig()`: persist capacity to NVS
   - Replace hardcoded `830.0f` in full_bottle_adc recalculation with stored capacity

5. **`firmware/src/display.cpp` + `firmware/include/display.h`** — Replace hardcoded 830 with dynamic capacity for clamping and fill percentage.

6. **`firmware/src/main.cpp`** — Initialize bottle capacity on wake from NVS/BLE.

### iOS App

7. **`ios/Aquavate/Aquavate/Services/BLEManager.swift`** — Add published property, setter with debounce, handle in config read/write.

8. **`ios/Aquavate/Aquavate/Views/SettingsView.swift`** — Add bottle capacity control to Device Information page: text field with +/- 10ml stepper, range 200–2000ml, disabled when not connected.

### UX Consideration

- When bottle capacity changes, the user should be prompted or advised to recalibrate, since the existing scale factor was calculated assuming the previous capacity. The calibration instructions tell the user to fill to the bottle's capacity, so the scale factor is only valid for the capacity it was calibrated against.

## Verification

- Build firmware: `cd firmware && ~/.platformio/penv/bin/platformio run`
- Build iOS: `cd ios/Aquavate && xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`
- Manual test: change capacity, verify calibration threshold adjusts, verify display uses new value, verify persistence across deep sleep
