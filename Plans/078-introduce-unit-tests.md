# Testing Strategy for Aquavate

**GitHub Issue:** [#122](https://github.com/bowerhaus/Aquavate/issues/122)
**Branch:** `introduce-unit-tests`

## Context

Neither the firmware nor the iOS app has any tests. The user wants minimum viable, high-value tests — not comprehensive coverage. The goal is to cover logic that is complex enough to be bug-prone and hard to catch manually.

Issue #120 ("timezone daily reset firing at UTC midnight") is a concrete example of exactly the kind of bug testing should catch. The drink detection, reset timing, and pace calculations are the highest-risk areas.

---

## Firmware Testing

### Current State
- `firmware/test/` directory exists (`.gitkeep` only)
- `platformio.ini` already has `test_dir = test` configured
- No test framework added yet
- PlatformIO natively supports both on-device (Unity) and native/host tests

### Approach: PlatformIO Native Tests with Unity

Run tests on the development Mac without any hardware. PlatformIO supports a `[env:native]` environment that compiles and runs C++ on the host.

**Pros:**
- Zero hardware required — runs in seconds locally
- PlatformIO already configured for it (`test_dir = test`)
- Unity (the embedded test framework) is trivial to add to `lib_deps`
- Pure logic modules have no hardware deps — ideal fit
- Mocking `getCurrentUnixTime()` to test timezone edge cases is trivial

**Cons:**
- Requires stubbing out ESP32-specific includes (`Arduino.h`, NVS, `RTC_DATA_ATTR`)
- Need a `[env:native]` build profile in `platformio.ini`
- Won't catch hardware integration issues (those need the device anyway)

### What to Test

1. **`drinks.cpp` — timezone reset and drink detection** (highest priority)
   - `getTodayResetTimestamp()` with injected timestamps across timezones
   - `recalculateDailyTotals()` with records spanning the 4am boundary
   - `drinksUpdate()` for gulp/pour/refill/drift classification
   - This module just had a real bug (Issue #120) — tests here would have caught it

2. **`weight.cpp` — outlier removal**
   - `removeOutliers()` with known sample arrays
   - No hardware dependency, pure math
   - Genuinely hard to reason about with outliers at boundaries

3. **`calibration.cpp` — scale factor math**
   - `calibrationCalculateScaleFactor()` and `calibrationGetWaterWeight()`
   - Simple linear math but accuracy is critical (±15ml target)

### What to Skip

- `gestures.cpp` — timing-dependent, needs `millis()` mock, lower risk
- Display, BLE, serial commands, deep sleep, RTC — hardware-only, no unit test value
- `main.cpp` — glue code, no testable units

---

## iOS App Testing

### Current State
- Zero XCTest targets or test files
- `ENABLE_TESTABILITY = YES` already set in the project — ready to go
- No testing frameworks installed

### Approach: XCTest Unit Tests

Apple's built-in test framework. No dependencies to add, first-class Xcode support, runs in simulator.

### What to Test

1. **`HydrationReminderService` — pace and deficit calculations** (highest priority)
   - Pure Swift math: daily goal ÷ 15-hour window = hourly pace
   - `calculateDeficit()` — expected intake by now vs actual
   - `calculateUrgency()` → Blue/Amber/Red status thresholds
   - `canSendReminder()` — throttle logic (max 12/day)
   - Zero CoreBluetooth or HealthKit dependency — no mocking needed
   - Runs fully in the simulator

2. **`HydrationState` computed properties** (stretch)
   - `progress`, `isGoalAchieved` — simple but core to UI correctness

### What to Skip

- `BLEManager` — 2,183 lines, deeply coupled to CoreBluetooth, enormous mocking effort for marginal gain
- `CalibrationManager` — depends on BLEManager protocol
- UI tests — brittle, slow, low ROI for a hardware-dependent app
- HealthKit — requires special simulator entitlements

---

## Files to Modify

**Firmware:**
- `firmware/platformio.ini` — add `[env:native]` + Unity dep
- `firmware/test/` — create test files here
- `firmware/src/drinks.cpp` — may need `getCurrentUnixTime()` injectable
- `firmware/src/weight.cpp` — pure functions, likely no changes needed
- `firmware/src/calibration.cpp` — pure functions, likely no changes needed

**iOS:**
- `ios/Aquavate/Aquavate.xcodeproj/` — add XCTest target via Xcode
- New: `ios/Aquavate/AquavateTests/HydrationReminderServiceTests.swift`
- `ios/Aquavate/Aquavate/Services/HydrationReminderService.swift` — may need injectable clock for time-based tests

**Documentation:**
- `CLAUDE.md` — add Firmware Testing and iOS Testing sections with run commands and guidance
- `AGENTS.md` — add pre-PR testing step to the development workflow

---

## Documentation to Add

### CLAUDE.md — Firmware Testing section (under Firmware Build Commands)

```bash
# Run firmware unit tests (no hardware required)
cd firmware
~/.platformio/penv/bin/platformio test -e native
```

- Tests cover pure logic modules only — hardware-dependent code (display, BLE, serial, deep sleep, RTC) is tested manually on device
- When to run: check `firmware/test/` to see what's covered; run tests if your changes touch anything under test

### CLAUDE.md — iOS Testing section (under iOS Build Commands)

```bash
# Run iOS unit tests (requires simulator)
cd ios/Aquavate
xcodebuild test -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17'
```

- Tests cover pure business logic only — BLE/CoreBluetooth, HealthKit, and UI are tested manually
- When to run: check `AquavateTests/` to see what's covered; run tests if your changes touch anything under test

### AGENTS.md — pre-PR checklist addition

- Before opening a firmware PR, check `firmware/test/` — if your changes touch anything covered there, run `platformio test -e native`
- Before opening an iOS PR, check `AquavateTests/` — if your changes touch anything covered there, run the test suite (`Cmd+U` or `xcodebuild test`)

---

## Verification

- Firmware: `cd firmware && ~/.platformio/penv/bin/platformio test -e native` → all pass
- iOS: `Cmd+U` in Xcode with iPhone 17 simulator → all pass
