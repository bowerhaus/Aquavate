# Plan: iOS Calibration Flow (Issue #30)

## Overview

Add the ability to perform two-point calibration from the iOS app with guided screens and real-time device feedback.

## Current State Analysis

### Command Code Mismatch (Bug)
- **iOS** (`BLEConstants.swift:99`): `startCalibration = 0x02`
- **Firmware** (`ble_service.h:98`): `BLE_CMD_PING = 0x02`
- Firmware reserves `0x03-0x04` for future calibration but never implemented them
- **Must fix this mismatch** before implementing calibration

### IRAM Constraints
- Current usage: ~125KB / 131KB (95.3%) with BLE enabled
- Standalone calibration state machine is disabled in IOS_MODE=1 to save ~2.4KB
- Cannot simply enable full calibration state machine without exceeding limits

### Key Insight
The firmware's core calibration functions are **always compiled**:
- `calibrationCalculateScaleFactor()` - Calculate scale from two ADC readings
- `calibrationGetWaterWeight()` - Convert ADC to weight
- `storageSaveCalibration()` - Persist calibration data

---

## Architecture Options

### Option A: iOS-Driven Calibration (Recommended)

iOS app controls the entire calibration flow. Firmware acts as a measurement service.

**Flow:**
1. iOS sends `CAL_MEASURE_POINT` command (0x03) with point type (empty/full)
2. Firmware takes stable measurement using existing `weightMeasureStable()`
3. Firmware returns raw ADC value via Current State notification
4. iOS stores both ADC readings, calculates scale factor
5. iOS sends `CAL_SET_DATA` command (0x04) with calculated calibration
6. Firmware saves to NVS

**Pros:**
- Minimal firmware changes (~400-500 bytes IRAM)
- Stays within IRAM budget
- Full UI flexibility on iOS side
- Leverages existing firmware measurement code

**Cons:**
- More complex iOS implementation
- Requires parsing raw ADC from notifications

### Option B: Device-Assisted Calibration

Re-enable simplified calibration state machine in firmware for BLE mode.

**Pros:**
- Simpler iOS implementation (just mirrors states)
- More robust measurement handling on device

**Cons:**
- Significant IRAM increase (~2KB+)
- May exceed IRAM budget
- Requires careful conditional compilation

---

## Recommended Implementation (Option A)

### Phase 1: Firmware Changes

**Files to modify:**
- [firmware/include/ble_service.h](firmware/include/ble_service.h)
- [firmware/src/ble_service.cpp](firmware/src/ble_service.cpp)

#### 1.1 Add Command Definitions

```cpp
// In ble_service.h (replace reserved comment)
#define BLE_CMD_CAL_MEASURE_POINT   0x03  // Take stable ADC measurement (param1: 0=empty, 1=full)
#define BLE_CMD_CAL_SET_DATA        0x04  // Save calibration (13-byte extended command)
```

#### 1.2 Extend Current State Flags

```cpp
// Existing flags (bits 0-2)
// Bit 0: time_valid
// Bit 1: calibrated
// Bit 2: stable

// New flags (bits 3-4)
// Bit 3: cal_measuring (measurement in progress)
// Bit 4: cal_result_ready (raw ADC available in current_weight_g)
```

#### 1.3 Add Command Handlers

```cpp
// In ble_service.cpp - handle CAL_MEASURE_POINT
case BLE_CMD_CAL_MEASURE_POINT: {
    uint8_t pointType = cmd.param1;  // 0=empty, 1=full
    g_cal_measuring = true;
    bleNotifyCalibrationState();  // Notify measuring started

    // Use existing stable measurement (blocks ~5 seconds)
    MeasurementResult result = weightMeasureStable();

    g_cal_measuring = false;
    g_cal_last_adc = result.adc_value;
    g_cal_result_ready = true;
    bleNotifyCalibrationResult();  // Notify with ADC value
    break;
}

// Handle CAL_SET_DATA (13-byte extended command)
case BLE_CMD_CAL_SET_DATA: {
    // Parse: empty_adc (4), full_adc (4), scale_factor (4)
    // Create CalibrationData and call storageSaveCalibration()
    break;
}
```

#### 1.4 IRAM Impact Estimate

| Addition | Est. Bytes |
|----------|-----------|
| Command handlers | ~300 |
| Static variables | ~50 |
| Notification helpers | ~100 |
| **Total** | **~450 bytes** |

