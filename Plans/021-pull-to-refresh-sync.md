# Pull-to-Refresh Sync for HomeView

## Summary

Add pull-to-refresh gesture to HomeView that connects to the bottle (if needed) and syncs drink history. Shows "Bottle is asleep - tilt to wake" alert if the bottle cannot be found.

## Files to Modify

1. **[ios/Aquavate/Aquavate/Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)**
   - Add `RefreshResult` enum for async operation outcomes
   - Add `performRefresh() async -> RefreshResult` - main entry point
   - Add `attemptConnection() async` - uses Combine to bridge callback to async
   - Add `performSyncOnly() async` - syncs when already connected

2. **[ios/Aquavate/Aquavate/Views/HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift)**
   - Add `@State` properties for alert presentation
   - Add `.refreshable { }` modifier to ScrollView
   - Add alert modifiers for "bottle asleep" and error cases

3. **[ios/Aquavate/Aquavate/Views/SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift)**
   - Wrap connection controls (Scan/Connect/Disconnect) in `#if DEBUG`
   - Keep Device Info section visible in release (shows battery, calibration status, etc.)
   - Keep Device Commands section visible in release (Tare, Reset Daily, etc.)

## Implementation Details

### 1. RefreshResult Enum (BLEManager.swift)

```swift
enum RefreshResult {
    case synced(recordCount: Int)  // Successfully synced N records
    case alreadySynced             // Connected but no records to sync
    case bottleAsleep              // Scan timeout - no devices found
    case bluetoothOff              // Bluetooth not available
    case connectionFailed(String)  // Connection error with message
    case syncFailed(String)        // Sync error with message
}
```

### 2. Async Refresh Method (BLEManager.swift)

Flow:
1. Check Bluetooth availability → return `.bluetoothOff` if off
2. If already connected → skip to sync
3. Attempt connection using Combine publisher on `$connectionState`
4. If scan times out with no devices → return `.bottleAsleep`
5. Once connected, trigger sync and wait for completion
6. Return appropriate result

Key pattern - bridging callbacks to async/await:
```swift
func performRefresh() async -> RefreshResult {
    guard isBluetoothReady else { return .bluetoothOff }

    if !connectionState.isConnected {
        let connectResult = await attemptConnection()
        if case .failure(let result) = connectResult {
            return result
        }
    }

    return await performSyncOnly()
}
```

### 3. HomeView Changes

```swift
@State private var showBottleAsleepAlert = false
@State private var showErrorAlert = false
@State private var errorAlertMessage = ""

var body: some View {
    NavigationView {
        ScrollView {
            // ... existing content ...
        }
        .refreshable {
            await handleRefresh()
        }
    }
    .alert("Bottle is Asleep", isPresented: $showBottleAsleepAlert) {
        Button("OK", role: .cancel) { }
    } message: {
        Text("Tilt your bottle to wake it up, then pull down to try again.")
    }
    .alert("Sync Error", isPresented: $showErrorAlert) {
        Button("OK", role: .cancel) { }
    } message: {
        Text(errorAlertMessage)
    }
}
```

### 4. Connection Lifecycle (Updated 2026-01-18)

After pull-to-refresh, the connection behavior was updated for better UX:

**Original plan:** Auto-disconnect immediately after sync
**Implemented behavior:** Stay connected with 60-second idle timer

This change provides:
- Real-time updates while viewing the app (weight, level, daily total)
- Better user experience - see live data without re-pulling
- Battery conservation via idle timeout (disconnects after 60s of no refresh)
- Immediate disconnect when app goes to background (allows bottle to sleep)

**Implementation:**
```swift
// BLEManager.swift
private var idleDisconnectTimer: Timer?
private let idleDisconnectInterval: TimeInterval = 60.0

func startIdleDisconnectTimer() { ... }
func cancelIdleDisconnectTimer() { ... }
```

**AquavateApp.swift changes:**
- Removed auto-reconnect on foreground (user uses pull-to-refresh instead)
- Background disconnect remains immediate

### 5. Settings View Changes

Connection controls (Scan/Connect/Disconnect) wrapped in `#if DEBUG`:
- **Debug builds:** Full connection controls visible for testing
- **Release builds:** Cleaner UI - users just use pull-to-refresh

Device Info and Device Commands sections remain visible in all builds.

### 6. Edge Cases (Updated 2026-01-18)

| Scenario | Behavior |
|----------|----------|
| Bluetooth off | Show error: "Bluetooth is turned off" |
| Already connected | Sync, reset idle timer |
| Already syncing | Wait for current sync |
| No unsynced records | Stay connected (60s idle timer) |
| Scan timeout (no devices) | Show "bottle asleep" alert |
| Connection drops mid-sync | Show sync error |

## Verification (Updated 2026-01-18)

1. Pull down when disconnected, bottle awake → connects, syncs, stays connected 60s ✅
2. Pull down when disconnected, bottle asleep → shows "tilt to wake" alert after ~10s
3. Pull down when already connected → syncs, resets idle timer ✅
4. Pull down with Bluetooth off → shows Bluetooth error
5. Pull down with no unsynced records → stays connected (idle timer) ✅
6. Verify bottle returns to sleep after app disconnects
7. Verify connection controls hidden in release build, visible in debug build ✅

## Implementation Status: ✅ COMPLETE (2026-01-18)

All items implemented and tested on hardware. Key change from original plan: connection now stays open 60 seconds for real-time updates instead of disconnecting immediately after sync.
