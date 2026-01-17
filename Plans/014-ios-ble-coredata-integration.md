# iOS BLE Integration Implementation Plan

## Context

The firmware BLE integration (Phase 3A-3D) is complete on the `ble-integration` branch. The iOS app has a placeholder UI with mock data. This plan implements Phase 4: iOS BLE client and CoreData persistence to enable end-to-end communication between the app and the ESP32 firmware.

## Current State

### Firmware (Ready)
- **Branch:** `ble-integration`
- **BLE Services:** Device Info (0x180A), Battery (0x180F), Aquavate Custom Service
- **Characteristics:** Current State, Bottle Config, Sync Control, Drink Data, Command
- **Protocol:** Binary packed structs, pull-based chunked sync (up to 600 records)
- **Status:** Fully implemented, tested with LightBlue iOS app

### iOS App (Placeholder UI)
- **Structure:** SwiftUI app with Home, History, Settings views
- **Models:** Mock `DrinkRecord` and `Bottle` structs (no persistence)
- **Components:** CircularProgressView, DrinkListItem
- **Gaps:** No BLE manager, no CoreData, no binary struct parsing, no background mode

## Implementation Approach Options

I've identified two distinct approaches with different trade-offs. Please review and select your preference.

### Option A: BLE-First (Recommended)

**Order:**
1. Implement BLE manager with device scanning/connection
2. Add binary struct parsing for all 5 characteristics
3. Implement real-time Current State updates (weight, battery, daily total)
4. Test live data flow from firmware → UI using mock data models
5. Implement CoreData schema and persistence
6. Replace mock data with CoreData in all views
7. Implement drink sync protocol (Sync Control + Drink Data)
8. Add command execution (tare, reset, calibration)
9. Implement time sync on first connection
10. Add background mode support

**Pros:**
- See live firmware data in UI immediately (motivating progress)
- Test BLE protocol early (catch firmware/iOS integration issues)
- Incremental complexity (BLE without persistence first)
- Can demo real-time weight updates quickly

**Cons:**
- Temporary mock data coexists with BLE (minor cleanup later)
- CoreData schema design happens after seeing data flow (less upfront planning)

**Timeline estimate:** Not providing time estimates per project policy

### Option B: Data-First

**Order:**
1. Design and implement CoreData schema (Bottle, DrinkRecord, DailySummary)
2. Replace all mock data with CoreData + sample seed data
3. Update all views to use @FetchRequest and CoreData queries
4. Implement BLE manager with device scanning/connection
5. Add binary struct parsing
6. Connect BLE → CoreData (Current State, sync protocol)
7. Implement commands and time sync
8. Add background mode support

**Pros:**
- CoreData schema designed upfront with full context
- No temporary mock data coexisting with real data
- Data layer complete before adding BLE complexity
- Clean separation of concerns (storage, then communication)

**Cons:**
- No visual progress until BLE layer complete
- Can't test firmware integration until late in process
- Risk of CoreData schema redesign after seeing BLE data flow

**Timeline estimate:** Not providing time estimates per project policy

## My Recommendation

**Option A (BLE-First)** for these reasons:

1. **Risk mitigation:** Test the firmware-iOS BLE protocol early. The binary struct parsing and chunked sync is complex - better to catch issues early.

2. **Motivation:** You'll see live weight readings updating in the UI within the first few implementation steps, providing immediate validation.

3. **Iterative discovery:** CoreData schema benefits from seeing the actual data flow first. For example, we might discover we need to track sync state differently after implementing the protocol.

4. **Debugging advantages:** Can test BLE characteristics individually without persistence complexity.

5. **Phase 3E dependency:** You need end-to-end BLE working to return to firmware power optimization. Getting BLE functional quickly unblocks that work.

## Design Decisions (From User Input)

✅ **Approach:** BLE-First (Option A)
✅ **Device Support:** Single device per app (MVP scope)
✅ **HealthKit:** Deferred to Phase 5
⏳ **Duplicate Handling:** Pending decision (see analysis below)

### Duplicate Record Handling - Detailed Analysis