Resulting usage: ~125.4KB / 131KB (95.7%) - **within budget**

---

### Phase 2: iOS BLE Updates

**Files to modify:**
- [ios/Aquavate/Aquavate/Services/BLEConstants.swift](ios/Aquavate/Aquavate/Services/BLEConstants.swift)
- [ios/Aquavate/Aquavate/Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)

#### 2.1 Fix Command Code Mismatch

```swift
enum Command: UInt8 {
    case tareNow = 0x01
    case ping = 0x02                  // Keep-alive (fix: was incorrectly startCalibration)
    case calMeasurePoint = 0x03       // Request stable measurement
    case calSetData = 0x04            // Write calibration to NVS
    case resetDaily = 0x05
    // ... rest unchanged
}

// Remove old startCalibration, calibratePoint, cancelCalibration
```

#### 2.2 Add Calibration State Flags

```swift
struct StateFlags: OptionSet {
    static let timeValid = StateFlags(rawValue: 0x01)
    static let calibrated = StateFlags(rawValue: 0x02)
    static let stable = StateFlags(rawValue: 0x04)
    static let calMeasuring = StateFlags(rawValue: 0x08)      // New
    static let calResultReady = StateFlags(rawValue: 0x10)    // New
}
```

#### 2.3 Add BLEManager Methods

```swift
extension BLEManager {
    func sendCalMeasureCommand(pointType: CalibrationPointType) {
        let command = BLECommand(command: 0x03, param1: pointType.rawValue, param2: 0)
        writeCommand(command)
    }

    func sendCalSetDataCommand(emptyADC: Int32, fullADC: Int32, scaleFactor: Float) {
        var data = Data(count: 13)
        data[0] = 0x04
        // Pack emptyADC, fullADC, scaleFactor as little-endian
        writeRawData(data, to: BLEConstants.commandUUID)
    }
}
```

---

### Phase 3: iOS Calibration UI

**New files:**
- `ios/Aquavate/Aquavate/Views/CalibrationView.swift`
- `ios/Aquavate/Aquavate/Views/Components/BottleGraphicView.swift`
- `ios/Aquavate/Aquavate/Services/CalibrationManager.swift`

#### 3.1 CalibrationState Enum

```swift
enum CalibrationState: Equatable {
    case notStarted
    case emptyBottlePrompt       // Step 1: "Place empty bottle on sensor"
    case measuringEmpty          // Step 1: Waiting for firmware measurement
    case emptyMeasured(adc: Int32)  // Step 1 complete
    case fullBottlePrompt        // Step 2: "Fill bottle to 830ml"
    case measuringFull           // Step 2: Waiting for firmware measurement
    case fullMeasured(adc: Int32)   // Step 2 complete
    case savingCalibration       // Step 3: Writing to device
    case complete(scaleFactor: Float)
    case failed(CalibrationError)
}

enum CalibrationError: Error, Equatable {
    case notConnected
    case measurementTimeout
    case measurementUnstable
    case invalidMeasurement(String)  // e.g., "Full reading must be greater than empty"
    case saveFailed
    case disconnected
}
```

#### 3.2 CalibrationManager

```swift
@MainActor
class CalibrationManager: ObservableObject {
    @Published private(set) var state: CalibrationState = .notStarted
    @Published private(set) var isStable: Bool = false
    @Published private(set) var currentWeightG: Int = 0
    @Published private(set) var measurementProgress: Double = 0  // 0.0-1.0 during measurement

    private weak var bleManager: BLEManager?
    private var emptyADC: Int32?
    private var fullADC: Int32?
    private var measurementTimer: Timer?
    private var timeoutTask: Task<Void, Never>?

    // Calibration known value
    static let calibrationVolumeMl: Float = 830.0

    func startCalibration()
    func confirmEmptyBottle()      // User confirms, sends CAL_MEASURE_POINT(empty)
    func confirmFullBottle()       // User confirms, sends CAL_MEASURE_POINT(full)
    func handleCurrentStateUpdate(flags: UInt8, weightOrADC: Int16)
    func cancel()
    func retry()

    private func calculateScaleFactor() -> Float {
        // scale = (full_adc - empty_adc) / 830g
    }

    private func saveCalibration()  // Sends CAL_SET_DATA command
}
```

#### 3.3 Step Progress Indicator

A horizontal progress bar showing the 3 calibration steps:

