# Fix: Foreground Auto-Reconnection to Bottle

## Context

When the iOS app is in the **background**, it can automatically receive drink updates from the bottle because it uses `CBCentralManager.connect()` — a **persistent, indefinite connection request**. iOS keeps this request alive and will auto-connect whenever the bottle wakes up and advertises, even hours later.

When the app is in the **foreground** and disconnected, it uses a completely different strategy: a **5-second scan burst** (`performForegroundScanBurst()`). If the bottle isn't advertising during those 5 seconds, the scan times out silently and nothing else happens. The user must manually pull-to-refresh to try again.

**The root cause is a design asymmetry, not an iOS limitation.** `CBCentralManager.connect()` works identically in foreground and background — it's not a background-only API. The current code just chose not to use it in the foreground.

## The Two Strategies Compared

| | Background | Foreground |
|---|---|---|
| **Method** | `centralManager.connect(peripheral)` | `centralManager.scanForPeripherals()` |
| **Duration** | Persistent/indefinite | 5 seconds then gives up |
| **Behaviour** | iOS auto-connects whenever bottle advertises | Only catches bottle if it's already advertising |
| **After timeout** | N/A (never times out) | User must manually pull-to-refresh |

## Proposed Fix

**Use `centralManager.connect()` in foreground too**, for the known peripheral. This is the standard CoreBluetooth pattern for "connect to a known device whenever it becomes available."

### Changes to `BLEManager.swift`

1. **Rename `requestBackgroundReconnection` → `requestAutoReconnection`** (it's not background-specific)

2. **In `appDidBecomeActive()`**: Instead of (or in addition to) a scan burst, call `requestAutoReconnection()` for the known peripheral. This sets up a persistent connection request that will fire whenever the bottle advertises.

3. **In `appDidEnterBackground()`**: Cancel any foreground auto-reconnection before setting up the background one (to avoid duplicate connect requests).

4. **On manual disconnect**: Cancel any pending auto-reconnection (already handled by `cancelBackgroundReconnection()`, just rename it).

5. **On successful connection**: Clear the pending reconnection state (already handled at line 840-842).

### Key Files
- [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) — All changes are in this single file

### Detailed Code Changes

**a) Rename for clarity:**
- `requestBackgroundReconnection()` → `requestAutoReconnection()`
- `cancelBackgroundReconnection()` → `cancelAutoReconnection()`
- `pendingBackgroundReconnectPeripheral` → `pendingAutoReconnectPeripheral`

**b) Modify `appDidBecomeActive()` (line 503):**
- After the existing guards, retrieve the known peripheral from `UserDefaults`
- Call `requestAutoReconnection(to: peripheral)` for it
- Keep the scan burst as a complementary fast-path (it can discover the device faster if it's already advertising)
- OR replace the scan burst entirely with `connect()` — simpler but slightly slower initial connection if bottle is already advertising

**c) Modify `performForegroundScanBurst()` (line 445):**
- Optionally remove the scan burst entirely since `connect()` covers the same case
- OR keep it as a "fast path" for immediate discovery (scan is faster than connect for already-advertising devices), but ensure it doesn't cancel the persistent connect request

**d) Modify `handleScanBurstTimeout()` (line 488):**
- When scan burst times out, set `connectionState = .disconnected` as today (silent waiting, no new state needed)

**e) Clean up on disconnect (line 870):**
- The existing logic already handles re-requesting reconnection after unexpected disconnect — just rename the variables

## UX Decision

Silent auto-connect: Show `connectionState = .disconnected` as today. When bottle advertises, connection just happens automatically. No UI changes needed.

## Verification

1. Build iOS app: `cd ios/Aquavate && xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`
2. Test scenario: Open app → wait for scan burst to timeout → pick up bottle → verify auto-connect happens without pull-to-refresh
3. Test background still works: Connect → background app → wake bottle → verify background sync
4. Test manual disconnect: Disconnect via Settings → verify no auto-reconnect attempts
