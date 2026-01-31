# Plan: Fix ADXL343 Register Addresses (Issue #98)

**Status:** ✅ COMPLETE

## Summary

Fix incorrect ADXL343 register address for THRESH_ACT in `configureADXL343Interrupt()` and the diagnostic readback function. Also separate activity wake threshold from tap threshold — the previous 3.0g was too high for tilt-to-pour detection.

## Changes

### 1. [config.h](firmware/src/config.h) — New `ACTIVITY_WAKE_THRESHOLD` constant

- Added `ACTIVITY_WAKE_THRESHOLD 0x08` (0.5g) — separate from `TAP_WAKE_THRESHOLD` (3.0g)
- Activity detection needs a low threshold to catch tilt/pour motions (~0.7g change at 45°)
- Tap detection remains at 3.0g for sharp impact detection

### 2. [main.cpp](firmware/src/main.cpp) — `configureADXL343Interrupt()`

- Changed `THRESH_ACT` from `0x1C` (reserved register) to `0x24` (correct address per datasheet)
- Changed hardcoded `0x30` (3.0g) to `ACTIVITY_WAKE_THRESHOLD` (0.5g)
- Updated comments and serial output to use the constant

### 3. [main.cpp](firmware/src/main.cpp) — Diagnostic readback

- Changed `readAccelReg(0x1C)` to `readAccelReg(0x24)`

### 4. [PRD.md](docs/PRD.md) — Updated motion wake description

- Corrected threshold from ">3.0g threshold, sustained ~1.6s" to ">0.5g threshold, AC-coupled"

### 5. [AGENTS.md](AGENTS.md) — Updated wake-on-tilt threshold reference

## Behavioural Change

- THRESH_ACT register address corrected: writes now reach the actual hardware register
- Activity wake threshold: 0.5g (previously effectively 0x00 due to wrong register, plan originally targeted 3.0g but this was too high for tilt detection)
- Single-tap wake at 3.0g: unaffected

## Verification

1. Build: `cd firmware && ~/.platformio/penv/bin/platformio run` — ✅ SUCCESS
2. Hardware: verify normal bottle tilt-to-pour wakes from normal sleep
3. Hardware: verify the diagnostic readback shows correct THRESH_ACT value (0x08 = 0.5g)
