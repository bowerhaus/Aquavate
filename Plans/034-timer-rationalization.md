# Plan 034: Timer & Sleep Behavior Rationalization

**Status:** ✅ Complete
**Branch:** `timer-rationalization`
**Created:** 2026-01-22
**Completed:** 2026-01-22
**Related:** [Plan 028 - BLE Auto-Reconnection](028-ble-auto-reconnection.md)

---

## Background

While debugging [Plan 028 (BLE Auto-Reconnection)](028-ble-auto-reconnection.md), we discovered that the firmware has too many overlapping timers with different reset conditions, making behavior hard to predict and debug. This plan simplifies the timer architecture.

---

## User Requirements

- **Simplicity** is the priority
- **Timeout even when connected** - don't let BLE keep device awake forever
- **Review backpack mode** - include in rationalization

---

## Current State (6 Timers - Too Many)

| Timer | Duration | Reset Condition | Problem |
|-------|----------|-----------------|---------|
| Normal Sleep | 30s | Any gesture | Overlaps with advertising |
| Extended Sleep Threshold | 5 min | UPRIGHT_STABLE only | Different reset rule |
| Extended Sleep Interval | 60s | Never | Separate concept |
| BLE Advertising | 30s/120s | On disconnect | Independent of sleep |
| Awake Extended | 120s | Never | Conditional on unsynced |

**iOS**: Idle disconnect (60s), Background disconnect (5s), Foreground scan (5s)

**Result**: Confusing interactions, hard to debug.

---

## Proposed: Two-Timer Model

Replace 5 firmware timers with just **2 simple concepts**:

### Timer 1: Activity Timeout (30 seconds)

**Purpose**: Enter sleep when device is idle

**Reset on**:
- Gesture change (any)
- BLE data activity (sync, command received)

