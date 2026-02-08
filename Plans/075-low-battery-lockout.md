# Plan: Low Battery Lockout (Issue #68) ✅ COMPLETE

## Context

The Aquavate bottle currently has no proactive low battery notification. Users can be caught off-guard when the bottle stops working due to a depleted battery. This feature adds:
1. **iOS early warning**: The iOS app warns the user (push notification + in-app banner) when battery drops below the *warning* level (lockout + 5%). The bottle is still functional at this point, giving the user advance notice.
2. **Firmware lockout**: A full-screen "charge me" icon on the e-paper display when battery drops below the *lockout* threshold. The bottle becomes unusable until charged above the recovery level.
3. The threshold is runtime-configurable via serial command (85% for testing, 20% for production), persisted in NVS.

## Two-Tier Battery Warning System

| Level | Default | Trigger | What happens |
|-------|---------|---------|-------------|
| **Warning** | 25% (lockout + 5%) | Battery < warning level | BLE flag set → iOS app shows banner + push notification. Bottle still works normally. |
| **Lockout** | 20% | Battery < lockout level | Full-screen "charge me" on e-paper. No drink detection, no BLE. Timer-only deep sleep. |
| **Recovery** | 25% (= warning level) | Battery >= recovery level | Lockout clears, normal operation resumes. |

The warning and recovery thresholds are the same value (lockout + offset), which is elegant: the iOS warns while the bottle works, then the firmware locks out, and recovery clears both.

## Design Decisions