```
┌─────────────────────────────────────────────────┐
│  ●━━━━━━━○━━━━━━━○                              │
│  Empty    Full    Save                          │
└─────────────────────────────────────────────────┘
```

**States:**
- Empty circle (○) = not started
- Filled circle (●) = current or completed
- Line between = progress connection

```swift
struct CalibrationProgressView: View {
    let currentStep: Int  // 1, 2, or 3

    var body: some View {
        HStack(spacing: 0) {
            StepIndicator(step: 1, label: "Empty", current: currentStep)
            ProgressLine(completed: currentStep > 1)
            StepIndicator(step: 2, label: "Full", current: currentStep)
            ProgressLine(completed: currentStep > 2)
            StepIndicator(step: 3, label: "Save", current: currentStep)
        }
        .padding(.horizontal)
    }
}
```

#### 3.4 BottleGraphicView (Faithful to Firmware)

Replicate the e-paper bottle graphic from `display.cpp:drawBottleGraphic()`:

**Firmware Dimensions (40x90px):**
- Cap: 12px wide × 10px tall (centered at top)
- Neck: 16px wide × 10px tall (below cap)
- Body: 40px wide × 70px tall (rounded rect, 8px corner radius)
- Water fill: Inside body, height = 70px × fill_percent

**SwiftUI Implementation:**

```swift
struct BottleGraphicView: View {
    let fillPercent: Double      // 0.0 to 1.0
    let showQuestionMark: Bool   // Show "?" overlay
    let size: CGSize             // Target size (scales proportionally)

    // Proportions from firmware (40x90 base)
    private let aspectRatio: CGFloat = 40.0 / 90.0

    var body: some View {
        GeometryReader { geo in
            let scale = min(geo.size.width / 40, geo.size.height / 90)

            ZStack {
                // Bottle shape (cap + neck + body)
                BottleShape()
                    .stroke(Color.primary, lineWidth: 2 * scale)

                // Body fill (white interior)
                BottleBodyShape()
                    .fill(Color.white)
                    .padding(2 * scale)

                // Water fill (from bottom)
                BottleBodyShape()
                    .fill(Color.blue.opacity(0.7))
                    .mask(
                        VStack {
                            Spacer()
                            Rectangle()
                                .frame(height: 70 * scale * fillPercent)
                        }
                    )
                    .padding(4 * scale)

                // Question mark overlay
                if showQuestionMark {
                    Text("?")
                        .font(.system(size: 36 * scale, weight: .bold))
                        .foregroundColor(fillPercent > 0.5 ? .white : .orange)
                        .offset(y: 15 * scale)  // Center in body area
                }
            }
            .frame(width: 40 * scale, height: 90 * scale)
            .position(x: geo.size.width / 2, y: geo.size.height / 2)
        }
    }
}

// Custom Shape matching firmware bottle outline
struct BottleShape: Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let scale = min(rect.width / 40, rect.height / 90)

        // Cap (centered, 12px wide, 10px tall)
        let capWidth: CGFloat = 12 * scale
        let capHeight: CGFloat = 10 * scale
        let capX = (rect.width - capWidth) / 2
        path.addRect(CGRect(x: capX, y: 0, width: capWidth, height: capHeight))

        // Neck (16px wide, 10px tall)
        let neckWidth: CGFloat = 16 * scale
        let neckHeight: CGFloat = 10 * scale
        let neckX = (rect.width - neckWidth) / 2
        path.addRect(CGRect(x: neckX, y: capHeight, width: neckWidth, height: neckHeight))

        // Body (40px wide, 70px tall, 8px corner radius)
        let bodyY = capHeight + neckHeight
        let bodyHeight: CGFloat = 70 * scale
        let cornerRadius: CGFloat = 8 * scale
        path.addRoundedRect(
            in: CGRect(x: 0, y: bodyY, width: rect.width, height: bodyHeight),
            cornerSize: CGSize(width: cornerRadius, height: cornerRadius)
        )

        return path
    }
}
```

#### 3.5 Calibration Screen Designs

Each screen uses the common layout:
- **Top:** Step progress indicator
- **Center:** Bottle graphic + instructional content
- **Bottom:** Action button(s)

---

**Screen 1: Welcome / Not Started**

