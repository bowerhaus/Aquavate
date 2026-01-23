# Activity & Sleep Mode Tracking for Battery Analysis

**Status:** ✅ COMPLETE (2026-01-23) - Needs device testing
**Branch:** `activity-sleep-tracking`
**Issue:** #36

## GitHub Issue Summary

### Feature: Activity & Sleep Mode Tracking

**Problem:** Currently there's no way to verify whether the bottle's dual sleep mode system (normal deep sleep vs extended "backpack mode") is working as intended. Users cannot diagnose battery drain issues or confirm the power-saving backpack mode is triggering correctly.

**Solution:** Track individual wake events and backpack sessions on the firmware, exposing this data to the iOS app for analysis.

### Requirements

#### Firmware
- [x] Record each motion wake event with: timestamp, duration awake, and which sleep mode was entered after
- [x] Aggregate backpack mode sessions with: start time, total duration, number of timer wakes, exit reason
- [x] Store data in RTC memory (survives deep sleep, resets on power cycle - "since last charge")
- [x] Capacity: 100 motion wake events + 20 backpack sessions (~1KB)
- [x] Expose data via new BLE characteristic with chunked transfer protocol
- [x] **Enhancement:** "Drink taken" flag in motion wake events (bit 7 of sleep_type)

#### iOS App
- [x] New "Sleep Mode Analysis" view accessible from Settings (Diagnostics section)
- [x] Lazy loading: only fetch data when user opens the view (not on app launch)
- [x] Display current status (normal mode vs in backpack mode)
- [x] Display summary counts (motion wakes, backpack sessions since charge)
- [x] List recent motion wake events with timestamps and durations
- [x] List backpack sessions with duration and timer wake counts
- [x] Pull-to-refresh to update data
- [x] Disabled state when bottle not connected
- [x] **Enhancement:** Water drop icon shown for wake events where drink was taken

### Use Cases
1. **Battery drain diagnosis**: See if bottle is waking too frequently
2. **Backpack mode verification**: Confirm extended sleep triggers during transport
3. **Usage patterns**: Understand how often bottle is picked up vs left stationary

---

## Goal
Track bottle activity (motion wakes and backpack sessions) so the iOS app can analyze sleep mode usage and battery life.

## Design Decisions
- **Storage**: RTC memory (survives deep sleep, resets on power cycle)
- **Motion wakes**: Individual records (100 events = 2-5 days typical)
- **Backpack sessions**: Aggregated records (20 sessions = many days)
- **BLE**: New dedicated characteristic

---

## Data Structures

### MotionWakeEvent (8 bytes)
Individual record for each time the bottle is picked up (motion wake).

```cpp
struct __attribute__((packed)) MotionWakeEvent {
    uint32_t timestamp;        // When wake occurred
    uint16_t duration_sec;     // How long awake before sleeping
    uint8_t  sleep_type;       // 0=normal, 1=extended (entered backpack mode after this wake)
    uint8_t  flags;            // Reserved
};
```

### BackpackSession (12 bytes)
Aggregated record for extended sleep (backpack mode) periods.

```cpp
struct __attribute__((packed)) BackpackSession {
    uint32_t start_timestamp;  // When backpack mode started
    uint32_t duration_sec;     // Total time in backpack mode
    uint16_t timer_wake_count; // Number of timer wakes during session
    uint8_t  exit_reason;      // 0=motion_detected, 1=still_active, 2=power_cycle
    uint8_t  flags;            // Reserved
};
```

### ActivityBuffer (RTC Memory, ~1,060 bytes)
```cpp
#define MOTION_WAKE_MAX_COUNT    100
#define BACKPACK_SESSION_MAX_COUNT 20

struct __attribute__((packed)) ActivityBuffer {
    uint32_t magic;                                    // 0x41435456 ("ACTV")

    // Motion wake circular buffer
    uint8_t  motion_write_index;
    uint8_t  motion_count;
    MotionWakeEvent motion_events[MOTION_WAKE_MAX_COUNT];  // 800 bytes

    // Backpack session circular buffer
    uint8_t  session_write_index;
    uint8_t  session_count;
    BackpackSession sessions[BACKPACK_SESSION_MAX_COUNT];  // 240 bytes

    // Current session tracking (for active backpack mode)
    uint32_t current_session_start;      // 0 if not in backpack mode
    uint16_t current_timer_wake_count;   // Accumulates during backpack mode
    uint16_t _reserved;
};
```

