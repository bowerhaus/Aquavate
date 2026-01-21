# Plan 027: Bidirectional Drink Record Sync (Option F)

**Status:** ✅ COMPLETE (2026-01-21)

## Problem Statement

The current "reverse sink" mechanism where iOS sends `SET_DAILY_TOTAL` back to the bottle is fundamentally flawed. When the bottle rejects a stale update (because a new drink arrived), the app and bottle end up with inconsistent state - the app has deleted a record locally but the bottle still counts it.

**Root cause:** We're syncing the *calculated total* instead of the *actual drink records*. The solution is to sync record-level operations (deletes) bidirectionally.

---

## Solution: Bidirectional Record Sync

When a user deletes a drink in iOS:
1. iOS sends `DELETE_DRINK_RECORD(record_id)` command to bottle
2. Bottle marks the record as deleted (soft delete)
3. Bottle recalculates `daily_total_ml` from non-deleted records
4. Bottle sends confirmation via Current State notification
5. Only then does iOS delete from CoreData (pessimistic delete)

Both sides now have the same view of which records are valid.

---

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Record ID type | `uint32_t` incrementing counter | Simple, 4 bytes, 4 billion records before wrap |
| Delete approach | Soft delete (flag) | Recoverable, no circular buffer restructuring |
| Failure handling | Pessimistic with confirmation | Guaranteed consistency, slight UX delay |
| Missing record | Silent success | Record rolled off = effectively deleted |

---

## Implementation Plan

### Step 1: Firmware - Add record_id to DrinkRecord

**File:** [firmware/include/drinks.h](firmware/include/drinks.h)

```cpp
struct DrinkRecord {
    uint32_t record_id;         // NEW: Unique incrementing ID
    uint32_t timestamp;
    int16_t  amount_ml;
    uint16_t bottle_level_ml;
    uint8_t  flags;             // 0x01=synced, 0x02=day_boundary, 0x04=deleted
    uint8_t  type;
};
// Size: 14 bytes (was 10)
```

**File:** [firmware/include/storage_drinks.h](firmware/include/storage_drinks.h)

Add to CircularBufferMetadata:
```cpp
struct CircularBufferMetadata {
    uint16_t write_index;
    uint16_t record_count;
    uint32_t total_writes;
    uint32_t next_record_id;    // NEW: Next ID to assign
    uint16_t _reserved;
};
```

### Step 2: Firmware - Update storage functions

**File:** [firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp)

- `storageSaveDrinkRecord()`: Assign `record_id = meta.next_record_id++` when saving
- `storageGetUnsyncedRecords()`: Skip records where `flags & 0x04` (deleted)
- Add new function `storageMarkDeleted(uint32_t record_id)`:
  - Search circular buffer for record with matching ID
  - Set `flags |= 0x04`
  - Return true if found, false if not found (rolled off)

### Step 3: Firmware - Update daily total calculation

**File:** [firmware/src/drinks.cpp](firmware/src/drinks.cpp)

Add function to recalculate total from non-deleted records:
```cpp
uint16_t drinksRecalculateTotal() {
    uint16_t total = 0;
    // Sum all records from today where !(flags & 0x04)
    // Update g_daily_state.daily_total_ml
    return total;
}
```

### Step 4: Firmware - Add DELETE_DRINK_RECORD command

**File:** [firmware/include/ble_service.h](firmware/include/ble_service.h)

```cpp
#define BLE_CMD_DELETE_DRINK_RECORD  0x12  // 5 bytes: cmd + record_id
```

**File:** [firmware/src/ble_service.cpp](firmware/src/ble_service.cpp)

