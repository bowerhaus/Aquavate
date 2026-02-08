# Plan: Fix Drink Detection Baseline Loss from BLE SET_TIME

## Context

When pouring ~150ml from a bottle until empty, the drink was not detected. Logs showed:
```
Drinks: baseline=-4.3ml, current=-4.4ml, delta=0.1ml
[BLE] Battery level updated: 100%
```

The baseline was at "empty" (-4.3ml) when it should have been at ~150ml.

**Root cause:** When the iOS app connects via BLE and sends a `SET_TIME` command, the BLE handler at [ble_service.cpp:197](firmware/src/ble_service.cpp#L197) calls `drinksInit()`. This **zeros the baseline** (`last_recorded_adc = 0` at [drinks.cpp:170](firmware/src/drinks.cpp#L170)). If this happens while the user is holding the bottle (between pickup and putdown), the RTC-restored baseline is destroyed. The next `drinksUpdate()` — which only runs during `UPRIGHT_STABLE` — establishes a fresh baseline from the current (post-drink) reading. The drink is invisible.

**Why it's intermittent:** The SET_TIME command usually arrives while the bottle is sitting on the sensor, so the baseline gets re-established at the correct water level before the next drink. The failure only occurs when the timing is unlucky: user picks up bottle → wake → iOS connects and sends SET_TIME (resetting baseline) → user drinks → puts bottle down → baseline established at empty → drink missed.

## Changes

### 1. Guard `drinksInit()` call in BLE SET_TIME handler — [ble_service.cpp:197](firmware/src/ble_service.cpp#L197)

Only call `drinksInit()` if drink tracking is not already initialized. This is the primary fix. The BLE handler should use `drinksInit()` for the first-time initialization case (when time wasn't valid before), not as a re-init on every SET_TIME.

Add a `bool drinksIsInitialized()` function to [drinks.h](firmware/include/drinks.h) / [drinks.cpp](firmware/src/drinks.cpp) that returns `g_drinks_initialized`. Then in the BLE handler:
```cpp
if (!drinksIsInitialized()) {
    drinksInit();
}
```

### 2. Remove forced baseline reset in `drinksInit()` — [drinks.cpp:167-170](firmware/src/drinks.cpp#L167-L170)

Defence-in-depth: Remove `g_daily_state.last_recorded_adc = 0` (line 170). Let the NVS-loaded value persist as a fallback. The validation in `drinksUpdate()` (lines 210-217) catches corrupt baselines (< -100ml or > 1000ml). First-boot-ever is unchanged (NVS load fails → memset zeros state → baseline = 0 → established from first reading).

This also protects against the same issue in [serial_commands.cpp](firmware/src/serial_commands.cpp) (lines 234, 362, 448) which also calls `drinksInit()`.

### 3. Try RTC restore on ALL boot types — [main.cpp:~1014](firmware/src/main.cpp#L1014)

Move `drinksRestoreFromRTC()` call **before** the wakeup reason guard at line 1015. RTC memory survives EN-pin resets (serial monitor connect). The magic number check inside `drinksRestoreFromRTC()` already handles power-on resets (RTC cleared → magic = 0 → returns false, NVS fallback used from Change 2).

### 4. Save daily state to NVS inside `drinksSaveToRTC()` — [drinks.cpp:430](firmware/src/drinks.cpp#L430)

Add `storageSaveDailyState(g_daily_state)` inside `drinksSaveToRTC()`. Since both sleep functions already call `drinksSaveToRTC()`, this keeps the NVS baseline up-to-date with the latest drift-compensated value before every sleep. Makes the NVS a better fallback for power-on resets.

### 5. Add diagnostic logging

- In `drinksInit()`: unconditional log showing NVS-loaded baseline ADC
- In `main.cpp`: log whether RTC restore succeeded and the baseline value
- In `drinksUpdate()` (line 221): make baseline establishment log unconditional (currently debug-gated)
- In BLE SET_TIME handler: log whether drinksInit was called or skipped

## Files to Modify

| File | Changes |
|------|---------|
| [firmware/src/drinks.cpp](firmware/src/drinks.cpp) | Remove baseline reset (line 170), add `drinksIsInitialized()`, add NVS save in `drinksSaveToRTC()`, add logging |
| [firmware/include/drinks.h](firmware/include/drinks.h) | Declare `drinksIsInitialized()` |
| [firmware/src/ble_service.cpp](firmware/src/ble_service.cpp) | Guard `drinksInit()` call with `drinksIsInitialized()` check |
| [firmware/src/main.cpp](firmware/src/main.cpp) | Move `drinksRestoreFromRTC()` outside wakeup guard, add logging |

## Verification

1. **Build**: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. **Manual test — drink to empty**: Fill bottle, pick up, drink until empty, put down. Verify drink IS detected on display.
3. **Check logs**: Verify "Drinks: Init baseline" shows NVS value, "Drinks: Baseline restored from RTC" appears on wake, and BLE SET_TIME logs "skipped drinksInit (already initialized)"
4. **Normal drink detection**: Verify partial drinks still register correctly