- **Hysteresis**: Lock at threshold (default 20%), recover at threshold + 5% (default 25%). Prevents oscillation near the boundary. The 5% offset is a compile-time constant.
- **iOS warns early**: The BLE `LOW_BATTERY` flag is set when battery < (lockout + offset), i.e., at 25%. This fires while the bottle is still operational, so the iOS app reliably sees it during normal background auto-reconnection.
- **No BLE during lockout**: The bottle does NOT advertise during lockout sleep, to conserve remaining battery. By the time lockout triggers, the iOS app has already warned the user during the warning phase.
- **Timer-only sleep during lockout**: No motion wake (saves power). Health check wakes every **15 minutes** during lockout (vs 2 hours normally) so recovery is detected quickly after charging. The power cost is negligible (~0.03mAh per check).
- **RTC + NVS for threshold**: NVS stores the threshold persistently. An RTC variable caches it for fast access during the early lockout check in setup(). `storageInit()` is called early (it's idempotent) to load the threshold before the lockout check.

## Files to Modify

### Firmware

#### 1. `firmware/include/config.h` — Add threshold constants
After the Battery Voltage section (~line 184):
```cpp
// Low Battery Lockout
#define LOW_BATTERY_LOCKOUT_PCT_DEFAULT  20      // Default lockout threshold (%)
#define LOW_BATTERY_RECOVERY_OFFSET      5       // Recovery = lockout + this (%)
#define LOW_BATTERY_CHECK_INTERVAL_SEC   900     // 15 minutes between checks during lockout
```

#### 2. `firmware/include/storage.h` — Add storage function declarations
```cpp
bool storageSaveLowBatteryThreshold(uint8_t percent);
uint8_t storageLoadLowBatteryThreshold();
```

#### 3. `firmware/src/storage.cpp` — Add NVS key and functions
Add key: `static const char* KEY_LOW_BAT_THR = "low_bat_thr";`

Add save/load functions following the existing pattern (e.g., `storageSaveDisplayMode`/`storageLoadDisplayMode`). Default return value: `LOW_BATTERY_LOCKOUT_PCT_DEFAULT`.

#### 4. `firmware/include/display.h` — Add function declaration
```cpp
void displayLowBattery();
```

#### 5. `firmware/src/display.cpp` — Add full-screen low battery display
New `displayLowBattery()` function following the pattern of `displayBackpackMode()` (line 694):
- `clearBuffer()` → draw large battery outline (80x40px, thick border) centered on display
- Exclamation mark "!" inside the battery (textSize=3)
- Battery terminal nub on right side
- "charge me" text below (textSize=3, centered)
- "battery too low to operate" note at bottom (textSize=1)
- `display()` to refresh

#### 6. `firmware/src/main.cpp` — Core lockout logic

**RTC variables** (after existing RTC declarations, ~line 75):
```cpp
RTC_DATA_ATTR bool rtc_low_battery_lockout = false;
RTC_DATA_ATTR uint8_t rtc_low_battery_threshold = LOW_BATTERY_LOCKOUT_PCT_DEFAULT;
```

**Early lockout check in setup()** — after battery read (line 745) and before sensor init:
1. Call `storageInit()` early (idempotent — safe to call again at line 818)
2. Load threshold from NVS into `rtc_low_battery_threshold`
3. Calculate recovery threshold = `rtc_low_battery_threshold + LOW_BATTERY_RECOVERY_OFFSET`
4. If `rtc_low_battery_lockout` is true:
   - If battery >= recovery threshold → clear lockout, log recovery, continue normal boot
   - If still below → call `displayInit(display)` + `displayLowBattery()`, enter timer-only deep sleep (LOW_BATTERY_CHECK_INTERVAL_SEC)
5. If `rtc_low_battery_lockout` is false:
   - If battery < lockout threshold → set lockout flag, show battery screen, enter timer-only deep sleep (LOW_BATTERY_CHECK_INTERVAL_SEC)

**Loop lockout check** — after the existing battery read in the display update section (~line 1780):
- If battery < lockout threshold and not already in lockout → set flag, show battery screen, stop BLE, save state to RTC, enter timer-only deep sleep (LOW_BATTERY_CHECK_INTERVAL_SEC)

#### 7. `firmware/include/ble_service.h` — Add low battery flag
```cpp
#define BLE_FLAG_LOW_BATTERY  0x20  // Bit 5: Battery below warning threshold (lockout + offset)
```
Update the `flags` field comment in `BLE_CurrentState` to include Bit 5.

#### 8. `firmware/src/ble_service.cpp` — Set flag in Current State
In `bleUpdateCurrentState()`: if `battery_percent < (rtc_low_battery_threshold + LOW_BATTERY_RECOVERY_OFFSET)`, set `BLE_FLAG_LOW_BATTERY` in flags. This triggers at the WARNING level (25%), not the lockout level (20%), giving the iOS app advance notice while the bottle is still operational. Add change detection for this flag to trigger notification to iOS.

#### 9. `firmware/src/serial_commands.cpp` — Add serial commands
- `SET LOW BATTERY LOCKOUT <percent>` — validates 5-95%, saves to NVS, updates `rtc_low_battery_threshold`
- `GET LOW BATTERY` — shows current lockout threshold, recovery threshold, and lockout state
- Add lockout info to `GET STATUS` output
- Update help text

### iOS App

#### 10. `ios/Aquavate/Aquavate/Services/BLEStructs.swift` — Parse low battery flag
Add to `BLECurrentState`:
```swift
var isLowBattery: Bool { (flags & 0x20) != 0 }
```

#### 11. `ios/Aquavate/Aquavate/Services/BLEManager.swift` — Detect and publish
- Add `@Published private(set) var isLowBattery: Bool = false`
- In `handleCurrentStateUpdate()`: detect transition to low battery, post `Notification.Name.bottleLowBatteryDetected`

#### 12. `ios/Aquavate/Aquavate/Services/NotificationManager.swift` — Push notification
Add `scheduleLowBatteryNotification(batteryPercent:)` method using fixed identifier `"bottle-low-battery"` (prevents duplicate notifications). Uses existing UNUserNotificationCenter infrastructure.

#### 13. `ios/Aquavate/Aquavate/Views/HomeView.swift` — In-app warning
- Add red "Low Battery" status badge when `bleManager.isLowBattery == true`
- Update `batteryColor` to show red when in lockout
- Wire up `.onReceive` for the low battery notification to trigger push notification via NotificationManager

## Flow

```
Normal Operation → Battery drops to 25% (warning level)
  └─ BLE flag BLE_FLAG_LOW_BATTERY set in Current State
  └─ iOS app receives notification → shows banner + push notification
  └─ Bottle continues to work normally

Normal Operation → Battery drops to 20% (lockout level)
  ├─ Show full-screen "charge me" on e-paper
  ├─ Set rtc_low_battery_lockout = true
  └─ Enter timer-only deep sleep (check every 15min, no BLE, no motion wake)

Lockout Check Wake (every 15min) → Read battery
  ├─ Battery < 25% (recovery): re-display icon → sleep again
  └─ Battery >= 25% (recovery): clear lockout → normal operation resumes

Power cycle/reset → RTC cleared → fresh battery check → lockout if needed
```

## Verification

1. **Build firmware**: `cd firmware && ~/.platformio/penv/bin/platformio run` — confirm no IRAM overflow
2. **Test with threshold=85%**: `SET LOW BATTERY LOCKOUT 85` via serial → bottle should enter lockout immediately on next wake → e-paper shows "charge me" → serial shows lockout messages
3. **Test persistence**: After health check wake (~2h, or reduce `HEALTH_CHECK_WAKE_INTERVAL_SEC` for testing), serial shows "Still in low battery lockout" and device re-sleeps
4. **Test recovery**: Connect USB charger → wait for health check wake → battery above 90% → serial shows "Battery recovered" → normal operation resumes
5. **Test BLE flag**: Connect iOS app while battery is below threshold → app shows red "Low Battery" badge → push notification delivered
6. **Test serial commands**: `GET LOW BATTERY` shows thresholds and state; `SET LOW BATTERY LOCKOUT 20` restores production threshold
7. **Build iOS**: `cd ios/Aquavate && xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`