**Scenario:** iOS syncing 142 records, receives chunks 0-3 (60 records), then disconnects. On reconnection, firmware resends all 142 records starting from chunk 0.

#### Option 1: Timestamp-based Deduplication (Recommended)

**Implementation:**
```swift
// CoreData DrinkRecord entity
@NSManaged public var timestamp: Date
@NSManaged public var bottleId: UUID

// Add unique constraint in CoreData model
uniquenessConstraints: [["timestamp", "bottleId"]]
```

**How it works:**
- When iOS receives chunk 0 again, attempts to insert records with same timestamps
- CoreData unique constraint prevents duplicate inserts automatically
- NSBatchInsertRequest ignores constraint violations (silent skip)
- Or use NSManagedObjectContext mergePolicy: `NSMergeByPropertyObjectTrumpMergePolicy`

**Pros:**
- **Automatic protection:** CoreData enforces uniqueness, no manual checking needed
- **Simple code:** Insert all records, database handles deduplication
- **Robust:** Works even if sync protocol has bugs or edge cases
- **Efficient:** Database-level constraint is fast (indexed lookup)
- **Clean data:** Guaranteed no duplicates in storage

**Cons:**
- **Constraint limitation:** Assumes timestamps are unique per bottle (they should be for this use case)
- **Silent failures:** If genuinely different records have same timestamp (firmware bug), second is silently rejected
- **Migration complexity:** If you later want to change uniqueness constraint, requires CoreData migration

**Example code:**
```swift
func insertDrinkRecords(_ records: [BLE_DrinkRecord]) {
    context.mergePolicy = NSMergeByPropertyObjectTrumpMergePolicy
    for record in records {
        let drink = DrinkRecord(context: context)
        drink.timestamp = Date(timeIntervalSince1970: TimeInterval(record.timestamp))
        drink.amountMl = Int16(record.amount_ml)
        // CoreData automatically skips if timestamp already exists
    }
    try? context.save() // Constraint violations don't throw with merge policy
}
```

---

#### Option 2: Accept Duplicates, Clean Up Later

**Implementation:**
```swift
// Insert all records without constraints
func insertDrinkRecords(_ records: [BLE_DrinkRecord]) {
    for record in records {
        let drink = DrinkRecord(context: context)
        drink.timestamp = Date(timeIntervalSince1970: TimeInterval(record.timestamp))
        drink.amountMl = Int16(record.amount_ml)
        // Always inserts, even if duplicate
    }
    try? context.save()
}

// Periodic cleanup (run after sync complete)
func deduplicateRecords() {
    let request = NSFetchRequest<DrinkRecord>(entityName: "DrinkRecord")
    request.sortDescriptors = [NSSortDescriptor(key: "timestamp", ascending: true)]
    let allRecords = try? context.fetch(request)

    var seen = Set<Date>()
    var toDelete: [DrinkRecord] = []

    for record in allRecords ?? [] {
        if seen.contains(record.timestamp) {
            toDelete.append(record)
        } else {
            seen.insert(record.timestamp)
        }
    }

    toDelete.forEach { context.delete($0) }
    try? context.save()
}
```

**Pros:**
- **Flexible:** Can handle complex deduplication logic (e.g., timestamp within 1 second tolerance)
- **Debuggable:** Can inspect duplicate records before deleting (useful during development)
- **No constraints:** Easier to change data model later without migrations
- **Handles edge cases:** Can merge duplicates with different fields (keep most recent)

**Cons:**
- **Manual cleanup required:** Must remember to call deduplication after every sync
- **Temporary duplicates:** UI may briefly show duplicate drinks if cleanup delayed
- **More code:** Additional deduplication logic to write, test, and maintain
- **Performance:** Fetching all records to deduplicate is slower than constraint check
- **Bug risk:** If cleanup code has bugs, duplicates persist in database
- **Memory usage:** Temporarily stores duplicate records before cleanup

**Example scenario impact:**
- 142 records, 60 duplicates on reconnection
- Option 1: 60 records silently skipped during insert (instant)
- Option 2: 60 duplicate records inserted, then must fetch all 202 records, identify 60 dupes, delete them (slower)

