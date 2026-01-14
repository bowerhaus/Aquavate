# Plan: Comprehensive FSM Refactoring for Aquavate Firmware

**Created:** 2026-01-14
**Status:** Planning Complete - Ready for Implementation
**Branch:** TBD (will create `fsm-refactoring` branch)

---

## Executive Summary

Refactor the Aquavate firmware to use a comprehensive Hierarchical Finite State Machine (FSM) architecture. This addresses 9 identified bugs stemming from implicit state management and establishes a solid foundation for future features (BLE, OTA updates, deep sleep).

**Motivation:** Code clarity/maintainability + experiencing bugs with current state handling
**Scope:** Comprehensive refactoring (full hierarchy)
**Goal:** Fix existing bugs AND establish solid FSM architecture

---

## Critical Bugs Identified (from code analysis)

### HIGH SEVERITY (Must Fix)
1. **Bug 1.1**: Gesture collision - gesture read once but used by multiple subsystems with stale data
2. **Bug 3.1**: Daily reset race condition - no atomicity, can double-reset or lose data
3. **Bug 3.3**: Baseline ADC not reset on daily counter - causes first drink after 4am to use yesterday's baseline
4. **Bug 4.1**: Three state machines (calibration/gesture/drink) share data without synchronization
5. **Bug 4.2**: `g_time_valid` can change mid-loop but drinksInit() never called when it becomes true

### MEDIUM SEVERITY
6. **Bug 2.1**: Inverted hold can double-trigger if bottle wobbles
7. **Bug 2.2**: UPRIGHT_STABLE weight tracking uses inconsistent timer resets
8. **Bug 3.2**: Aggregation window state pollution after daily reset

### ROOT CAUSE
All bugs stem from **implicit state management** without clear ownership:
- Sensor readings cached once but used across multiple contexts
- State transitions happen without coordination
- No master state machine to orchestrate subsystems

---

## Pros and Cons of Comprehensive FSM Refactoring

### PROS

1. **Fixes Critical Bugs**
   - Eliminates all 9 identified bugs including high-severity race conditions
   - Prevents stale sensor reads (Bug 1.1, 4.1)
   - Fixes daily reset atomicity issues (Bug 3.1, 3.3)
   - Resolves gesture double-trigger problems (Bug 2.1)

2. **Code Clarity & Maintainability**
   - Explicit state makes behavior crystal clear
   - State transitions are self-documenting
   - Easier to understand control flow at a glance
   - Matches STATE_MACHINE.md documentation exactly

3. **Robustness & Reliability**
   - Prevents invalid state combinations
   - Atomic state transitions prevent half-updated state
   - Clear entry/exit actions ensure consistent initialization
   - No hidden state in boolean flags

4. **Easier Debugging**
   - Can log state transitions with Serial output
   - State is queryable at any time
   - State history can be tracked
   - Reproducible state sequences for testing

5. **Future Extensibility**
   - Adding BLE pairing mode is straightforward (new state)
   - Firmware update mode can be added cleanly
   - Deep sleep mode already architected in
   - Each feature gets isolated state handling

6. **Better Testing**
   - Can unit test state machines independently
   - Mock states for integration tests
   - Verify state transition correctness
   - Test edge cases systematically

### CONS

1. **Significant Refactoring Effort**
   - 6 files to modify (main, drinks, gestures + 3 headers)
   - ~500-800 lines of code changes
   - Need to test thoroughly after changes
   - Risk of introducing new bugs during refactor

2. **Increased Code Size**
   - New structs and enums (~200 bytes RAM)
   - State transition functions add ~1-2KB flash
   - SystemContext struct on stack each loop (~16 bytes)
   - May impact tight memory constraints on ESP32

3. **More Verbose Code**
   - Switch statements instead of simple if/else
   - Entry/exit handlers add boilerplate
   - State transition logging adds overhead
   - More code to read and understand initially

4. **Learning Curve**
   - Team members need to understand FSM pattern
   - More abstract than direct boolean logic
   - Requires discipline to maintain properly
   - May be overkill for simple embedded project

5. **Breaking Existing Patterns**
   - Changes APIs: `gesturesUpdate()` return type
   - `drinksUpdate()` takes SystemContext instead of raw values
   - Existing code patterns disrupted
   - Any external code relying on APIs will break