```
┌─────────────────────────────────────────────────┐
│              Calibrate Your Bottle              │
│                                                 │
│          ┌──────────────────────┐               │
│          │    [Bottle Icon]     │               │
│          │      (outline)       │               │
│          └──────────────────────┘               │
│                                                 │
│  Two-point calibration ensures accurate         │
│  drink tracking by measuring your empty         │
│  and full bottle weights.                       │
│                                                 │
│  You'll need:                                   │
│  • Your empty bottle                            │
│  • 830ml of water to fill it                    │
│                                                 │
│           ┌─────────────────────┐               │
│           │  Start Calibration  │               │
│           └─────────────────────┘               │
└─────────────────────────────────────────────────┘
```

---

**Screen 2: Empty Bottle Prompt (Step 1)**

```
┌─────────────────────────────────────────────────┐
│     ●━━━━━━━━○━━━━━━━━○                         │
│   Empty     Full     Save                       │
│─────────────────────────────────────────────────│
│                                                 │
│         ┌──────────────────────┐                │
│         │                      │                │
│         │    [Empty Bottle]    │                │
│         │         ?            │  ← 0% fill     │
│         │                      │    with "?"    │
│         └──────────────────────┘                │
│                                                 │
│           Step 1: Empty Bottle                  │
│                                                 │
│  1. Make sure your bottle is completely empty   │
│  2. Place it upright on the sensor puck         │
│  3. Keep it still until measurement completes   │
│                                                 │
│  ┌──────────────────────────────────────────┐   │
│  │ Weight: 245g    Stability: ● Stable      │   │
│  └──────────────────────────────────────────┘   │
│                                                 │
│           ┌─────────────────────┐               │
│           │   Measure Empty     │               │
│           └─────────────────────┘               │
│              (enabled when stable)              │
└─────────────────────────────────────────────────┘
```

---

**Screen 3: Measuring Empty (Step 1 - In Progress)**

```
┌─────────────────────────────────────────────────┐
│     ●━━━━━━━━○━━━━━━━━○                         │
│   Empty     Full     Save                       │
│─────────────────────────────────────────────────│
│                                                 │
│         ┌──────────────────────┐                │
│         │                      │                │
│         │    [Empty Bottle]    │                │
│         │                      │                │
│         │                      │                │
│         └──────────────────────┘                │
│                                                 │
│              Measuring...                       │
│                                                 │
│         ████████████░░░░░░░░░░  60%            │
│                                                 │
│           Keep the bottle still                 │
│       This takes about 5 seconds                │
│                                                 │
└─────────────────────────────────────────────────┘
```

---

**Screen 4: Full Bottle Prompt (Step 2)**

```
┌─────────────────────────────────────────────────┐
│     ●━━━━━━━━●━━━━━━━━○                         │
│   Empty     Full     Save       ✓ Empty done    │
│─────────────────────────────────────────────────│
│                                                 │
│         ┌──────────────────────┐                │
│         │   ████████████████   │                │
│         │   ████████████████   │                │
│         │   ███[Full Bottle]██ │  ← 100% fill   │
│         │   █████████?████████ │    with "?"    │
│         │   ████████████████   │                │
│         └──────────────────────┘                │
│                                                 │
│            Step 2: Full Bottle                  │
│                                                 │
│  1. Fill your bottle with exactly 830ml         │
│  2. Place it upright on the sensor puck         │
│  3. Keep it still until measurement completes   │
│                                                 │
│  ┌──────────────────────────────────────────┐   │
│  │ Weight: 1075g    Stability: ● Stable     │   │
│  └──────────────────────────────────────────┘   │
│                                                 │
│           ┌─────────────────────┐               │
│           │    Measure Full     │               │
│           └─────────────────────┘               │
└─────────────────────────────────────────────────┘
```

---

**Screen 5: Calibration Complete**

```
┌─────────────────────────────────────────────────┐
│     ●━━━━━━━━●━━━━━━━━●                         │
│   Empty     Full     Save       All complete!   │
│─────────────────────────────────────────────────│
│                                                 │
│                    ✓                            │
│              (large checkmark)                  │
│                                                 │
│          Calibration Complete                   │
│                                                 │
│  Your bottle is now calibrated and ready        │
│  for accurate drink tracking.                   │
│                                                 │
│  ┌──────────────────────────────────────────┐   │
│  │  Empty weight:     245g                  │   │
│  │  Full weight:      1075g                 │   │
│  │  Water capacity:   830ml                 │   │
│  │  Scale factor:     450.2 ADC/g           │   │
│  └──────────────────────────────────────────┘   │
│                                                 │
│           ┌─────────────────────┐               │
│           │        Done         │               │
│           └─────────────────────┘               │
└─────────────────────────────────────────────────┘
```

