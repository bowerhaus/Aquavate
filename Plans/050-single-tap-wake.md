# Plan: Add Single-Tap Wake to Normal Sleep Mode

**Issue**: #63

## Summary
Add single-tap detection as an additional wake source for normal deep sleep mode, alongside the existing sustained motion detection. Also fix documentation that incorrectly references LIS3DH accelerometer (should be ADXL343).

## Current Behavior
- **Normal sleep**: Wakes on sustained motion (>3.0g for >1.6 seconds) - requires deliberate shake
- **Extended sleep (backpack)**: Wakes on double-tap only

## Proposed Behavior
- **Normal sleep**: Wakes on single-tap OR sustained motion (either triggers wake)
- **Extended sleep (backpack)**: Unchanged (double-tap only)

## Implementation

### 1. Modify `configureADXL343Interrupt()` in [main.cpp](firmware/src/main.cpp)

Add tap detection configuration alongside existing activity detection:

```cpp
// Add tap registers (after existing activity config)
const uint8_t THRESH_TAP = 0x1D;
const uint8_t DUR = 0x21;
const uint8_t TAP_AXES = 0x2A;

// Configure tap threshold (same as double-tap: 3.0g)
writeAccelReg(THRESH_TAP, TAP_WAKE_THRESHOLD);  // 0x30 = 3.0g

// Configure tap duration (10ms max)
writeAccelReg(DUR, TAP_WAKE_DURATION);  // 0x10 = 10ms

// Enable all axes for tap detection
writeAccelReg(TAP_AXES, 0x07);  // X, Y, Z

// Change INT_ENABLE from 0x10 to 0x50
// Bit 4 = Activity (0x10)
// Bit 6 = Single-tap (0x40)
// Combined = 0x50
writeAccelReg(INT_ENABLE, 0x50);
```

**Key change**: Line 267 changes from `0x10` (activity only) to `0x50` (activity + single-tap).

### 2. Update Serial Output Messages

Update the configuration complete message to reflect both wake methods:
```
"Wake condition: Single-tap OR sustained motion (>3.0g)"
```

### 3. Documentation Updates (LIS3DH → ADXL343)

Files requiring updates (19 files found):

**Critical files:**
- [CLAUDE.md](CLAUDE.md) - Lines mentioning "LIS3DH accelerometer"
- [AGENTS.md](AGENTS.md)
- [README.md](README.md)
- [docs/PRD.md](docs/PRD.md)

**Pin config comments:**
- [firmware/src/config/pins_adafruit.h](firmware/src/config/pins_adafruit.h)
- [firmware/src/config/pins_sparkfun.h](firmware/src/config/pins_sparkfun.h) - Line 30 says "LIS3DH"

**Plan documents:**
- [Plans/001-hardware-research.md](Plans/001-hardware-research.md)
- [Plans/002-bom-adafruit-feather.md](Plans/002-bom-adafruit-feather.md)
- [Plans/003-bom-sparkfun-qwiic.md](Plans/003-bom-sparkfun-qwiic.md)
- [Plans/004-sensor-puck-design.md](Plans/004-sensor-puck-design.md)
- [Plans/005-standalone-calibration-mode.md](Plans/005-standalone-calibration-mode.md)
- [Plans/007-daily-intake-tracking.md](Plans/007-daily-intake-tracking.md)
- [Plans/010-deep-sleep-reinstatement.md](Plans/010-deep-sleep-reinstatement.md)
- [Plans/011-extended-deep-sleep-backpack-mode.md](Plans/011-extended-deep-sleep-backpack-mode.md)
- [Plans/022-ds3231-rtc-integration.md](Plans/022-ds3231-rtc-integration.md)
- [Plans/023-adxl343-accelerometer-migration.md](Plans/023-adxl343-accelerometer-migration.md)
- [docs/mechanical/component-dimensions.md](docs/mechanical/component-dimensions.md)
- [docs/STATE_MACHINE.md](docs/STATE_MACHINE.md)

## Files to Modify

| File | Change |
|------|--------|
| [firmware/src/main.cpp](firmware/src/main.cpp) | Add single-tap config to `configureADXL343Interrupt()` |
| 19 documentation files | Replace "LIS3DH" with "ADXL343" |

## Verification

1. Build firmware: `cd firmware && ~/.platformio/penv/bin/platformio run`
2. User uploads and tests:
   - Place bottle upright, let it enter normal sleep
   - Single tap should wake it
   - Sustained shake should also wake it (existing behavior)
   - Backpack mode should still require double-tap (unchanged)