Add command handler:
```cpp
// DELETE_DRINK_RECORD command (5 bytes: cmd + 4-byte record_id)
if (value.length() == 5 && value[0] == BLE_CMD_DELETE_DRINK_RECORD) {
    uint32_t recordId = (uint8_t)value[1] | ((uint8_t)value[2] << 8) |
                        ((uint8_t)value[3] << 16) | ((uint8_t)value[4] << 24);

    bool found = storageMarkDeleted(recordId);
    if (found) {
        drinksRecalculateTotal();
        BLE_DEBUG_F("DELETE_DRINK_RECORD: %lu deleted", recordId);
    } else {
        BLE_DEBUG_F("DELETE_DRINK_RECORD: %lu not found (rolled off)", recordId);
    }
    // Always notify - either way, the delete is "successful"
    g_ble_force_display_refresh = true;
    bleNotifyCurrentStateUpdate();
    return;
}
```

### Step 5: Firmware - Update BLE_DrinkRecord struct

**File:** [firmware/include/ble_service.h](firmware/include/ble_service.h)

```cpp
struct __attribute__((packed)) BLE_DrinkRecord {
    uint32_t record_id;         // NEW: Unique ID for deletion
    uint32_t timestamp;
    int16_t  amount_ml;
    uint16_t bottle_level_ml;
    uint8_t  type;
    uint8_t  flags;
};
// Size: 14 bytes (was 10)
```

Update `blePopulateDrinkRecord()` to include record_id.

### Step 6: iOS - Update BLEDrinkRecord struct

**File:** [ios/Aquavate/Aquavate/Services/BLEStructs.swift](ios/Aquavate/Aquavate/Services/BLEStructs.swift)

```swift
struct BLEDrinkRecord {
    let recordId: UInt32       // NEW: Firmware record ID
    let timestamp: UInt32
    let amountMl: Int16
    let bottleLevelMl: UInt16
    let type: UInt8
    let flags: UInt8

    static let size = 14  // was 10
}
```

Update parsing in `init?(data:offset:)`.

### Step 7: iOS - Add DELETE_DRINK_RECORD command

**File:** [ios/Aquavate/Aquavate/Services/BLEStructs.swift](ios/Aquavate/Aquavate/Services/BLEStructs.swift)

```swift
static func deleteDrinkRecord(recordId: UInt32) -> Data {
    var data = Data(count: 5)
    data.withUnsafeMutableBytes { ptr in
        guard let baseAddress = ptr.baseAddress else { return }
        baseAddress.storeBytes(of: UInt8(0x12), as: UInt8.self)
        baseAddress.storeBytes(of: recordId, toByteOffset: 1, as: UInt32.self)
    }
    return data
}
```

### Step 8: iOS - Add firmwareRecordId to CDDrinkRecord

**File:** CoreData model

Add attribute:
```
firmwareRecordId: Integer 32 (optional)
```

### Step 9: iOS - Update PersistenceController

**File:** [ios/Aquavate/Aquavate/CoreData/PersistenceController.swift](ios/Aquavate/Aquavate/CoreData/PersistenceController.swift)

- `saveDrinkRecords()`: Store `recordId` from BLEDrinkRecord
- Add `getDrinkRecord(byFirmwareId:)` to look up records for deletion confirmation

### Step 10: iOS - Update BLEManager for pessimistic delete

**File:** [ios/Aquavate/Aquavate/Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)

Add delete function with confirmation:
```swift
func deleteDrinkRecord(firmwareRecordId: UInt32, completion: @escaping (Bool) -> Void) {
    guard let peripheral = connectedPeripheral,
          let characteristic = characteristics[BLEConstants.commandUUID] else {
        completion(false)
        return
    }

    // Store completion handler for when Current State notification arrives
    pendingDeleteCompletion = completion
    pendingDeleteRecordId = firmwareRecordId

    let data = BLECommand.deleteDrinkRecord(recordId: firmwareRecordId)
    peripheral.writeValue(data, for: characteristic, type: .withResponse)
}
```

Handle confirmation in `handleCurrentStateUpdate()`:
```swift
if let completion = pendingDeleteCompletion {
    completion(true)  // Delete confirmed
    pendingDeleteCompletion = nil
}
```

### Step 11: iOS - Update HistoryView for pessimistic delete

**File:** [ios/Aquavate/Aquavate/Views/HistoryView.swift](ios/Aquavate/Aquavate/Views/HistoryView.swift)