---

**Screen 6: Error State**

```
┌─────────────────────────────────────────────────┐
│     ●━━━━━━━━○━━━━━━━━○                         │
│   Empty     Full     Save                       │
│─────────────────────────────────────────────────│
│                                                 │
│                    ✕                            │
│              (large X icon)                     │
│                                                 │
│           Calibration Failed                    │
│                                                 │
│  The measurement timed out. Please ensure       │
│  the bottle is placed firmly on the sensor      │
│  and remains still during measurement.          │
│                                                 │
│     ┌───────────────┐  ┌───────────────┐        │
│     │     Retry     │  │    Cancel     │        │
│     └───────────────┘  └───────────────┘        │
└─────────────────────────────────────────────────┘
```

#### 3.6 Real-Time Feedback Components

**StabilityIndicator:** Shows current weight stability from BLE

```swift
struct StabilityIndicator: View {
    let isStable: Bool
    let currentWeightG: Int

    var body: some View {
        HStack {
            Text("Weight: \(currentWeightG)g")
            Spacer()
            HStack(spacing: 4) {
                Circle()
                    .fill(isStable ? Color.green : Color.orange)
                    .frame(width: 8, height: 8)
                Text(isStable ? "Stable" : "Stabilizing...")
                    .foregroundColor(isStable ? .green : .orange)
            }
        }
        .font(.subheadline)
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(8)
    }
}
```

**MeasurementProgressView:** Animated progress during 5-second measurement

```swift
struct MeasurementProgressView: View {
    let progress: Double  // 0.0 to 1.0

    var body: some View {
        VStack(spacing: 12) {
            ProgressView(value: progress)
                .progressViewStyle(.linear)
                .tint(.blue)

            Text("Keep the bottle still")
                .font(.subheadline)
                .foregroundColor(.secondary)
        }
    }
}
```

---

### Phase 4: Integration

**File to modify:**
- [ios/Aquavate/Aquavate/Views/SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift)

Add calibration entry point in "Device Commands" section:

```swift
Section("Device Commands") {
    // Existing buttons...

    NavigationLink {
        CalibrationView()
    } label: {
        HStack {
            Image(systemName: bleManager.isCalibrated ? "checkmark.seal.fill" : "seal")
                .foregroundStyle(bleManager.isCalibrated ? .green : .orange)
            Text("Calibrate Bottle")
            Spacer()
            if !bleManager.isCalibrated {
                Text("Required")
                    .font(.caption)
                    .foregroundStyle(.orange)
            }
        }
    }
    .disabled(!bleManager.connectionState.isConnected)
}
```

---

## Verification Plan

### Firmware Testing
1. Build with `IOS_MODE=1` - verify IRAM usage stays under 131KB
2. Use serial monitor to test CAL_MEASURE_POINT command
3. Verify stable measurement returns valid ADC
4. Test CAL_SET_DATA saves correctly to NVS

### iOS Testing
1. Test command code fix (ping should work, not trigger calibration)
2. Test measurement command sends correctly
3. Test notification parsing extracts ADC value
4. Test full calibration flow end-to-end
5. Test error scenarios (timeout, disconnect, invalid data)

### End-to-End Testing
1. Connect iOS app to device
2. Navigate to Settings > Calibrate Bottle
3. Follow guided flow with actual empty/full bottle
4. Verify calibration saved and `isCalibrated` flag updates
5. Verify drink tracking uses new calibration

---

## Critical Files Summary

| File | Changes |
|------|---------|
| `firmware/include/ble_service.h` | Add command definitions 0x03, 0x04; extend flags |
| `firmware/src/ble_service.cpp` | Add command handlers for measurement and save |
| `ios/.../BLEConstants.swift` | Fix command mismatch; add new commands and flags |
| `ios/.../BLEManager.swift` | Add calibration command methods |
| `ios/.../CalibrationManager.swift` | New: state machine and BLE coordination |
| `ios/.../CalibrationView.swift` | New: guided calibration UI |
| `ios/.../BottleGraphicView.swift` | New: bottle graphic component |
| `ios/.../SettingsView.swift` | Add calibration navigation link |

---

## Open Questions

None - ready for implementation.