**Does NOT reset on**:
- BLE connect/disconnect (connection alone isn't "activity")

**When expires**:
- Stop advertising
- Enter normal deep sleep (motion wake)

### Timer 2: Time Since Stable (3 minutes)

**Purpose**: Force sleep in backpack scenario (constant motion)

**Resets when**: bottle becomes UPRIGHT_STABLE

**Counts when**: bottle is in motion / not stable

**When expires**:
- Enter extended deep sleep (timer wake, 60s)
- This is the "backpack mode" - use timer wake since motion wake would trigger immediately

**Normal use scenario**:
- Bottle on table (stable) → timer keeps resetting → never triggers
- Pick up, pour, put down → timer never reaches 3 min

**Backpack scenario**:
- Constant motion, never stable → timer counts up
- After 3 min → extended sleep

---

## Extended Sleep Wake Behavior

When waking from extended sleep (timer wake every 60s):

```
Timer Wake
    │
    ├─ Check motion for 500ms
    │
    ├─ Still moving?
    │   └─ Re-enter extended sleep immediately
    │      (no 3 min wait - backpack scenario continues)
    │
    └─ Stable (bottle taken out of backpack)?
        ├─ Start advertising
        ├─ Activity timeout = 30s
        ├─ Time-since-stable = 0 (just became stable)
        │
        ├─ If user interacts → activity timeout resets, normal operation
        └─ If idle 30s → normal sleep (motion wake)
```

**Key point**: No 3-minute wait to re-enter extended sleep if motion continues. The 3-minute threshold is only for the *initial* transition from normal operation to backpack mode.

---

## Behavior Comparison

### Current (Confusing)
```
Motion Wake
├─ Advertising timeout (30s) - can stop while still awake
├─ Sleep timeout (30s) - blocked while connected
├─ If unsynced: both become 120s
├─ If connected >60s: iOS disconnects → restart advertising
├─ Extended sleep threshold (5min) - separate tracking
```

### Proposed (Simple)
```
Motion Wake
├─ Start advertising
├─ Activity timeout = 30s (resets on gesture/BLE data)
├─ Time since stable = 0 (resets when UPRIGHT_STABLE)
│
├─ Activity → reset activity timeout
├─ UPRIGHT_STABLE → reset time-since-stable
├─ Activity timeout expires → normal sleep (motion wake)
├─ Time since stable reaches 3 min → extended sleep (timer wake)
│
└─ Sleep → stop advertising
```

---

## Removed Complexity

| Removed | Replacement |
|---------|-------------|
| Separate advertising timeout | Advertising stops at sleep |
| Extended awake (120s if unsynced) | Removed - rely on time-since-stable |
| Extended sleep threshold (5 min) | Time since stable (3 min) - same concept, clearer name |
| "Continuous awake" tracking | Time since stable - same concept, clearer name |
| Different reset conditions | Two clear rules: activity resets timeout, stable resets backpack timer |

---

## Backpack Mode Simplified

**Current**: "Continuous awake" tracking - confusing name, resets on UPRIGHT_STABLE

**Proposed**: "Time since stable" - same logic, clearer name, shorter threshold

```
Time since stable timer:
  - Resets: when bottle is UPRIGHT_STABLE
  - Expires (3 min): enter extended deep sleep (timer wake 60s)
```

**Why this works**: In a backpack, the bottle is never stable (upright and still), so the timer counts up. On a desk, even brief stability resets the timer.

**Change from current**: Threshold reduced from 5 min → 3 min for faster battery protection.

---

## Config Changes

```cpp
// Remove these
// #define BLE_ADV_TIMEOUT_SEC 30
// #define BLE_ADV_TIMEOUT_EXTENDED_SEC 120
// #define AWAKE_DURATION_EXTENDED_MS 120000

// Rename for clarity (same concept, clearer name)
// OLD: #define EXTENDED_SLEEP_THRESHOLD_SEC 300
// NEW:
#define TIME_SINCE_STABLE_THRESHOLD_SEC  180    // 3 minutes (was 5 min)

// Keep these
#define ACTIVITY_TIMEOUT_MS              30000  // 30 seconds idle → sleep
#define EXTENDED_SLEEP_TIMER_SEC            60  // Timer wake interval in backpack mode
```

---

## iOS App Impact

Minimal changes needed:

1. **Idle disconnect timer (60s)** - Keep as-is, still useful
2. **Background disconnect (5s)** - Keep as-is
3. No need to handle "advertising stopped but device awake" edge case

The bottle behavior becomes predictable: "awake = advertising, asleep = not advertising"

---

## Implementation Steps

### Step 1: Simplify Advertising ✅
- [x] Remove `BLE_ADV_TIMEOUT_SEC` and `bleUpdate()` timeout check
- [x] Advertising runs until `bleStopAdvertising()` is called at sleep

### Step 2: Implement Activity Timeout ✅
- [x] Single timer in main.cpp
- [x] Reset on gesture change (existing) AND BLE data activity (new via `bleCheckDataActivity()`)
- [x] When expires → enter normal sleep (motion wake)

### Step 3: Rename & Simplify Time Since Stable ✅
- [x] Rename `g_continuous_awake_start` → `g_time_since_stable_start`
- [x] Keep existing reset logic (UPRIGHT_STABLE)
- [x] Change threshold from 5 min → 3 min
- [x] When expires → enter extended sleep (timer wake)

### Step 4: Remove Legacy Code ✅
- [x] Remove `AWAKE_DURATION_EXTENDED_MS` and unsynced-record conditional
- [x] Remove advertising timeout in `bleUpdate()`
- [x] Remove `BLE_ADV_TIMEOUT_SEC` / `BLE_ADV_TIMEOUT_EXTENDED_SEC`

### Step 5: Update Config ✅
- [x] Rename `EXTENDED_SLEEP_THRESHOLD_SEC` → `TIME_SINCE_STABLE_THRESHOLD_SEC`
- [x] Update threshold: 300 → 180 (5 min → 3 min)
- [x] Add clear comments explaining two-timer model

---

## Files to Modify

| File | Changes |
|------|---------|
| [config.h](../firmware/include/config.h) | New constants, remove old |
| [main.cpp](../firmware/src/main.cpp) | Activity timeout, simplified sleep logic |
| [ble_service.cpp](../firmware/src/ble_service.cpp) | Remove advertising timeout, add activity callback |
| [ble_service.h](../firmware/include/ble_service.h) | Remove timeout constants |

---

## Verification

1. **Basic sleep**: Pick up bottle, set down, verify sleeps after 30s idle
2. **Activity extends awake**: Pick up, move around, verify resets 30s activity timer
3. **Stable resets backpack timer**: Move bottle for 2 min, set down stable briefly, move again - should NOT trigger extended sleep at 3 min total (timer was reset)
4. **Backpack mode triggers**: Shake/move continuously without stability, verify extended sleep after 3 min
5. **Extended sleep continues**: While in extended sleep, keep shaking - should stay in extended sleep (immediate re-entry, no 3 min wait)
6. **Extended sleep exit**: In extended sleep, set bottle down stable - should wake, advertise for 30s, then normal sleep
7. **BLE sync resets activity**: Connect app, sync, verify activity timeout resets during sync
8. **BLE doesn't block indefinitely**: Stay connected without activity, verify sleeps after 30s
9. **Advertising tied to awake**: Verify advertising stops exactly when sleep starts
10. **Normal use scenario**: Bottle on desk 2.5 min, pick up to pour, put down - should NOT trigger extended sleep