---

### My Recommendation: **Option 1 (Timestamp-based Deduplication)**

**Rationale:**

1. **Firmware guarantees unique timestamps:** Each drink event has a distinct Unix timestamp. The firmware doesn't generate multiple events in the same second (drink detection has ~5s intervals).

2. **Simpler = more reliable:** CoreData's built-in constraints are battle-tested. Less custom code means fewer bugs.

3. **Better UX:** No temporary duplicate records visible in UI during cleanup windows.

4. **Database best practice:** Uniqueness constraints are standard for preventing duplicates (used in production apps everywhere).

5. **Forward compatible:** If firmware later adds sub-second timestamps or event IDs, can add to constraint (requires migration, but clean).

**Trade-off accepted:** If firmware has a bug and generates two events with the same timestamp, the second is silently dropped. This is acceptable because:
- It's a firmware bug that should be fixed, not worked around
- The alternative (allowing duplicates) pollutes user's data
- Logs can detect this if needed (log constraint violations during development)

✅ **Duplicate Handling:** Timestamp-based deduplication (Option 1)
✅ **Auto-reconnect:** On foreground (automatic)
✅ **Time Sync:** On every connection

---

## Implementation Plan - Phase 4: iOS BLE & CoreData

### File Structure

**New Files to Create:**

```
ios/Aquavate/Aquavate/
├── Services/
│   ├── BLEManager.swift              # CoreBluetooth manager (~600 lines)
│   ├── BLEStructs.swift              # Binary struct definitions
│   └── BLEConstants.swift            # UUIDs and configuration
├── CoreData/
│   ├── Aquavate.xcdatamodeld         # CoreData schema
│   ├── PersistenceController.swift   # CoreData stack
│   └── CoreDataExtensions.swift      # Entity helpers
├── ViewModels/
│   ├── HomeViewModel.swift           # Home business logic
│   ├── HistoryViewModel.swift        # History business logic
│   └── SettingsViewModel.swift       # Settings business logic
└── Utils/
    ├── BinaryParser.swift            # withUnsafeBytes helpers
    └── DateHelpers.swift             # Date formatting
```

**Files to Modify:**
- [AquavateApp.swift](ios/Aquavate/Aquavate/AquavateApp.swift) - Add environment objects
- [Views/HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift) - Replace mock data
- [Views/HistoryView.swift](ios/Aquavate/Aquavate/Views/HistoryView.swift) - Replace mock data
- [Views/SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift) - Replace mock data
- [Models/DrinkRecord.swift](ios/Aquavate/Aquavate/Models/DrinkRecord.swift) - Add CoreData conversion
- [Models/Bottle.swift](ios/Aquavate/Aquavate/Models/Bottle.swift) - Add CoreData conversion

### CoreData Schema

#### Entity: CDDrinkRecord
| Attribute | Type | Constraints |
|-----------|------|-------------|
| id | UUID | Primary key |
| timestamp | Date | Unique with bottleId, Indexed |
| amountMl | Int16 | Required |
| bottleLevelMl | Int16 | Required |
| drinkType | Int16 | Required (0=gulp, 1=pour) |
| syncedToHealth | Bool | Default false |
| bottleId | UUID | Foreign key, Indexed |

**Unique Constraint:** `[timestamp, bottleId]` prevents duplicate sync

#### Entity: CDBottle
| Attribute | Type | Notes |
|-----------|------|-------|
| id | UUID | Primary key |
| name | String | User-editable |
| capacityMl | Int16 | 100-5000 range |
| tareWeightGrams | Int32 | Empty bottle weight |
| scaleFactor | Float | ADC calibration |
| dailyGoalMl | Int16 | 500-10000 range |
| peripheralIdentifier | String? | CBPeripheral UUID for reconnection |
| lastSyncDate | Date? | Last successful sync |
| batteryPercent | Int16 | 0-100 |
| isCalibrated | Bool | Calibration status |

**Cascade delete:** Deleting bottle deletes all drink records

