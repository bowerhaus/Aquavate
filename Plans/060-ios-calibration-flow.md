# Plan: iOS Calibration Flow (Issue #30) - COMPLETE

**Status:** ✅ COMPLETE (2026-01-27)

## Overview

Add iOS-triggered calibration that uses the bottle's existing standalone calibration state machine. The iOS app sends start/cancel commands and mirrors the bottle's state, while the bottle handles all measurement logic and displays status on e-paper.

## Architecture: Bottle-Driven, iOS-Mirrored

```
┌─────────────────┐                    ┌─────────────────┐
│    iOS App      │                    │     Bottle      │
├─────────────────┤                    ├─────────────────┤
│                 │  START_CALIBRATION │                 │
│  "Calibrate"    │ ──────────────────>│ Enters calib    │
│   button tap    │                    │ state machine   │
│                 │                    │                 │
│                 │  STATE_NOTIFY      │ Detects stable  │
│  Updates rich   │ <─────────────────-│ upright, runs   │
│  UI to mirror   │  (state + weights) │ existing logic  │
│  bottle state   │                    │                 │
│                 │  CANCEL_CALIB      │                 │
│  Cancel button  │ ──────────────────>│ Returns to idle │
│                 │                    │                 │
│  User watches   │                    │ E-paper shows   │
│  phone screen   │                    │ simple version  │
└─────────────────┘                    └─────────────────┘
```

**Key Principles:**
- Bottle runs its existing, tested calibration state machine
- iOS sends only 2 commands: START and CANCEL
- Bottle broadcasts state changes as notifications
- iOS mirrors state with richer UI - user primarily watches phone
- Bottle e-paper shows simplified confirmation screens

---

## IRAM Verification ✓

Build test confirmed both BLE and standalone calibration fit:

| Configuration | RAM | Flash |
|--------------|-----|-------|
| BLE only | 37,984 bytes (11.6%) | 764,749 bytes (58.3%) |
| BLE + Standalone Calibration | 38,080 bytes (11.6%) | 771,733 bytes (58.9%) |
| **Delta** | **+96 bytes** | **+6,984 bytes** |

**Conclusion:** Minimal overhead, well within limits.

---

## BLE Protocol (Minimal)

### Commands (iOS → Bottle)

| Command | Code | Params | Description |
|---------|------|--------|-------------|
| `START_CALIBRATION` | `0x20` | None | Start calibration state machine |
| `CANCEL_CALIBRATION` | `0x21` | None | Abort calibration, return to idle |

### Notifications (Bottle → iOS)

**Calibration State Characteristic** (new, 12 bytes):

```cpp
struct __attribute__((packed)) BLE_CalibrationState {
    uint8_t  state;           // CalibrationState enum value
    uint8_t  flags;           // Bit 0: error occurred
    int32_t  empty_adc;       // Captured empty ADC (0 if not yet measured)
    int32_t  full_adc;        // Captured full ADC (0 if not yet measured)
    uint16_t reserved;        // Padding/future use
};
```

**State Values:**

| State | Value | iOS Action |
|-------|-------|------------|
| `CAL_IDLE` | 0 | Show "Start Calibration" button |
| `CAL_STARTED` | 2 | Show "Calibration Starting..." |
| `CAL_WAIT_EMPTY` | 3 | Show empty bottle prompt with live weight |
| `CAL_MEASURE_EMPTY` | 4 | Show "Measuring..." progress |
| `CAL_WAIT_FULL` | 6 | Show full bottle prompt with live weight |
| `CAL_MEASURE_FULL` | 7 | Show "Measuring..." progress |
| `CAL_COMPLETE` | 9 | Show success screen with results |
| `CAL_ERROR` | 10 | Show error with retry option |

---

## Implementation Phases

### Phase 1: Firmware Changes

**Files to modify:**
- [firmware/src/config.h](../firmware/src/config.h) - Enable standalone calibration in IOS_MODE ✓
- [firmware/include/ble_service.h](../firmware/include/ble_service.h) - Add calibration characteristic and commands
- [firmware/src/ble_service.cpp](../firmware/src/ble_service.cpp) - Implement command handlers and notifications
- [firmware/src/main.cpp](../firmware/src/main.cpp) - Integrate BLE calibration trigger with state machine

#### 1.1 Config Change (Already Done)

```cpp
#if IOS_MODE
    #define ENABLE_BLE                      1
    #define ENABLE_SERIAL_COMMANDS          0
    #define ENABLE_STANDALONE_CALIBRATION   1   // Enable alongside BLE
#endif
```

#### 1.2 Add BLE Calibration Characteristic

```cpp
// In ble_service.h
#define AQUAVATE_CALIBRATION_STATE_UUID "6F75616B-7661-7465-2D00-000000000008"

#define BLE_CMD_START_CALIBRATION   0x20  // Start calibration
#define BLE_CMD_CANCEL_CALIBRATION  0x21  // Cancel calibration

struct __attribute__((packed)) BLE_CalibrationState {
    uint8_t  state;           // CalibrationState enum
    uint8_t  flags;           // Bit 0: error flag
    int32_t  empty_adc;       // Empty ADC reading
    int32_t  full_adc;        // Full ADC reading
    uint16_t reserved;
};
```