**Total: ~1,060 bytes** (fits comfortably in ESP32's ~8KB RTC memory)

---

## Capacity

| Data Type | Count | Size | Typical Coverage |
|-----------|-------|------|------------------|
| Motion wakes | 100 | 800 bytes | 2-5 days (20-50 wakes/day) |
| Backpack sessions | 20 | 240 bytes | 4-20 days (1-5 sessions/day) |
| **Total** | - | ~1,060 bytes | **Easily covers 24+ hours** |

---

## State Machine

```
                    ┌─────────────────────────────────────┐
                    │                                     │
                    ▼                                     │
    ┌──────────┐  motion   ┌──────────┐  30s idle  ┌─────────────┐
    │  ASLEEP  │ ────────► │  AWAKE   │ ─────────► │ NORMAL SLEEP│
    │ (normal) │           │          │            └─────────────┘
    └──────────┘           │          │                   │
         ▲                 │          │  3min motion      │ motion
         │                 │          │  (no stable)      │ detected
         │                 │          ▼                   │
         │                 │    ┌─────────────┐           │
         │                 │    │  BACKPACK   │◄──────────┘
         │                 │    │   MODE      │
         │                 │    │             │
         │                 │    │ (timer wake │
         │                 │    │  every 60s) │
         │                 │    └─────────────┘
         │                 │          │
         │                 │          │ stable detected
         │                 │          ▼
         │                 │    Record BackpackSession
         │                 │          │
         └─────────────────┴──────────┘
```

---

## Recording Logic

### On Motion Wake (EXT0 interrupt)
```cpp
void activityRecordMotionWake() {
    // If we were in backpack mode, finalize that session first
    if (g_buffer.current_session_start != 0) {
        finalizeBackpackSession(EXIT_MOTION_DETECTED);
    }

    // Start tracking this wake
    g_current_wake.timestamp = getCurrentUnixTime();
    g_current_wake.start_millis = millis();
}
```

### On Entering Normal Sleep
```cpp
void activityRecordNormalSleep() {
    MotionWakeEvent event;
    event.timestamp = g_current_wake.timestamp;
    event.duration_sec = (millis() - g_current_wake.start_millis) / 1000;
    event.sleep_type = SLEEP_TYPE_NORMAL;
    addMotionEvent(event);
}
```

### On Entering Extended Sleep (Backpack Mode)
```cpp
void activityRecordExtendedSleep() {
    // Record the motion wake that led to backpack mode
    MotionWakeEvent event;
    event.timestamp = g_current_wake.timestamp;
    event.duration_sec = (millis() - g_current_wake.start_millis) / 1000;
    event.sleep_type = SLEEP_TYPE_EXTENDED;  // Indicates backpack mode started
    addMotionEvent(event);

    // Start tracking backpack session
    if (g_buffer.current_session_start == 0) {
        g_buffer.current_session_start = getCurrentUnixTime();
        g_buffer.current_timer_wake_count = 0;
    }
}
```

### On Timer Wake (in backpack mode)
```cpp
void activityRecordTimerWake() {
    g_buffer.current_timer_wake_count++;
    // No individual event recorded - just increment counter
}
```

### On Exiting Backpack Mode (motion detected during timer check)
```cpp
void finalizeBackpackSession(uint8_t exit_reason) {
    BackpackSession session;
    session.start_timestamp = g_buffer.current_session_start;
    session.duration_sec = getCurrentUnixTime() - g_buffer.current_session_start;
    session.timer_wake_count = g_buffer.current_timer_wake_count;
    session.exit_reason = exit_reason;
    addBackpackSession(session);

    // Reset current session
    g_buffer.current_session_start = 0;
    g_buffer.current_timer_wake_count = 0;
}
```

---

## BLE Characteristic

### UUID
`6F75616B-7661-7465-2D00-000000000007`

### Structure: BLE_ActivityData
Since total data is ~1KB, we'll use chunked transfer similar to drink sync.

```cpp
// Summary header (first read)
struct __attribute__((packed)) BLE_ActivitySummary {
    uint8_t  motion_event_count;      // Number of motion wake events
    uint8_t  backpack_session_count;  // Number of backpack sessions
    uint8_t  in_backpack_mode;        // Currently in backpack mode?
    uint8_t  flags;                   // Bit 0: time_valid
    uint32_t current_session_start;   // If in backpack mode, when it started
    uint16_t current_timer_wakes;     // Timer wakes in current session
    uint16_t _reserved;
};  // 12 bytes

// Motion event chunk (on request)
struct __attribute__((packed)) BLE_MotionEventChunk {
    uint8_t  chunk_index;
    uint8_t  total_chunks;
    uint8_t  event_count;             // Events in this chunk (max 10)
    uint8_t  _reserved;
    MotionWakeEvent events[10];       // 80 bytes
};  // 84 bytes

// Backpack session chunk (on request)
struct __attribute__((packed)) BLE_BackpackSessionChunk {
    uint8_t  chunk_index;
    uint8_t  total_chunks;
    uint8_t  session_count;           // Sessions in this chunk (max 5)
    uint8_t  _reserved;
    BackpackSession sessions[5];      // 60 bytes
};  // 64 bytes
```

### Commands
```cpp
#define BLE_CMD_GET_ACTIVITY_SUMMARY  0x21  // Get summary
#define BLE_CMD_GET_MOTION_CHUNK      0x22  // Get motion event chunk N
#define BLE_CMD_GET_BACKPACK_CHUNK    0x23  // Get backpack session chunk N
```

---

## New Files

| File | Purpose |
|------|---------|
| `firmware/include/activity_stats.h` | Structs, enums, function declarations |
| `firmware/src/activity_stats.cpp` | Implementation |

## Files to Modify

| File | Changes |
|------|---------|
| `firmware/include/ble_service.h` | Add UUID, structs, commands |
| `firmware/src/ble_service.cpp` | Add characteristic and command handlers |
| `firmware/src/main.cpp` | Integration in setup() and sleep functions |

---

## Integration Points

### main.cpp - setup()
```cpp
// After RTC restore
activityRestoreFromRTC();

// Record wake based on reason
if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    activityRecordMotionWake();
} else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    activityRecordTimerWake();
} else {
    activityInit();  // Power cycle - fresh buffer
}
```

### main.cpp - enterDeepSleep()
```cpp
activityRecordNormalSleep();
activitySaveToRTC();
```

### main.cpp - enterExtendedDeepSleep()
```cpp
activityRecordExtendedSleep();
activitySaveToRTC();
```

---

## Example Data

**Motion Wake Events:**
```
#0: 10:32:15, awake 45s, → normal sleep
#1: 11:15:42, awake 120s, → normal sleep
#2: 14:00:00, awake 180s, → BACKPACK MODE (3min motion triggered it)
#3: 15:30:00, awake 60s, → normal sleep
```

**Backpack Sessions:**
```
#0: Started 14:03:00, duration 5400s (90min), 90 timer wakes, exit: motion_detected
#1: Started 16:00:00, duration 1800s (30min), 30 timer wakes, exit: motion_detected
```

From this the app can calculate:
- Time spent in each mode
- How often backpack mode triggers
- Average wake duration
- Battery drain estimates

---

---

# iOS App Implementation

## Design Principle
**Lazy loading**: Activity data is only fetched when the user explicitly requests it (navigates to the Activity Stats section in Settings). This avoids unnecessary BLE traffic.

---

## New Files

| File | Purpose |
|------|---------|
| `ios/.../Models/ActivityStats.swift` | Data models for activity stats |

## Files to Modify

| File | Changes |
|------|---------|
| `ios/.../Services/BLEConstants.swift` | Add activity stats UUID and commands |
| `ios/.../Services/BLEStructs.swift` | Add BLE struct parsers |
| `ios/.../Services/BLEManager.swift` | Add fetch methods (on-demand, not auto) |
| `ios/.../Views/SettingsView.swift` | Add Activity Stats section with disclosure |

---

## iOS Data Models

### ActivityStats.swift
```swift
import Foundation

struct MotionWakeEvent: Identifiable {
    let id = UUID()
    let timestamp: Date
    let durationSec: Int
    let sleepType: SleepType

    enum SleepType: UInt8 {
        case normal = 0
        case extended = 1  // Entered backpack mode after this wake
    }
}

struct BackpackSession: Identifiable {
    let id = UUID()
    let startTime: Date
    let durationSec: Int
    let timerWakeCount: Int
    let exitReason: ExitReason

    enum ExitReason: UInt8 {
        case motionDetected = 0
        case stillActive = 1
        case powerCycle = 2
    }
}

struct ActivitySummary {
    let motionEventCount: Int
    let backpackSessionCount: Int
    let isInBackpackMode: Bool
    let currentSessionStart: Date?
    let currentTimerWakes: Int
}
```

---

## BLE Structs (BLEStructs.swift additions)

```swift
// MARK: - Activity Stats Structures

struct BLEActivitySummary {
    let motionEventCount: UInt8
    let backpackSessionCount: UInt8
    let inBackpackMode: UInt8
    let flags: UInt8
    let currentSessionStart: UInt32
    let currentTimerWakes: UInt16

    static func parse(from data: Data) -> BLEActivitySummary? {
        guard data.count >= 12 else { return nil }
        return BLEActivitySummary(
            motionEventCount: data[0],
            backpackSessionCount: data[1],
            inBackpackMode: data[2],
            flags: data[3],
            currentSessionStart: data.withUnsafeBytes { $0.load(fromByteOffset: 4, as: UInt32.self) },
            currentTimerWakes: data.withUnsafeBytes { $0.load(fromByteOffset: 8, as: UInt16.self) }
        )
    }
}

struct BLEMotionWakeEvent {
    let timestamp: UInt32
    let durationSec: UInt16
    let sleepType: UInt8
    let flags: UInt8

    static let size = 8

    static func parse(from data: Data, offset: Int) -> BLEMotionWakeEvent? {
        guard data.count >= offset + size else { return nil }
        let slice = data.subdata(in: offset..<offset+size)
        return BLEMotionWakeEvent(
            timestamp: slice.withUnsafeBytes { $0.load(as: UInt32.self) },
            durationSec: slice.withUnsafeBytes { $0.load(fromByteOffset: 4, as: UInt16.self) },
            sleepType: slice[6],
            flags: slice[7]
        )
    }
}

struct BLEBackpackSession {
    let startTimestamp: UInt32
    let durationSec: UInt32
    let timerWakeCount: UInt16
    let exitReason: UInt8
    let flags: UInt8

    static let size = 12

    static func parse(from data: Data, offset: Int) -> BLEBackpackSession? {
        guard data.count >= offset + size else { return nil }
        let slice = data.subdata(in: offset..<offset+size)
        return BLEBackpackSession(
            startTimestamp: slice.withUnsafeBytes { $0.load(as: UInt32.self) },
            durationSec: slice.withUnsafeBytes { $0.load(fromByteOffset: 4, as: UInt32.self) },
            timerWakeCount: slice.withUnsafeBytes { $0.load(fromByteOffset: 8, as: UInt16.self) },
            exitReason: slice[10],
            flags: slice[11]
        )
    }
}
```

---

## BLEConstants.swift additions

```swift
// Activity Stats
static let activityStatsUUID = CBUUID(string: "6F75616B-7661-7465-2D00-000000000007")

// Activity Stats Commands (sent via commandUUID)
static let cmdGetActivitySummary: UInt8 = 0x21
static let cmdGetMotionChunk: UInt8 = 0x22
static let cmdGetBackpackChunk: UInt8 = 0x23
```

---

## BLEManager.swift additions

```swift
// MARK: - Activity Stats (On-Demand Fetch)

enum ActivityFetchState {
    case idle
    case fetchingSummary
    case fetchingMotionEvents
    case fetchingBackpackSessions
    case complete
    case failed(Error)
}

@Published private(set) var activityFetchState: ActivityFetchState = .idle
@Published private(set) var activitySummary: ActivitySummary?
@Published private(set) var motionWakeEvents: [MotionWakeEvent] = []
@Published private(set) var backpackSessions: [BackpackSession] = []

/// Fetch activity stats on-demand (called when user opens Activity Stats view)
func fetchActivityStats() async {
    guard connectionState == .connected else { return }

    activityFetchState = .fetchingSummary
    motionWakeEvents = []
    backpackSessions = []

    // 1. Send command to get summary
    sendCommand(BLEConstants.cmdGetActivitySummary, param: 0)

    // Wait for response via notification handler
    // (Continuation pattern or delegate callback)
}

/// Handle activity stats characteristic notification
private func handleActivityStatsUpdate(_ data: Data) {
    // Parse based on current fetch state
    switch activityFetchState {
    case .fetchingSummary:
        if let summary = BLEActivitySummary.parse(from: data) {
            activitySummary = ActivitySummary(/* map from BLE struct */)
            // Request motion events if any
            if summary.motionEventCount > 0 {
                activityFetchState = .fetchingMotionEvents
                sendCommand(BLEConstants.cmdGetMotionChunk, param: 0)
            } else if summary.backpackSessionCount > 0 {
                activityFetchState = .fetchingBackpackSessions
                sendCommand(BLEConstants.cmdGetBackpackChunk, param: 0)
            } else {
                activityFetchState = .complete
            }
        }
    case .fetchingMotionEvents:
        // Parse chunk, append to motionWakeEvents
        // Request next chunk or move to backpack sessions
    case .fetchingBackpackSessions:
        // Parse chunk, append to backpackSessions
        // Request next chunk or complete
    default:
        break
    }
}
```

---

## SettingsView.swift additions

```swift
// Add new section in SettingsView List

Section("Activity Stats") {
    NavigationLink {
        ActivityStatsView()
    } label: {
        HStack {
            Label("Sleep Mode Analysis", systemImage: "moon.zzz")
            Spacer()
            if bleManager.activityFetchState == .fetchingSummary ||
               bleManager.activityFetchState == .fetchingMotionEvents {
                ProgressView()
                    .scaleEffect(0.8)
            }
        }
    }
    .disabled(bleManager.connectionState != .connected)
}
```

### New View: ActivityStatsView.swift

```swift
import SwiftUI

struct ActivityStatsView: View {
    @EnvironmentObject var bleManager: BLEManager

    var body: some View {
        List {
            // Summary Section
            if let summary = bleManager.activitySummary {
                Section("Current Status") {
                    if summary.isInBackpackMode {
                        Label("In Backpack Mode", systemImage: "bag.fill")
                        Text("Timer wakes: \(summary.currentTimerWakes)")
                    } else {
                        Label("Normal Mode", systemImage: "drop.fill")
                    }
                }

                Section("Since Last Charge") {
                    LabeledContent("Motion Wakes", value: "\(summary.motionEventCount)")
                    LabeledContent("Backpack Sessions", value: "\(summary.backpackSessionCount)")
                }
            }

            // Motion Wake Events
            if !bleManager.motionWakeEvents.isEmpty {
                Section("Recent Motion Wakes") {
                    ForEach(bleManager.motionWakeEvents.prefix(20)) { event in
                        HStack {
                            VStack(alignment: .leading) {
                                Text(event.timestamp, style: .time)
                                    .font(.headline)
                                Text("Awake \(event.durationSec)s")
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                            }
                            Spacer()
                            if event.sleepType == .extended {
                                Label("→ Backpack", systemImage: "bag")
                                    .font(.caption)
                                    .foregroundColor(.orange)
                            }
                        }
                    }
                }
            }

            // Backpack Sessions
            if !bleManager.backpackSessions.isEmpty {
                Section("Backpack Sessions") {
                    ForEach(bleManager.backpackSessions) { session in
                        VStack(alignment: .leading, spacing: 4) {
                            Text(session.startTime, style: .date) + Text(" ") +
                            Text(session.startTime, style: .time)
                            HStack {
                                Text(formatDuration(session.durationSec))
                                Text("•")
                                Text("\(session.timerWakeCount) timer wakes")
                            }
                            .font(.caption)
                            .foregroundColor(.secondary)
                        }
                    }
                }
            }
        }
        .navigationTitle("Activity Stats")
        .onAppear {
            // Lazy load: only fetch when view appears
            Task {
                await bleManager.fetchActivityStats()
            }
        }
        .refreshable {
            await bleManager.fetchActivityStats()
        }
    }

    private func formatDuration(_ seconds: Int) -> String {
        let hours = seconds / 3600
        let minutes = (seconds % 3600) / 60
        if hours > 0 {
            return "\(hours)h \(minutes)m"
        }
        return "\(minutes)m"
    }
}
```

---

## Verification Plan

### Firmware
1. **Build**: Firmware compiles with new code
2. **Power cycle**: Buffer initializes fresh
3. **Motion wake**: Pick up bottle, verify event recorded
4. **Normal sleep**: Set down, wait 30s, verify sleep_type=normal
5. **Backpack mode trigger**: Shake for 3min, verify sleep_type=extended
6. **Timer wake counting**: In backpack mode, verify counter increments each minute
7. **Session finalization**: After backpack mode exits, verify session recorded with correct duration/count
8. **BLE read**: Use nRF Connect to read summary and request chunks

### iOS App
9. **Navigation**: Settings → Activity Stats view opens
10. **Lazy load**: Verify BLE fetch only triggers on view appear (not on app launch)
11. **Summary display**: Verify motion wake count, backpack session count shown
12. **Event list**: Verify motion wake events display with correct times/durations
13. **Session list**: Verify backpack sessions display with duration and timer wake count
14. **Refresh**: Pull-to-refresh re-fetches data
15. **Disconnected state**: View disabled when bottle not connected