6. **Runtime Overhead (Minor)**
   - Function call overhead for state transitions
   - Switch statement evaluation each loop
   - Context struct copying
   - ~1-5% CPU increase (negligible at 5Hz loop)

### RISK ASSESSMENT

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Introduce new bugs during refactor | Medium | High | Comprehensive testing, incremental implementation |
| Break existing functionality | Medium | High | Test each phase before moving to next |
| Exceed memory constraints | Low | Medium | Monitor RAM/flash usage, optimize if needed |
| Timing issues with 200ms loop | Low | Low | Profile execution time |
| Team resistance to new pattern | Low | Low | Good documentation, clear benefits |

### VERDICT

**RECOMMENDED** - The pros strongly outweigh the cons:
- **Critical bugs need fixing** regardless of approach
- FSM provides systematic fix for root cause (implicit state)
- Refactoring effort is one-time, benefits are permanent
- Code clarity improvement is substantial
- Future extensibility is valuable (BLE, OTA updates planned)

The current bugs indicate the implicit state approach is fundamentally flawed. Better to fix architecture now than accumulate more state-related bugs.

---

## Recommended Solution: Hierarchical FSM

### Architecture Overview

```
SystemState (master)
├── SYSTEM_STARTUP
├── SYSTEM_NORMAL_OPERATION
│   ├── DrinkTrackingState (sub-FSM)
│   │   ├── DRINK_UNINITIALIZED
│   │   ├── DRINK_ESTABLISHING_BASELINE
│   │   ├── DRINK_MONITORING
│   │   └── DRINK_AGGREGATING
│   └── DisplayState (sub-FSM)
│       ├── DISPLAY_IDLE
│       ├── DISPLAY_NEEDS_UPDATE
│       └── DISPLAY_UPDATING
├── SYSTEM_CALIBRATION
│   └── CalibrationState (existing FSM - keep as-is)
│       ├── CAL_TRIGGERED
│       ├── CAL_WAIT_EMPTY
│       └── ... (existing states)
└── SYSTEM_DEEP_SLEEP
```

### Key Design Principles

1. **Single Source of Truth**: One `SystemState` enum controls which subsystem is active
2. **Event-Driven**: Gestures/sensor readings processed as events, not polled repeatedly
3. **State Entry/Exit Actions**: Each state has explicit entry/exit handlers
4. **Atomic Transitions**: State changes happen atomically with all necessary cleanup
5. **Clear Ownership**: Each state "owns" its subsystems - no shared mutable state

---

## Implementation Plan

### Phase 1: Create Master State Machine (main.cpp)

**Files to modify:**
- `firmware/src/main.cpp` (952-1172: refactor loop)
- `firmware/include/system_state.h` (NEW)

**Changes:**
1. Create `system_state.h` with master FSM:
   ```cpp
   enum SystemState {
       SYSTEM_STARTUP,
       SYSTEM_NORMAL_OPERATION,
       SYSTEM_CALIBRATION,
       SYSTEM_DEEP_SLEEP
   };

   // State context - sensor readings cached per loop iteration
   struct SystemContext {
       uint32_t loop_timestamp;
       int32_t current_adc;
       float current_water_ml;
       GestureType gesture;
       bool gesture_consumed;  // Prevent re-trigger
   };
   ```

2. Refactor `loop()` to:
   - Read sensors ONCE at start of loop
   - Populate `SystemContext`
   - Call state-specific handler based on `g_system_state`
   - Each state handler receives context (no global reads)

3. Add state transition functions:
   ```cpp
   void enterSystemState(SystemState new_state);
   void exitSystemState(SystemState old_state);
   void transitionSystemState(SystemState from, SystemState to);
   ```

**Fixes bugs**: 1.1 (sensor read collision), 4.1 (coordination)

---

### Phase 2: Refactor Drink Tracking to Explicit FSM (drinks.cpp)

**Files to modify:**
- `firmware/src/drinks.cpp` (all drink detection logic)
- `firmware/include/drinks.h` (add state enum)

**Key Changes:**
1. Add `DrinkTrackingState` enum (UNINITIALIZED, ESTABLISHING_BASELINE, MONITORING, AGGREGATING)
2. Add state field to `DailyState` struct
3. Refactor `drinksUpdate()` to state machine with switch statement
4. Add atomic daily reset handler `handleDailyReset()`
5. Handle time becoming valid in `onTimeSet()`