#### 1.3 Add Command Handlers

```cpp
// In ble_service.cpp
case BLE_CMD_START_CALIBRATION:
    if (calibrationGetState() == CAL_IDLE) {
        calibrationStart();
        bleNotifyCalibrationState();
    }
    break;

case BLE_CMD_CANCEL_CALIBRATION:
    if (calibrationIsActive()) {
        calibrationCancel();
        uiCalibrationShowAborted();
        bleNotifyCalibrationState();
    }
    break;
```

#### 1.4 State Change Notifications

```cpp
// Call from main.cpp whenever calibration state changes
void bleNotifyCalibrationState() {
    BLE_CalibrationState state;
    state.state = (uint8_t)calibrationGetState();
    state.flags = (calibrationGetState() == CAL_ERROR) ? 0x01 : 0x00;

    CalibrationResult result = calibrationGetResult();
    state.empty_adc = result.data.empty_bottle_adc;
    state.full_adc = result.data.full_bottle_adc;
    state.reserved = 0;

    // Notify subscribed clients
    pCalibrationStateChar->setValue((uint8_t*)&state, sizeof(state));
    pCalibrationStateChar->notify();
}
```

#### 1.5 Main Loop Integration

```cpp
// In main.cpp loop() - when BLE connected
if (bleIsConnected()) {
    // Check for BLE calibration start command
    if (bleCheckCalibrationStartRequested()) {
        if (calibrationGetState() == CAL_IDLE) {
            calibrationInit();
            calibrationStart();
        }
    }

    // Check for BLE calibration cancel command
    if (bleCheckCalibrationCancelRequested()) {
        if (calibrationIsActive()) {
            calibrationCancel();
            uiCalibrationShowAborted();
        }
    }
}

// Normal calibration state machine update (existing code)
if (calibrationIsActive()) {
    CalibrationState prevState = calibrationGetState();
    calibrationUpdate(current_gesture, current_adc);

    // Notify iOS if state changed
    if (calibrationGetState() != prevState) {
        bleNotifyCalibrationState();
        uiCalibrationUpdateForState(calibrationGetState(), ...);
    }
}
```

---

### Phase 2: iOS BLE Updates

**Files to modify:**
- [ios/Aquavate/Aquavate/Services/BLEConstants.swift](../ios/Aquavate/Aquavate/Services/BLEConstants.swift)
- [ios/Aquavate/Aquavate/Services/BLEManager.swift](../ios/Aquavate/Aquavate/Services/BLEManager.swift)
- [ios/Aquavate/Aquavate/Services/BLEStructs.swift](../ios/Aquavate/Aquavate/Services/BLEStructs.swift)

#### 2.1 Add Constants

```swift
// BLEConstants.swift
static let calibrationStateUUID = CBUUID(string: "6F75616B-7661-7465-2D00-000000000008")

enum Command: UInt8 {
    // ... existing ...
    case startCalibration = 0x20
    case cancelCalibration = 0x21
}
```

#### 2.2 Add Calibration State Struct

```swift
// BLEStructs.swift
struct CalibrationState {
    let state: CalibrationStep
    let hasError: Bool
    let emptyADC: Int32
    let fullADC: Int32

    enum CalibrationStep: UInt8 {
        case idle = 0
        case started = 2
        case waitEmpty = 3
        case measureEmpty = 4
        case waitFull = 6
        case measureFull = 7
        case complete = 9
        case error = 10
    }

    init(data: Data) {
        // Parse 12-byte struct
        state = CalibrationStep(rawValue: data[0]) ?? .idle
        hasError = (data[1] & 0x01) != 0
        emptyADC = data.withUnsafeBytes { $0.load(fromByteOffset: 2, as: Int32.self) }
        fullADC = data.withUnsafeBytes { $0.load(fromByteOffset: 6, as: Int32.self) }
    }
}
```

#### 2.3 BLEManager Methods

```swift
// BLEManager.swift
@Published var calibrationState: CalibrationState?

func startCalibration() {
    sendCommand(.startCalibration)
}

func cancelCalibration() {
    sendCommand(.cancelCalibration)
}

// In peripheral(_:didUpdateValueFor:)
if characteristic.uuid == BLEConstants.calibrationStateUUID,
   let data = characteristic.value {
    calibrationState = CalibrationState(data: data)
}
```

---

### Phase 3: Simplified iOS Calibration UI

**Files to modify:**
- [ios/Aquavate/Aquavate/Services/CalibrationManager.swift](../ios/Aquavate/Aquavate/Services/CalibrationManager.swift) - Simplify to state mirror
- [ios/Aquavate/Aquavate/Views/CalibrationView.swift](../ios/Aquavate/Aquavate/Views/CalibrationView.swift) - Update to mirror bottle state

#### 3.1 Simplified CalibrationManager

