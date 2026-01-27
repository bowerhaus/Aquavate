# Aquavate - Active Development Progress

**Last Updated:** 2026-01-27 (Session 4)
**Current Branch:** `ios-calibration-flow`

---

## Current Task

**iOS Calibration Flow (Issue #30)** - [Plan 060](Plans/060-ios-calibration-flow.md)

Add the ability to perform two-point calibration from the iOS app with guided screens and real-time device feedback.

### Implementation Phases

- [x] **Phase 1: Firmware Changes** - Add BLE calibration commands ✓
- [x] **Phase 2: iOS BLE Updates** - Fix command codes and add calibration methods ✓
- [x] **Phase 3: iOS Calibration UI** - Create guided calibration screens ✓
- [x] **Phase 4: Integration** - Connect to SettingsView ✓
- [x] **Phase 4.5: Keep-Alive During Calibration** - Prevent disconnects ✓
- [x] **Phase 4.6: BLE Updates During Calibration** - Fix stability indicator not updating ✓
- [ ] **Phase 5: Testing** - Verify end-to-end with hardware

### Session 4 Changes

**Bug Fixed: Stability indicator never updated during calibration**

Root cause: BLE state updates (weight, stability) were only sent when `g_calibrated == true`:
```cpp
// Line 1569 in main.cpp - this requires calibration to exist!
if (cal_state == CAL_IDLE && g_calibrated && (gesture == GESTURE_UPRIGHT_STABLE ...)) {
    bleUpdateCurrentState(...);
}
```
During calibration, the bottle typically ISN'T calibrated yet, so no BLE updates were sent.

Fix: Added new block in main.cpp that sends BLE updates every 500ms when `bleIsCalibrationInProgress()`:
```cpp
#if ENABLE_BLE
    if (bleIsCalibrationInProgress()) {
        // Update every 500ms during calibration
        bleUpdateCurrentState(daily_total, current_adc, g_calibration,
                            battery_pct, g_calibrated, g_time_valid,
                            gesture == GESTURE_UPRIGHT_STABLE);
    }
#endif
```

**UI Improvements:**
- Removed all references to "sensor puck" (sensor is built into bottle)
- Improved instruction text clarity on all calibration screens
- Made "Measure Empty" and "Measure Full" buttons always enabled (firmware handles stability internally)
- Changed hint text from blocking orange warning to informational gray text

### Key Files Modified This Session

**Firmware:**
- `firmware/src/main.cpp` - Added BLE updates during calibration mode (lines ~1663-1688)

**iOS:**
- `ios/Aquavate/Aquavate/Views/CalibrationView.swift` - Improved instruction text, removed stability gate from buttons

### Current Step

**Phase 5: Hardware Testing** - Need to flash firmware and test

**Next action:** Flash updated firmware and test full calibration flow with real hardware

### Calibration Flow Summary

**User-facing flow:**
1. Navigate to Settings > Calibrate Bottle
2. Welcome screen explains what you'll need
3. Step 1: Empty bottle completely, cap on, set upright, tap "Measure Empty"
4. Step 2: Fill with exactly 830ml, cap on, set upright, tap "Measure Full"
5. Tap "Save Calibration" to store to device
6. Done - bottle is calibrated

**BLE communication:**
1. iOS sends `CAL_MEASURE_POINT` command (empty or full)
2. Firmware sets `g_cal_mode = true` (blocks sleep), takes ~10s stable measurement
3. Firmware sends BLE state updates every 500ms during calibration
4. iOS reads `calibrationRawADC` from BLEManager when ready
5. After both measurements, iOS calculates scale factor and sends `CAL_SET_DATA`
6. Firmware saves to NVS, clears `g_cal_mode`

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md
```

---

## Recently Completed

- **LittleFS Drink Storage / NVS Fragmentation Fix (Issue #76)** - [Plan 059](Plans/059-littlefs-drink-storage.md)
  - Root cause: NVS doesn't support in-place updates, fragments after ~136 drink writes
  - Solution: LittleFS file storage for drink records (true in-place overwrites)
  - NVS retained for calibration, settings, daily state (with retry logic)
  - Also fixed: display garbage on cold boot, IOS_MODE=0 build error
  - Files: partitions.csv (new), storage_drinks.cpp, drinks.h, main.cpp, display.cpp, and others

- Drink Baseline Hysteresis Fix (Issue #76) - [Plan 057](Plans/057-drink-baseline-hysteresis.md)
  - Fixed drinks sometimes not recorded when second drink taken during same session
  - Added drift threshold (15ml) to prevent baseline contamination during drink detection
  - Only firmware changes (config.h, drinks.cpp)

- Unified Sessions View Fix (Issue #74) - [Plan 056](Plans/056-unified-sessions-view.md)
  - Replaced confusing separate "Recent Motion Wakes" and "Backpack Sessions" sections
  - New unified "Sessions" list shows both types chronologically
  - Summary changed from "Since Last Charge" to "Last 7 Days"
  - Users can now clearly see backpack sessions with correct 1+ hour durations

- Repeated Amber Notification Fix (Issue #72) - [Plan 055](Plans/055-repeated-amber-notification-fix.md)
  - Fixed `lastNotifiedUrgency` not persisting across app restarts (notifications were re-sent)
  - Updated urgency colors: attention = RGB(247,239,151), overdue = red
  - Human figure now uses smooth gradient from attention to overdue color
  - Added white background layer to fix color blending issues

- iOS Day Boundary Fix (Issue #70) - [Plan 054](Plans/054-ios-day-boundary-fix.md)
  - Fixed drinks from previous day showing after midnight
  - Made @FetchRequest predicate dynamic with onAppear and significantTimeChangeNotification

- Notification Threshold Adjustment (Issue #67) - [Plan 053](Plans/053-notification-threshold-adjustment.md)
  - Increased minimum deficit thresholds for hydration notifications (50ml -> 150ml)

- iOS Memory Exhaustion Fix (Issue #28) - [Plan 052](Plans/052-ios-memory-exhaustion-fix.md)
  - Fixed memory leaks from WatchConnectivity queue, CoreData @FetchRequest, NotificationManager captures
  - App now runs 60+ minutes without memory exhaustion

- Foreground Notification Fix (Issue #56) - [Plan 051](Plans/051-foreground-notification-fix.md)
  - Fixed hydration reminders not appearing when app is in foreground
  - Added UNUserNotificationCenterDelegate to NotificationManager

---

## Branch Status

- `master` - Stable baseline
- `ios-calibration-flow` - **ACTIVE** - iOS calibration flow (Issue #30)

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
