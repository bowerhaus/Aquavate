# Plan: Fix BLE Sync Chunk Truncation ("Invalid drink data chunk") ✅ COMPLETE

**GitHub Issue:** [#126](https://github.com/bowerhaus/Aquavate/issues/126)
**Branch:** `fix-ble-sync-mtu-truncation`

## Context

Syncing the bottle with the iOS app aborted with `Invalid drink data chunk`.
Small syncs worked, larger ones did not, so it presented as intermittent.

The chunk did not fit in a single BLE notification:

- `BLE_DrinkRecord` is 14 bytes; a chunk is a 6-byte header + up to 20 records
  = **286 bytes**
- An ATT notification carries at most **MTU − 3** bytes: 182 at the MTU of 185
  iOS negotiates, 244 at the 247 the firmware requests
- NimBLE silently truncates anything longer, so the app received ~182 bytes
  whose header claimed 20 records
- `BLEDrinkDataChunk.parse` rejected the short payload and the sync failed

The cut-off is ~12 records at iOS's usual MTU.

### How it got in

The record used to be 10 bytes — 6 + 20 × 10 = 206, which is where the "max 206
bytes" comments in `ble_service.h` and `BLEStructs.swift` came from, and the
`recordCount * 10` in the iOS error log. When the record grew to 14 bytes the
20-records-per-chunk constant was never revisited. The struct size, the chunk
count and the MTU each lived in a different file, with nothing checking them
against each other.

### Relationship to Plan 077 / Issue #120

[Plan 077](077-fix-timezone-daily-reset.md) fixed a daily-reset race that
produced the same error message — `drinksResetDaily()` mutating records while
the BLE task iterated them. That fix was correct. This is a **second,
independent cause of the same symptom**, which is why the error came back.

## Files Modified

| File | Change |
|------|--------|
| `firmware/include/ble_service.h` | `BLE_DRINK_RECORDS_PER_CHUNK` / `BLE_DRINK_CHUNK_HEADER_SIZE` constants; `static_assert`s pinning every wire struct size |
| `firmware/src/ble_service.cpp` | `bleMaxRecordsPerChunk()`; clamp `chunk_size` at sync START; track connection handle and MTU |
| `ios/.../Services/BLEStructs.swift` | MTU-safe default `chunkSize`; `safeRecordsPerChunk` |
| `ios/.../Services/BLEManager.swift` | `maxRecordsPerChunk()` derived from the write budget; progress estimate uses the actual chunk size; corrected error-log arithmetic |
| `ios/Aquavate/AquavateTests/BLEDrinkChunkTests.swift` | New — wire-format and truncation tests |

## The Fix

### Firmware — clamp to the negotiated MTU

`bleMaxRecordsPerChunk()` derives the limit from the live connection rather than
a constant:

```cpp
static uint16_t bleMaxRecordsPerChunk() {
    uint16_t mtu = 0;
    if (pServer != nullptr && syncConnHandle != BLE_HS_CONN_HANDLE_NONE) {
        mtu = pServer->getPeerMTU(syncConnHandle);
    }
    if (mtu < BLE_ATT_MTU_DFLT) {
        mtu = BLE_ATT_MTU_DFLT;  // Not yet negotiated - assume the minimum
    }
    uint16_t payload = mtu - 3 - BLE_DRINK_CHUNK_HEADER_SIZE;
    uint16_t max_records = payload / sizeof(BLE_DrinkRecord);
    ...
}
```

The sync START handler clamps whatever the app requested down to this: 12
records at MTU 185, 17 at 247. The clamped value goes back to the app in the
sync-control characteristic, as it already did.

The connection handle is captured in the `onConnect(pServer, desc)` overload and
cleared on disconnect; `onMTUChange` logs the negotiated value at debug level 9.

### iOS — ask for a size that fits

CoreBluetooth does not expose the MTU, but
`maximumWriteValueLength(for: .withoutResponse)` is the same MTU − 3 budget:

```swift
let payload = peripheral.maximumWriteValueLength(for: .withoutResponse) - BLEDrinkDataChunk.headerSize
let records = payload / BLEDrinkRecord.size
```

This is defence in depth — the firmware clamp alone fixes the bug — but it also
makes the app correct against firmware that does not clamp.

`totalRecordsToSync` previously assumed 20 records per chunk; it now uses the
first chunk's actual count. That value only feeds the "Total to Sync" label in
DebugView.

### Deliberately not done

The parser was **not** made tolerant of a short payload. Parsing only the
records that fit would turn a loud failure into silently dropped drink records.

## Resilience Against Recurrence

The underlying class of bug is a protocol struct growing without the size
assumptions that depend on it being revisited. Three guards:

1. **Compile-time assertions** in `ble_service.h` pin every wire struct size and
   tie `BLE_DRINK_CHUNK_HEADER_SIZE` to the actual layout. Growing
   `BLE_DrinkRecord` again fails the build instead of shipping.
2. **Both ends compute the limit** from the negotiated MTU rather than
   hard-coding it, so a size change cannot reintroduce the mismatch.
3. **`BLEDrinkChunkTests.swift`** builds chunks byte-for-byte as the firmware
   packs them, asserts the default chunk fits an iOS notification, and asserts a
   truncated chunk is rejected rather than half-parsed.

## Verification

1. Firmware builds: `cd firmware && ~/.platformio/penv/bin/platformio run` —
   Flash 61.2%, RAM 11.7%, no IRAM change ✅
2. Native firmware tests pass (`platformio test -e native`) ✅
3. iOS tests pass including 6 new chunk tests ✅
4. On device: sync with >12 pending records completes without
   `Invalid drink data chunk`; serial at `d9` shows the clamped chunk size —
   **user to confirm on hardware**

## Note on Deployment

The firmware clamp is server-side, so **flashing the firmware alone fixes the
sync** — an already-installed app receives correctly-sized chunks. The app
changes are not required for the fix, only for defence in depth.