```swift
@MainActor
class CalibrationManager: ObservableObject {
    @Published var isActive: Bool = false
    @Published var currentStep: CalibrationStep = .idle
    @Published var emptyADC: Int32 = 0
    @Published var fullADC: Int32 = 0
    @Published var hasError: Bool = false

    private weak var bleManager: BLEManager?

    func start() {
        bleManager?.startCalibration()
        isActive = true
    }

    func cancel() {
        bleManager?.cancelCalibration()
        isActive = false
    }

    // Called when BLE notification received
    func updateFromBLE(_ state: CalibrationState) {
        currentStep = state.state
        hasError = state.hasError
        emptyADC = state.emptyADC
        fullADC = state.fullADC

        if state.state == .complete || state.state == .idle {
            isActive = false
        }
    }
}
```

#### 3.2 CalibrationView Structure

```swift
struct CalibrationView: View {
    @StateObject private var calibrationManager = CalibrationManager()
    @EnvironmentObject var bleManager: BLEManager

    var body: some View {
        VStack {
            // Progress indicator
            CalibrationProgressView(step: calibrationManager.currentStep)

            // Content based on current step
            switch calibrationManager.currentStep {
            case .idle:
                WelcomeContent(onStart: calibrationManager.start)

            case .started:
                StartingContent()

            case .waitEmpty, .measureEmpty:
                EmptyBottleContent(
                    isMeasuring: calibrationManager.currentStep == .measureEmpty,
                    currentWeight: bleManager.currentWeight,
                    isStable: bleManager.isStable
                )

            case .waitFull, .measureFull:
                FullBottleContent(
                    isMeasuring: calibrationManager.currentStep == .measureFull,
                    currentWeight: bleManager.currentWeight,
                    isStable: bleManager.isStable
                )

            case .complete:
                CompleteContent(
                    emptyADC: calibrationManager.emptyADC,
                    fullADC: calibrationManager.fullADC
                )

            case .error:
                ErrorContent(onRetry: calibrationManager.start)
            }

            // Cancel button (when active)
            if calibrationManager.isActive {
                Button("Cancel") {
                    calibrationManager.cancel()
                }
                .foregroundColor(.red)
            }
        }
        .onReceive(bleManager.$calibrationState) { state in
            if let state = state {
                calibrationManager.updateFromBLE(state)
            }
        }
    }
}
```

---

## Comparison: Old vs New Approach

| Aspect | Old (iOS-Driven) | New (Bottle-Driven) |
|--------|-----------------|---------------------|
| BLE Commands | 4+ (measure point, set data, etc.) | 2 (start, cancel) |
| iOS Logic | Complex state machine, timing, retries | Simple state mirror |
| Firmware Logic | Minimal (just measure on demand) | Reuse existing state machine |
| Resilience | Dependent on BLE connection | Bottle works autonomously |
| IRAM Impact | ~450 bytes new | ~7KB (existing code enabled) |
| Development Effort | High (new iOS state machine) | Low (mostly wiring) |
| Testing | Complex integration testing | Simple - bottle logic already tested |

---

## Implementation Order

1. **Firmware: Enable standalone calibration** ✓ (config.h already updated)
2. **Firmware: Add BLE characteristic and commands** (~2 hours)
3. **Firmware: Integrate with main loop** (~1 hour)
4. **iOS: Add BLE constants and parsing** (~1 hour)
5. **iOS: Simplify CalibrationManager** (~1 hour)
6. **iOS: Update CalibrationView** (~2 hours)
7. **Test end-to-end** (~1 hour)

---

## Files Summary

| File | Changes |
|------|---------|
| `firmware/src/config.h` | Enable STANDALONE_CALIBRATION in IOS_MODE ✓ |
| `firmware/include/ble_service.h` | Add calibration UUID, commands, struct |
| `firmware/src/ble_service.cpp` | Add characteristic, command handlers, notifications |
| `firmware/src/main.cpp` | Integrate BLE calibration trigger |
| `ios/.../BLEConstants.swift` | Add calibration UUID and commands |
| `ios/.../BLEStructs.swift` | Add CalibrationState struct |
| `ios/.../BLEManager.swift` | Add calibration methods and state |
| `ios/.../CalibrationManager.swift` | Simplify to state mirror |
| `ios/.../CalibrationView.swift` | Update to display mirrored state |

---

## Completion Summary (2026-01-27)

### What Was Implemented

**Firmware:**
- CalibrationState BLE characteristic (12 bytes) for state broadcasting
- START_CALIBRATION (0x20) and CANCEL_CALIBRATION (0x21) commands
- Integration with existing standalone calibration state machine
- State change notifications to iOS on each transition

**iOS:**
- BLECalibrationState struct for parsing calibration notifications
- CalibrationManager with bottle-driven mode (state mirroring)
- Simplified CalibrationView with 4 screens: Welcome → Empty → Full → Complete
- Auto-transitions between steps (no intermediate confirmation screens)

### Simplified UI Flow

The iOS calibration UI was simplified to match the firmware's single-screen-per-step approach:
1. **Welcome** - Description + Start button
2. **Step 1: Empty Bottle** - Instructions + stability indicator/progress
3. **Step 2: Full Bottle** - Instructions + stability indicator/progress
4. **Complete** - Success message + Done button

### Testing Performed
- iOS app build verified (no compilation errors)
- Hardware testing pending

### Files Modified
See Files Summary table above.