**Fixes bugs**: 3.1 (daily reset atomicity), 3.3 (baseline reset), 3.2 (window pollution), 4.2 (time valid change)

---

### Phase 3: Refactor Gesture Detection (gestures.cpp)

**Files to modify:**
- `firmware/src/gestures.cpp` (124-326: gesture detection logic)
- `firmware/include/gestures.h` (add state tracking)

**Key Changes:**
1. Add `GestureState` struct with acknowledgment pattern
2. Modify `gesturesUpdate()` to return `GestureState` instead of `GestureType`
3. Add `gestureAcknowledge()` function
4. Fix inverted hold latch with cooldown timer (Bug 2.1)
5. Fix UPRIGHT_STABLE weight tracking (Bug 2.2)

**Fixes bugs**: 2.1 (double-trigger), 2.2 (weight tracking), 4.3 (gesture latch persistence)

---

### Phase 4: Update Main Loop to Use New FSM (main.cpp)

**File to modify:**
- `firmware/src/main.cpp` (loop function 952-1172)

**New loop() structure:**
- Create SystemContext at start (read sensors once)
- Switch on `g_system_state`
- Call state-specific handlers (handleNormalOperation, handleCalibration)
- Each handler receives const SystemContext reference
- State transitions use explicit entry/exit handlers

---

## Summary of Changes

### New Files
- `firmware/include/system_state.h` - Master FSM definitions

### Modified Files
- `firmware/src/main.cpp` - Refactor loop() to use FSM
- `firmware/src/drinks.cpp` - Add drink tracking FSM
- `firmware/include/drinks.h` - Add DrinkTrackingState enum
- `firmware/src/gestures.cpp` - Add gesture acknowledgment, fix bugs
- `firmware/include/gestures.h` - Add GestureState struct
- `firmware/src/serial_commands.cpp` - Call drinksInit() when time becomes valid

### Bugs Fixed
✅ Bug 1.1: Gesture collision (via SystemContext)
✅ Bug 2.1: Double-trigger (via cooldown timer)
✅ Bug 2.2: Weight tracking (via explicit duration tracking)
✅ Bug 3.1: Daily reset race (via atomic handler)
✅ Bug 3.2: Window pollution (via state reset in handler)
✅ Bug 3.3: Baseline not reset (via handleDailyReset)
✅ Bug 4.1: Three-state collision (via master FSM)
✅ Bug 4.2: Time valid change (via onTimeSet)
✅ Bug 4.3: Gesture latch (via acknowledgment pattern)

---

## Testing Strategy

### Unit Tests (if applicable)
1. Test state transitions for each FSM
2. Test daily reset atomicity
3. Test gesture acknowledgment prevents double-trigger
4. Test SystemContext prevents stale reads

### Integration Tests
1. Trigger calibration, verify state transitions
2. Drink detection across 4am boundary
3. Multiple sips within 5-min window aggregate correctly
4. Time set command initializes drink tracking

### Manual Tests
1. Hold bottle inverted 5s → calibration starts (no double-trigger)
2. Drink 30ml → recorded correctly
3. Drink at 3:59am and 4:01am → separate days
4. Serial SET_TIME command → drink tracking initializes

---

## Verification Checklist
- [ ] All compilation warnings resolved
- [ ] Serial debug output shows state transitions
- [ ] Calibration completes successfully
- [ ] Drinks detected and aggregated correctly
- [ ] Daily reset works at 4am boundary
- [ ] No crashes or memory leaks
- [ ] State machine diagram matches implementation
- [ ] Memory usage within constraints (< 10% RAM, < 50% Flash)

---

## Implementation Notes

**Incremental Approach:**
Implement in 4 phases as outlined above. Test thoroughly after each phase before proceeding to the next. This minimizes risk and allows for early detection of issues.

**Memory Monitoring:**
After each phase, check compilation output for RAM/Flash usage. Target: Keep RAM < 10%, Flash < 50%.

**Serial Debugging:**
Add state transition logging with timestamps to aid debugging:
```cpp
Serial.printf("[%lu] State transition: %s -> %s\n",
              millis(),
              getSystemStateName(old_state),
              getSystemStateName(new_state));
```

**Backward Compatibility:**
This refactoring breaks API compatibility with external code. Since this is standalone firmware with no external dependencies, this is acceptable. Document API changes in commit message.