### BLE Manager Architecture

**Key Components:**

1. **Connection State Management**
   - States: disconnected, scanning, connecting, discovering, connected
   - Auto-reconnect on foreground using saved peripheral identifier
   - Background state restoration for seamless reconnection

2. **Binary Struct Parsing**
   - Pattern: `data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) in ... }`
   - Parse firmware's packed structs (Current State: 14 bytes, Drink Data: variable)
   - Handle endianness (firmware is little-endian, matches iOS)

3. **Sync State Machine**
   ```
   IDLE → REQUESTING → RECEIVING → COMPLETE → IDLE
   ```
   - Write START to Sync Control (command=1, count=unsyncedCount)
   - Receive chunks via Drink Data notifications (20 records per chunk)
   - Send ACK after each chunk (command=2)
   - Mark records as synced in firmware after complete transfer

4. **@Published Properties for Reactive UI**
   - `connectionState`, `currentWeight`, `bottleLevel`, `dailyTotal`
   - `batteryPercent`, `isCalibrated`, `isStable`, `unsyncedCount`
   - `syncProgress` (idle, syncing, complete, failed)
   - SwiftUI views auto-update on changes

### Implementation Phases

#### Phase 4.1: BLE Foundation

**Deliverables:**
- BLE scanning and device discovery ("Aquavate-XXXX" filter)
- Connection establishment with peripheral persistence
- Service/characteristic discovery (5 Aquavate + Battery)
- Background mode enabled (`bluetooth-central` in Info.plist)

**Testing:**
- Connect to firmware via LightBlue app first (verify advertising)
- App discovers device within 5 seconds
- All characteristics discovered
- Connection persists when app backgrounded

**Critical Files:**
- [Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) - CBCentralManager delegate
- [Services/BLEConstants.swift](ios/Aquavate/Aquavate/Services/BLEConstants.swift) - UUIDs
- Info.plist - Add `UIBackgroundModes` array with `bluetooth-central`

#### Phase 4.2: Current State & Battery Notifications

**Deliverables:**
- Parse 14-byte Current State characteristic
- Subscribe to Current State notifications (real-time weight/battery/daily total)
- Parse Battery Level characteristic (1 byte)
- Update @Published properties to trigger UI updates

**Testing:**
- Connect app, verify immediate Current State received
- Pick up bottle (stable flag changes)
- Take drink (bottle level and daily total update in <1s)
- Background app → take drink → foreground → verify updated

**Critical Files:**
- [Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) - Add parsing methods
- [Services/BLEStructs.swift](ios/Aquavate/Aquavate/Services/BLEStructs.swift) - BLE_CurrentState struct
- [Utils/BinaryParser.swift](ios/Aquavate/Aquavate/Utils/BinaryParser.swift) - withUnsafeBytes helpers

#### Phase 4.3: CoreData Schema & Persistence

**Deliverables:**
- Create Aquavate.xcdatamodeld with 3 entities (CDDrinkRecord, CDBottle, CDDailySummary)
- Set up PersistenceController with preview data
- Add timestamp+bottleId unique constraint to CDDrinkRecord
- Inject CoreData context into SwiftUI environment

**Testing:**
- Insert 100 test drink records manually
- Attempt duplicate timestamp insert (verify constraint prevents)
- Query today's drinks (verify @FetchRequest works)
- Restart app (verify data persists)

**Critical Files:**
- [CoreData/Aquavate.xcdatamodeld](ios/Aquavate/Aquavate/CoreData/Aquavate.xcdatamodeld) - Schema definition
- [CoreData/PersistenceController.swift](ios/Aquavate/Aquavate/CoreData/PersistenceController.swift) - Stack setup
- [AquavateApp.swift](ios/Aquavate/Aquavate/AquavateApp.swift) - Add `.environment(\.managedObjectContext, ...)`

#### Phase 4.4: Drink Sync Protocol

**Deliverables:**
- Implement sync state machine (START → ACK → COMPLETE)
- Parse variable-size Drink Data chunks (up to 206 bytes)
- Auto-trigger sync when `unsyncedCount > 0` on connection
- Save synced records to CoreData with timestamp deduplication
- Mark records as synced on firmware after successful transfer

