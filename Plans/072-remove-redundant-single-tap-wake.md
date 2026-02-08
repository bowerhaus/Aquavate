# Remove Redundant Single-Tap Wake Interrupt

## Context

When the bottle wakes from normal deep sleep via a single tap, the serial log shows it as an activity wake ("Woke up from EXT0 (tilt/motion interrupt!)"). This is because the activity threshold (1.5g) is lower than the single-tap threshold (3.0g), so any tap that triggers single-tap also triggers activity. Since both interrupts share the same INT1 pin and the ESP32 can't distinguish them, single-tap wake adds no value during normal deep sleep.

Double-tap remains essential: it's polled via INT_SOURCE while awake (manual backpack mode entry) and is the sole wake source in extended/backpack sleep.

## Changes

### 1. Remove single-tap from normal sleep interrupt config
**File:** [main.cpp](firmware/src/main.cpp)

In `configureADXL343Interrupt()` (~line 298):
- Change `INT_ENABLE` from `0x70` (activity + single-tap + double-tap) to `0x30` (activity + double-tap)
- Update the comments at lines 293-298 and debug messages at lines 299, 310
- Remove or update the "Step 5" comment block (lines 274-278) that describes single-tap as an "additional wake method" - the tap threshold/duration config still applies to double-tap so the register writes stay, just update the comments
- Update the summary log at line 314

### 2. Update config.h comment
**File:** [config.h](firmware/src/config.h)

- Line 147: Change comment from "shared by single-tap wake + double-tap backpack mode" to just "double-tap detection (backpack mode)"

## What stays unchanged
- All tap threshold/duration/window/latency register writes (needed for double-tap detection)
- `configureADXL343TapWake()` (backpack mode - only enables double-tap, already correct)
- Double-tap polling in gesture detection loop (INT_SOURCE bit 5)
- All extended sleep / backpack mode logic

## Verification
- Build firmware: `cd firmware && ~/.platformio/penv/bin/platformio run`
- Upload and test: single tap still wakes bottle (via activity interrupt); double-tap while awake still enters backpack mode; double-tap wakes from extended sleep
