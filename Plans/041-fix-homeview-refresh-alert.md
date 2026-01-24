# Fix: HomeView Refresh Alert Not Showing "Bottle is Asleep"

## Problem Summary

The HomeView's pull-to-refresh sometimes fails to show the "Bottle is Asleep" alert when the bottle cannot be found, even though HistoryView correctly shows this alert. The issue appears after extended use and is fixed by restarting the app.

## Root Cause Analysis

### The Bug: stopScanning() Guard Returns Early

In [BLEManager.swift:325-336](ios/Aquavate/Aquavate/Services/BLEManager.swift#L325-L336):

```swift
func stopScanning() {
    guard centralManager.isScanning else { return }  // BUG!
    // ...
    if connectionState == .scanning {
        connectionState = .disconnected  // Never executes when guard triggers
    }
}
```

**What happens:**
1. iOS stops BLE scanning externally (background transition, power saving, Bluetooth state change)
2. `centralManager.isScanning` becomes `false` without our code knowing
3. Our `connectionState` stays `.scanning` because `stopScanning()` returns early
4. Subsequent `startScanning()` calls fail guard at line 299: `guard connectionState == .disconnected`
5. No fresh scan starts, `discoveredDevices` isn't cleared
6. The `.bottleAsleep` condition at line 1797-1798 never matches
7. Returns `.connectionFailed("Connection timeout")` instead of `.bottleAsleep`
8. User sees generic error or no alert, not the "Bottle is Asleep" alert

### Why HistoryView Sometimes Works
- Used less frequently, less likely to have accumulated corrupted state
- Other lifecycle events (idle timer, background transitions) may reset state between uses

## Implementation Plan

### Change 1: Fix stopScanning() (Lines 325-336)

**Before:**
```swift
func stopScanning() {
    guard centralManager.isScanning else { return }

    logger.info("Stopping BLE scan")
    centralManager.stopScan()
    scanTimer?.invalidate()
    scanTimer = nil

    if connectionState == .scanning {
        connectionState = .disconnected
    }
}
```

**After:**
```swift
func stopScanning() {
    // Always clean up our state, even if CoreBluetooth stopped scanning externally
    scanTimer?.invalidate()
    scanTimer = nil

    // Only call stopScan if actually scanning (avoids CoreBluetooth warning)
    if centralManager.isScanning {
        logger.info("Stopping BLE scan")
        centralManager.stopScan()
    } else if connectionState == .scanning {
        logger.info("Cleaning up scanning state (CoreBluetooth already stopped)")
    }

    // Always reset connectionState if we were scanning
    if connectionState == .scanning {
        connectionState = .disconnected
    }
}
```

### Change 2: Add Defensive Recovery in startScanning() (Lines 292-302)

Add state corruption detection and recovery before the existing guard:

```swift
func startScanning() {
    guard isBluetoothReady else {
        errorMessage = "Bluetooth is not available"
        logger.warning("Cannot scan: Bluetooth not ready")
        return
    }

    // NEW: Defensive state recovery
    if connectionState == .scanning && !centralManager.isScanning {
        logger.warning("Detected corrupted scanning state - recovering")
        connectionState = .disconnected
        scanTimer?.invalidate()
        scanTimer = nil
    }

    guard connectionState == .disconnected else {
        logger.info("Already scanning or connected, ignoring scan request")
        return
    }
    // ... rest of function unchanged
```

### Change 3: Improve handleScanTimeout() Robustness (Lines 598-606)

**Before:**
```swift
private func handleScanTimeout() {
    if connectionState == .scanning {
        stopScanning()
        if discoveredDevices.isEmpty {
            errorMessage = "No Aquavate devices found"
            logger.warning("Scan timeout: no devices found")
        }
    }
}
```

**After:**
```swift
private func handleScanTimeout() {
    // Clean up even if state appears inconsistent
    if connectionState == .scanning || centralManager.isScanning {
        logger.info("Scan timeout - stopping scan")
        stopScanning()

        if discoveredDevices.isEmpty {
            errorMessage = "No Aquavate devices found"
            logger.warning("Scan timeout: no devices found")
        }
    }
}
```

### Change 4: Add Diagnostic Logging to attemptConnection() (Lines 1775-1812)

Add logging at key points to help diagnose future issues:

```swift
private func attemptConnection() async -> ConnectionResult {
    if connectionState.isConnected {
        logger.info("attemptConnection: already connected")
        return .success
    }

    // NEW: Log state before starting
    logger.info("attemptConnection: starting (state=\(String(describing: self.connectionState)), isScanning=\(self.centralManager.isScanning), devices=\(self.discoveredDevices.count))")

    startScanning()

    // ... polling loop unchanged ...

    // NEW: Log state on timeout
    logger.warning("attemptConnection: timeout (state=\(String(describing: self.connectionState)), isScanning=\(self.centralManager.isScanning), devices=\(self.discoveredDevices.count))")
    stopScanning()
    return .failure(.connectionFailed("Connection timeout"))
}
```

### Change 5: Clean Up Scanning State on Bluetooth Transitions (Lines 668-670)

Add timer cleanup for `.resetting` case in `centralManagerDidUpdateState`:

```swift
case .resetting:
    logger.warning("Bluetooth resetting")
    isBluetoothReady = false
    // NEW: Clean up timers during reset
    scanTimer?.invalidate()
    scanTimer = nil
    if connectionState == .scanning {
        connectionState = .disconnected
    }
```

## Files to Modify

| File | Location |
|------|----------|
| [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) | Lines 292-302, 325-336, 598-606, 668-670, 1775-1812 |

## Verification Plan

### Test 1: Basic Refresh (Regression)
1. Launch app with bottle awake
2. Pull to refresh on HomeView
3. **Expected:** Connects, syncs successfully

### Test 2: Bottle Asleep Alert
1. Launch app with bottle asleep (in deep sleep)
2. Pull to refresh on HomeView
3. **Expected:** Shows "Bottle is Asleep" alert after ~10 seconds

### Test 3: State Recovery After Bluetooth Toggle
1. Connect to bottle successfully
2. Turn Bluetooth off in iOS Settings
3. Turn Bluetooth back on
4. Pull to refresh on HomeView (bottle asleep)
5. **Expected:** Shows "Bottle is Asleep" alert (not generic timeout)

### Test 4: Extended Use Simulation
1. Connect/disconnect multiple times
2. Background/foreground app several times
3. Pull to refresh with bottle asleep
4. **Expected:** Consistently shows "Bottle is Asleep" alert

### Test 5: HomeView/HistoryView Parity
1. With bottle asleep, refresh on HomeView - note result
2. Refresh on HistoryView - note result
3. **Expected:** Both show identical "Bottle is Asleep" alert