**Testing:**
- Add 100 drink records via firmware (USB serial or actual usage)
- Connect app, verify automatic sync starts
- Verify all 100 records saved to CoreData (no duplicates)
- Disconnect at chunk 3, reconnect, verify resumes correctly
- Background sync: app backgrounded → pick up bottle → verify records synced

**Critical Files:**
- [Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) - Sync state machine
- [Services/BLEStructs.swift](ios/Aquavate/Aquavate/Services/BLEStructs.swift) - BLE_DrinkRecord, BLE_DrinkDataChunk
- [CoreData/PersistenceController.swift](ios/Aquavate/Aquavate/CoreData/PersistenceController.swift) - Save with deduplication

**Sync Protocol Details:**
1. iOS reads Current State → sees `unsyncedCount=142`
2. iOS writes Sync Control: `{command=START, count=142, chunk_size=20}`
3. Firmware sends first chunk via Drink Data notification (chunk 0/8, 20 records)
4. iOS parses chunk, appends to buffer, writes ACK: `{command=ACK}`
5. Firmware sends chunk 1/8, iOS ACKs, repeat...
6. After chunk 7/8 (last 2 records), iOS writes final ACK
7. iOS saves all 142 records to CoreData (timestamp constraint prevents duplicates)
8. Firmware marks records as synced (flags |= 0x01)
9. Current State notifies: `unsyncedCount=0`

#### Phase 4.5: Commands & Time Sync

**Deliverables:**
- Write operations to Command characteristic (4-byte BLE_Command struct)
- Implement commands: TARE_NOW (0x01), RESET_DAILY (0x05), CLEAR_HISTORY (0x06)
- Auto-sync device time on every connection (critical for accurate timestamps)
- Read/write Bottle Config characteristic (12-byte BLE_BottleConfig)

**Testing:**
- Connect app
- Verify time syncs automatically (check `time_valid` flag in Current State)
- Send TARE command via Settings screen → verify weight resets to 0g
- Send RESET_DAILY → verify daily total resets to 0ml
- Write new bottle capacity → disconnect → reconnect → verify persists

**Critical Files:**
- [Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) - Add command methods
- [Services/BLEStructs.swift](ios/Aquavate/Aquavate/Services/BLEStructs.swift) - BLE_Command, BLE_BottleConfig
- [Views/SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift) - Add command buttons

**Time Sync Implementation:**
```swift
func syncTimeOnConnection() {
    let currentTimestamp = UInt32(Date().timeIntervalSince1970)
    // Write to Command characteristic with SET_TIME opcode
    // Firmware sets g_time_valid = true in storage
}
```

#### Phase 4.6: Background Mode & Polish

**Deliverables:**
- Implement CBCentralManager state restoration
- Save peripheral identifier to UserDefaults for auto-reconnect
- Auto-reconnect logic on app foreground
- Error handling UI (connection errors, sync failures)
- Connection state indicators in all views

**Testing:**
- Background app → pick up bottle → wait 30s → foreground app → verify synced
- Force-quit app → pick up bottle → verify no sync (expected iOS behavior)
- Force-quit app → open app → verify reconnects within 5s
- Airplane mode → verify graceful error handling
- Background sync 5× in 1 hour → check battery usage (<5% drain)

**Critical Files:**
- [Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) - State restoration
- Info.plist - `NSBluetoothAlwaysUsageDescription`
- [Views/HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift) - Connection status indicator

**State Restoration Setup:**
```swift
let options: [String: Any] = [
    CBCentralManagerOptionRestoreIdentifierKey: "com.aquavate.centralmanager",
    CBCentralManagerOptionShowPowerAlertKey: true
]
centralManager = CBCentralManager(delegate: self, queue: nil, options: options)
```

---

#### Phase 4.7: iOS Calibration Wizard (NEW - 2026-01-17)

**Context:**
- Firmware standalone calibration removed to save IRAM (1KB savings)
- iOS app now responsible for calibration UI and workflow
- Firmware provides core math functions via Bottle Config characteristic

