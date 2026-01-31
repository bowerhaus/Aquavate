# Plan: Double-Tap Gesture to Enter Extended Deep Sleep (Backpack Mode)

## Summary

Add a double-tap gesture while the bottle is **awake** to manually enter extended deep sleep (backpack mode). This mirrors the existing double-tap-to-wake and gives the user a quick way to put the bottle to sleep before placing it in a backpack, without waiting the 3-minute auto-detection.

## Approach: ADXL343 Hardware Double-Tap Detection

The ADXL343 already has built-in hardware double-tap detection (used for wake). We enable it during awake mode too, and poll the INT_SOURCE register each loop cycle to detect it.

- Same 3.0g threshold as the wake double-tap (requires deliberate firm tapping)
- Same timing: 100ms latency between taps, 300ms window for second tap
- The 3-minute automatic backpack mode detection remains alongside this manual trigger

## Register Note: ADXL343 Address Bug (Separate Fix)

During investigation, I found two incorrect register addresses in `configureADXL343Interrupt()`:

| Code | Address Used | Actual Register | Correct Address |
|------|-------------|----------------|-----------------|
| `THRESH_ACT` | 0x1C | Reserved | 0x24 |
| `TIME_ACT` | 0x22 | Latent (tap) | N/A (doesn't exist) |

**For this change:** We only fix the LATENT register issue (replacing the incorrect `TIME_ACT` write with the correct LATENT value and adding WINDOW). The THRESH_ACT address bug (0x1C → 0x24) will be deferred to a separate PR to keep behavioral changes isolated and independently testable.

## Files to Modify

### 1. [gestures.h](firmware/include/gestures.h)
- Add `GESTURE_DOUBLE_TAP` to the `GestureType` enum (after `GESTURE_SHAKE_WHILE_INVERTED`)

### 2. [main.cpp](firmware/src/main.cpp) - `configureADXL343Interrupt()` (line 224)
- Replace `TIME_ACT` write at 0x22 with proper LATENT = `TAP_WAKE_LATENT` (was accidentally writing to LATENT as "TIME_ACT" anyway)
- Add WINDOW (0x23) = `TAP_WAKE_WINDOW` configuration
- Change `INT_ENABLE` from 0x50 (activity + single-tap) to 0x70 (+ double-tap)
- Clear pending interrupts by reading INT_SOURCE at end
- Leave THRESH_ACT at 0x1C as-is (deferred to separate PR)

### 3. [main.cpp](firmware/src/main.cpp) - After `gesturesUpdate()` call (line 1258-1261)
- Read INT_SOURCE register (0x30) after each `gesturesUpdate()` call
- Check bit 5 (double-tap flag); if set, override gesture to `GESTURE_DOUBLE_TAP`
- Reading INT_SOURCE clears flags - safe during awake mode since the INT pin is only used for sleep wake

### 4. [main.cpp](firmware/src/main.cpp) - After shake-while-inverted handler (line 1285)
- Add `GESTURE_DOUBLE_TAP` handler:
  - Same guards as automatic entry: not during calibration, not during BLE calibration
  - Log `"=== DOUBLE-TAP → ENTERING BACKPACK MODE ==="`
  - Set `g_in_extended_sleep_mode = true`
  - Call `enterExtendedDeepSleep()` (reuses existing function - shows backpack screen, saves state, configures tap wake, sleeps)

### 5. [main.cpp](firmware/src/main.cpp) - Debug gesture switch-case (~line 1804)
- Add `case GESTURE_DOUBLE_TAP: Serial.print("DOUBLE_TAP"); break;`

## Guards Against False Positives

- **3.0g threshold**: Normal handling is 0.5-1.0g, accidental bumps are 1-2g. Double-tap at 3.0g requires deliberate firm tapping
- **Double-tap timing**: Must tap, wait 100ms, then tap again within 300ms. Random vibrations won't produce this pattern
- **Calibration guard**: Blocked during standalone or BLE calibration (same as auto-trigger)
- **Recovery**: If accidentally triggered, user double-taps again to wake - same gesture, symmetric behavior

## IRAM Impact

Minimal (~200 bytes): one enum value, one register read per loop, one conditional branch. Well within the 6KB headroom in IOS_MODE=1.

## Verification

1. Build: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. Test double-tap to sleep: With bottle awake on table, double-tap firmly → should see "DOUBLE-TAP DETECTED" and "ENTERING BACKPACK MODE" in serial, backpack screen displays
3. Test double-tap to wake: After entering backpack mode via double-tap, double-tap again → should wake normally
4. Test calibration guard: Start calibration (inverted hold), then double-tap → should be ignored
5. Test auto-trigger still works: Leave bottle without stable placement for 3+ minutes → auto backpack mode still functions
6. Test normal sleep wake: Verify normal sleep still wakes on motion/tap (no regression from LATENT/WINDOW register changes)