Update `deleteDrinks()`:
```swift
func deleteDrinks(at offsets: IndexSet) {
    for index in offsets {
        let record = todaysDrinks[index]

        if let firmwareId = record.firmwareRecordId,
           bleManager.connectionState.isConnected {
            // Pessimistic delete - wait for confirmation
            bleManager.deleteDrinkRecord(firmwareRecordId: UInt32(firmwareId)) { success in
                if success {
                    PersistenceController.shared.deleteDrinkRecord(id: record.id)
                }
            }
        } else {
            // Not connected or no firmware ID - delete locally only
            PersistenceController.shared.deleteDrinkRecord(id: record.id)
        }
    }
}
```

### Step 12: Remove SET_DAILY_TOTAL reverse sync

**File:** [ios/Aquavate/Aquavate/Views/HistoryView.swift](ios/Aquavate/Aquavate/Views/HistoryView.swift)
**File:** [ios/Aquavate/Aquavate/Views/HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift)

Remove the `sendSetDailyTotalCommand()` calls after deletion - no longer needed since we're syncing the actual delete operation.

---

## Files to Modify

| File | Changes |
|------|---------|
| [firmware/include/drinks.h](firmware/include/drinks.h) | Add `record_id` to DrinkRecord (14 bytes) |
| [firmware/include/storage_drinks.h](firmware/include/storage_drinks.h) | Add `next_record_id` to metadata |
| [firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp) | Assign IDs, add `storageMarkDeleted()` |
| [firmware/src/drinks.cpp](firmware/src/drinks.cpp) | Add `drinksRecalculateTotal()` |
| [firmware/include/ble_service.h](firmware/include/ble_service.h) | Add command, update BLE_DrinkRecord |
| [firmware/src/ble_service.cpp](firmware/src/ble_service.cpp) | Add DELETE_DRINK_RECORD handler |
| [ios/.../BLEStructs.swift](ios/Aquavate/Aquavate/Services/BLEStructs.swift) | Update record size, add delete command |
| [ios/.../CoreData model](ios/Aquavate/Aquavate/CoreData/) | Add `firmwareRecordId` attribute |
| [ios/.../PersistenceController.swift](ios/Aquavate/Aquavate/CoreData/PersistenceController.swift) | Store firmware ID when syncing |
| [ios/.../BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) | Add pessimistic delete with confirmation |
| [ios/.../HistoryView.swift](ios/Aquavate/Aquavate/Views/HistoryView.swift) | Update delete to use new flow |
| [ios/.../HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift) | Remove SET_DAILY_TOTAL call |

---

## Data Migration

Existing drink records in NVS won't have `record_id`. Options:
1. **Assign IDs on first read** - When loading old records, assign sequential IDs starting from 1
2. **Clear history** - Wipe drink records on firmware upgrade (loses ~2 months of history)

**Recommendation:** Option 1 - backwards compatible, preserves data.

---

## Verification Plan

1. **Build firmware** and upload
2. **Build iOS app** and install
3. **Test new record sync:**
   - Create drink on bottle
   - Verify iOS receives record with `firmwareRecordId`
   - Check CoreData has the ID stored
4. **Test delete flow:**
   - Delete drink in iOS while connected
   - Verify bottle receives DELETE command
   - Verify bottle's daily total updates
   - Verify iOS deletes only after confirmation
5. **Test disconnected delete:**
   - Delete drink while disconnected
   - Verify local delete works
   - Reconnect - totals should match (bottle still has record)
6. **Test rolled-off record:**
   - Delete very old drink (if buffer wrapped)
   - Verify silent success (no error)

---

## IRAM Budget Check

Current: 125KB / 131KB (95.3%)

Additions:
- DrinkRecord size: +4 bytes × 600 records = 2.4KB (in NVS, not IRAM)
- DELETE handler: ~300 bytes
- `drinksRecalculateTotal()`: ~200 bytes

**Estimated new usage:** ~125.5KB / 131KB (95.8%) - should fit.