**Deliverables:**
- CalibrationWizardView with multi-step UI flow
- Weight measurement helpers using BLE Current State
- Write calibration results to firmware via Bottle Config characteristic
- Calibration validation and error handling

**User Flow:**
```
1. User taps "Calibrate" in Settings
2. Welcome screen: "You'll need an empty bottle and tap water"
3. Step 1: Place empty bottle → "Tap when ready" → measure 10s → show ADC value
4. Step 2: Fill to 830ml line → "Tap when ready" → measure 10s → show ADC value
5. Calculate scale factor (firmware function)
6. Write calibration to firmware (BLE Bottle Config)
7. Success screen: "Calibration complete! ✓"
```

**BLE Integration:**
```swift
// Read current weight (ADC) during calibration
let currentState = bleManager.currentWeightG  // From Current State notifications

// After both measurements
let scaleFactor = calculateScaleFactor(emptyADC: emptyReading,
                                       fullADC: fullReading,
                                       waterVolumeMl: 830.0)

// Write to firmware
bleManager.writeBottleConfig(
    scaleFactor: scaleFactor,
    tareWeight: emptyReading,
    capacity: 830,
    goal: 2400  // Default daily goal
)
```

**Testing:**
- Complete calibration flow with real hardware
- Verify scale factor calculation matches firmware
- Verify calibration persists after disconnect/reconnect
- Test error cases: disconnection mid-calibration, unstable readings
- Verify "calibrated" flag updates in Current State

**Critical Files:**
- [Views/CalibrationWizardView.swift](ios/Aquavate/Aquavate/Views/CalibrationWizardView.swift) - New file
- [Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) - Add `writeBottleConfig()` method
- [Views/SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift) - Add "Calibrate" button

**Note:** This phase can be implemented after Phase 4.6 or deferred to Phase 5 depending on priorities. Bottle can still be used with manual calibration via USB serial commands in standalone mode.

---

### View Integration Pattern

**HomeView.swift Example:**

```swift
struct HomeView: View {
    @EnvironmentObject var bleManager: BLEManager
    @Environment(\.managedObjectContext) private var viewContext

    @FetchRequest(
        entity: CDDrinkRecord.entity(),
        sortDescriptors: [NSSortDescriptor(keyPath: \CDDrinkRecord.timestamp, ascending: false)],
        predicate: NSPredicate(format: "timestamp >= %@", Calendar.current.startOfDay(for: Date()) as CVarArg)
    ) var todaysDrinks: FetchedResults<CDDrinkRecord>

    var body: some View {
        VStack {
            // Connection indicator
            HStack {
                Circle()
                    .fill(bleManager.connectionState == .connected ? Color.green : Color.gray)
                    .frame(width: 8, height: 8)
                Text(connectionStatusText)
            }

            // Real-time bottle level from BLE
            CircularProgressView(
                current: bleManager.bottleLevel,
                total: 750,
                color: .blue
            )

            // Daily total from BLE Current State
            Text("Today: \(bleManager.dailyTotal)ml / 2000ml")

            // Recent drinks from CoreData
            ForEach(todaysDrinks.prefix(5)) { drink in
                DrinkListItem(drink: drink.toDrinkRecord())
            }
        }
    }
}
```

### Binary Struct Parsing Examples

**Current State (14 bytes):**

```swift
func handleCurrentStateUpdate(_ data: Data) {
    guard data.count == 14 else { return }

    data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) in
        guard let baseAddress = ptr.baseAddress else { return }

        let timestamp = baseAddress.load(as: UInt32.self)
        let weightG = baseAddress.load(fromByteOffset: 4, as: Int16.self)
        let levelMl = baseAddress.load(fromByteOffset: 6, as: UInt16.self)
        let dailyMl = baseAddress.load(fromByteOffset: 8, as: UInt16.self)
        let battery = baseAddress.load(fromByteOffset: 10, as: UInt8.self)
        let flags = baseAddress.load(fromByteOffset: 11, as: UInt8.self)
        let unsynced = baseAddress.load(fromByteOffset: 12, as: UInt16.self)

        DispatchQueue.main.async {
            self.currentWeight = Int(weightG)
            self.bottleLevel = Int(levelMl)
            self.dailyTotal = Int(dailyMl)
            self.batteryPercent = Int(battery)
            self.isTimeValid = (flags & 0x01) != 0
            self.isCalibrated = (flags & 0x02) != 0
            self.isStable = (flags & 0x04) != 0
            self.unsyncedCount = Int(unsynced)
        }
    }
}
```

**Drink Data Chunk (variable size):**

```swift
func handleDrinkDataUpdate(_ data: Data) {
    guard data.count >= 6 else { return }

    data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) in
        guard let baseAddress = ptr.baseAddress else { return }

        let chunkIndex = baseAddress.load(as: UInt16.self)
        let totalChunks = baseAddress.load(fromByteOffset: 2, as: UInt16.self)
        let recordCount = baseAddress.load(fromByteOffset: 4, as: UInt8.self)

        // Parse individual records (10 bytes each)
        for i in 0..<Int(recordCount) {
            let offset = 6 + (i * 10)
            guard data.count >= offset + 10 else { break }

            let timestamp = baseAddress.load(fromByteOffset: offset, as: UInt32.self)
            let amountMl = baseAddress.load(fromByteOffset: offset + 4, as: Int16.self)
            let levelMl = baseAddress.load(fromByteOffset: offset + 6, as: UInt16.self)
            let type = baseAddress.load(fromByteOffset: offset + 8, as: UInt8.self)

            // Append to sync buffer
            syncBuffer.append(BLE_DrinkRecord(
                timestamp: timestamp,
                amountMl: amountMl,
                bottleLevelMl: levelMl,
                type: type
            ))
        }

        // Send ACK or complete sync
        if chunkIndex + 1 < totalChunks {
            sendSyncAck()
        } else {
            completeSyncTransfer()
        }
    }
}
```

### CoreData Deduplication Strategy

**Automatic via Unique Constraint:**

```swift
// In Aquavate.xcdatamodeld, set unique constraint on CDDrinkRecord:
// uniquenessConstraints = [["timestamp", "bottleId"]]

func saveDrinkRecords(_ bleRecords: [BLE_DrinkRecord], context: NSManagedObjectContext) {
    // Set merge policy to handle constraint violations
    context.mergePolicy = NSMergeByPropertyObjectTrumpMergePolicy

    for bleRecord in bleRecords {
        let record = CDDrinkRecord(context: context)
        record.id = UUID()
        record.timestamp = Date(timeIntervalSince1970: TimeInterval(bleRecord.timestamp))
        record.amountMl = bleRecord.amountMl
        record.bottleLevelMl = Int16(bleRecord.bottleLevelMl)
        record.drinkType = Int16(bleRecord.type)
        record.syncedToHealth = false
        record.bottleId = getCurrentBottleId()
    }

    do {
        try context.save()
        print("[CoreData] Saved \(bleRecords.count) records")
    } catch {
        print("[CoreData] Save error: \(error)")
        context.rollback()
    }
}
```

**Rationale:**
- Database enforces uniqueness automatically (no manual checking)
- Duplicate timestamps silently skipped (merge policy)
- Handles mid-sync disconnection gracefully (partial records saved)
- Firmware guarantees unique timestamps (drink detection ~5s intervals)

### Testing Strategy

**Phase-by-Phase Testing:**

| Phase | Test Method | Success Criteria |
|-------|-------------|------------------|
| 4.1 | LightBlue app, Xcode debugger | All characteristics discovered |
| 4.2 | Firmware serial monitor, live drinks | UI updates <1s after firmware event |
| 4.3 | Manual CoreData inserts | Unique constraint prevents duplicates |
| 4.4 | 100+ drink records sync | All records saved, no duplicates |
| 4.5 | Command buttons in Settings | Firmware responds to commands |
| 4.6 | Background app testing (real device) | Sync completes in <10s background |

**End-to-End Integration Test:**

1. Morning: Background app → pick up bottle → verify auto-sync
2. Drink 200ml → verify HomeView updates in real-time
3. Check History → verify drink in timeline with correct timestamp
4. Afternoon: Force-quit app → take 3 drinks → open app → verify all synced
5. Evening: Daily total matches firmware display ±5ml

### Known Issues & Mitigations

**Issue 1: iOS Background Execution Limit (~10 seconds)**
- **Mitigation:** Chunk size limited to 20 records (processes <5s), sync resumes on reconnection

**Issue 2: Force-Quit Disables Background Reconnection**
- **Mitigation:** UI shows "Last synced X minutes ago" warning, acceptable for MVP

**Issue 3: Firmware Time Drift (No RTC)**
- **Mitigation:** Sync time on every connection, not just first pairing

**Issue 4: Circular Buffer Overflow (600 records)**
- **Mitigation:** 600 records = ~2 weeks heavy use, UI warning if unsyncedCount > 500

### Critical Implementation Files (Priority Order)

1. **[Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)** - Core BLE logic (~600 lines)
2. **[CoreData/Aquavate.xcdatamodeld](ios/Aquavate/Aquavate/CoreData/Aquavate.xcdatamodeld)** - Schema definition
3. **[Services/BLEStructs.swift](ios/Aquavate/Aquavate/Services/BLEStructs.swift)** - Binary struct definitions (must match firmware exactly)
4. **[CoreData/PersistenceController.swift](ios/Aquavate/Aquavate/CoreData/PersistenceController.swift)** - CoreData stack
5. **[AquavateApp.swift](ios/Aquavate/Aquavate/AquavateApp.swift)** - Environment setup

---

## Verification & Acceptance Criteria

### Phase 4 Complete When:

- [ ] App discovers and connects to firmware automatically
- [ ] Real-time bottle level updates in HomeView (<1s latency)
- [ ] Drink history syncs automatically on connection (100+ records)
- [ ] CoreData prevents duplicate records (timestamp constraint works)
- [ ] Commands work (TARE, RESET_DAILY tested)
- [ ] Time syncs on every connection (time_valid flag set)
- [ ] Background sync works (app backgrounded → pick up bottle → data synced)
- [ ] Connection state visible in UI (connected/scanning/disconnected)
- [ ] Daily total matches firmware display ±10ml
- [ ] Battery level displays correctly
- [ ] All 6 core phases tested and verified (4.1-4.6)

**Phase 4.7 (iOS Calibration) - Optional for Phase 4:**
- [ ] Calibration wizard UI complete with multi-step flow
- [ ] Calibration writes to firmware via Bottle Config
- [ ] Calibrated flag updates in Current State after calibration
- [ ] Error handling for disconnection during calibration

### Post-Phase 4 Handoff:

After Phase 4 complete:
1. Merge `ble-integration` firmware branch to `master`
2. Return to Phase 3E: Firmware power optimization (connection parameter tuning, graceful disconnect UI)
3. End-to-end power testing with iOS app (measure battery drain with real usage)

### Future Work (Phase 5 - Deferred):

- HealthKit integration (sync drink records to Apple Health)
- Watch app (view-only, syncs via iPhone)
- Multi-bottle support
- Notification reminders for daily goal

---

## Summary

This plan implements iOS BLE client using a **BLE-First approach**: establish communication first, then add persistence. The implementation follows 6 sequential phases over ~10 days, with each phase tested independently before proceeding.

**Key Technical Decisions:**
- CoreData unique constraint for automatic deduplication
- Pull-based chunked sync (20 records per chunk)
- Auto-reconnect on foreground using saved peripheral ID
- Time sync on every connection (not just first pair)
- Background mode with state restoration
- Single device per app (MVP scope)

**Critical Success Factors:**
- Binary struct parsing must exactly match firmware (use withUnsafeBytes)
- Sync state machine must handle disconnection gracefully
- CoreData merge policy handles constraint violations silently
- Background execution stays within iOS 10-second limit

The firmware is ready and waiting. Let's build the iOS app to connect them!
