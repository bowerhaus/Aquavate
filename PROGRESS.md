# Aquavate - Active Development Progress

**Last Updated:** 2026-01-16

---

## Current Work

### Phase 3: Firmware BLE Integration & App Sync
**Status:** ⏸️ Paused at Phase 3D - Moving to Phase 4 (iOS App) (2026-01-16)

Implementing Bluetooth Low Energy (BLE) communication on ESP32 firmware to enable iOS app sync. Phase 3E (power optimization) deferred until iOS app is functional to enable end-to-end testing. See [Plans/013-ble-integration.md](Plans/013-ble-integration.md) for full implementation plan.

**Branch:** `ble-integration` (based on `master`)

**Completed Phases:**
- [x] **Phase 3A: BLE Foundation** ✅ Complete
  - NimBLE server initialization with power-optimized settings (0dBm TX power)
  - Device Information Service (0x180A): manufacturer, model, firmware, serial
  - Battery Service (0x180F): battery level with READ/NOTIFY
  - Aquavate custom service (UUID: 6F75616B-7661-7465-2D00-000000000000)
  - Advertising with "Aquavate-XXXX" device name (last 4 MAC digits)
  - Power-optimized advertising: 1000ms interval, 30s timeout
  - Connection/disconnection callbacks with auto-restart advertising
  - Integration: starts advertising on motion wake, skips on timer wake
  - Sleep prevention when BLE connected

- [x] **Phase 3B: Current State Characteristic** ✅ Complete
  - Current State characteristic (UUID ...0001) with READ/NOTIFY
  - 14-byte packed struct: timestamp, weight, bottle level, daily total, battery, flags, unsynced count
  - Real-time ADC to weight/ml conversion using calibration data
  - Intelligent notifications triggered on:
    - Daily total changed
    - Bottle level changed ≥10ml
    - Stability flag changed
  - Integrated with drink tracking loop (updates every 5s when stable)
  - Battery Service notifications on battery level changes
  - Flags: time_valid (bit 0), calibrated (bit 1), stable (bit 2)

- [x] **Phase 3C: Config & Commands** ✅ Complete
  - Bottle Config characteristic (UUID ...0002) with READ/WRITE
    - 12-byte packed struct: scale_factor, tare_weight, capacity, goal
    - Reads/writes calibration data from/to NVS
    - Persists across reboots
  - Command characteristic (UUID ...0005) with WRITE
    - 4-byte packed struct: command, param1, param2
    - Implemented commands:
      - TARE_NOW (0x01) - Resets empty bottle baseline to current weight
      - START_CALIBRATION (0x02) - Triggers two-point calibration flow
      - CANCEL_CALIBRATION (0x04) - Cancels active calibration
      - RESET_DAILY (0x05) - Resets daily intake counter
      - CLEAR_HISTORY (0x06) - Clears all drink records
  - Flag-based command handling (thread-safe)
  - All commands reset sleep timer to keep device awake

**Files Created:**
- [firmware/include/ble_service.h](firmware/include/ble_service.h) - BLE API and data structures
- [firmware/src/ble_service.cpp](firmware/src/ble_service.cpp) - BLE service implementation

**Files Modified:**
- [firmware/src/config.h](firmware/src/config.h) - Enabled DEBUG_BLE flag
- [firmware/src/main.cpp](firmware/src/main.cpp) - Integrated BLE init, update, sleep prevention, Current State updates, command handlers
- [firmware/include/storage_drinks.h](firmware/include/storage_drinks.h) - Added iterator function declarations (Phase 3D)
- [firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp) - Implemented circular buffer iterator functions (Phase 3D)

**Testing Status:**
- ✅ Firmware builds successfully (811KB flash / 61.9%, 11.4% RAM used)
- ✅ BLE advertising verified via serial debug output
- ✅ Connection/disconnection logic implemented
- ✅ Phases 3A-3C tested on real hardware with LightBlue iOS app
- ✅ Commands verified working (TARE_NOW, RESET_DAILY tested)
- ⏳ Phase 3D ready for hardware testing with LightBlue app

- [x] **Phase 3D: Drink Sync Protocol** ✅ Complete
  - Sync Control characteristic (UUID ...0003) with READ/WRITE
    - 8-byte packed struct: start_index, count, command, status, chunk_size
    - Commands: QUERY (0), START (1), ACK (2)
  - Drink Data characteristic (UUID ...0004) with READ/NOTIFY
    - Variable-size chunks (max 206 bytes): up to 20 records per chunk
    - Chunked transfer protocol with ACK-based flow control
  - Circular buffer iterator functions in storage_drinks.cpp:
    - `storageGetDrinkRecord()` - Read record by logical index
    - `storageMarkSynced()` - Set synced flag on record range
    - `storageGetUnsyncedCount()` - Count unsynced records
    - `storageGetUnsyncedRecords()` - Load all unsynced records into buffer
  - Sync state machine: IDLE → IN_PROGRESS → COMPLETE
  - Records marked as synced (flags |= 0x01) after successful transfer
  - Dynamic buffer allocation for sync (up to 600 records)
  - Current State characteristic updated with real-time unsynced_count

**Deferred Phases:**
- [ ] Phase 3E: Power Optimization - Deferred until Phase 4 complete (requires iOS app for end-to-end testing)
  - Connection parameter tuning (15-30ms intervals)
  - Graceful disconnection UI ("Synced ✓" message)
  - Verify wake integration (motion vs timer)
  - Power measurement validation

**Key Design Decisions:**
- Binary packed structs over JSON (4-5× size reduction)
- iOS background BLE auto-reconnect enabled
- Motion-wake required (no wake-on-BLE)
- No BLE pairing for MVP (open connection)
- Time sync deferred to Phase 4 (iOS app will set)

---

## Recently Completed

### iOS Placeholder UI (Phase 3.5)
**Status:** ✅ Complete (2026-01-16)

Multi-screen placeholder UI for iOS app with mock data. See [Plans/012-ios-placeholder-ui.md](Plans/012-ios-placeholder-ui.md) for details.

**Delivered:**
- Splash screen with water drop icon and fade-in animations
- Mock data models (DrinkRecord with 12 samples, Bottle config)
- Three main views (Home, History, Settings) with TabView navigation
- Reusable components (CircularProgressView, DrinkListItem)
- Ready for testing in Xcode on iPhone

**Files Created:** 8 new files (2 models, 4 views, 2 components) + 2 modified files

### Extended Deep Sleep Backpack Mode (Phase 2.6)
**Status:** ✅ Complete - Merged to master (2026-01-16)

Dual deep sleep modes (normal motion-wake + extended timer-wake) prevent battery drain during continuous motion scenarios. See [Plans/011-extended-deep-sleep-backpack-mode.md](Plans/011-extended-deep-sleep-backpack-mode.md).

**Power Impact:** ~60× reduction in backpack scenario (30mA continuous → 0.5mAh/hour)

---

## Next Up

### iOS App - Phase 4: BLE & Storage (Current Priority)
**Status:** 🔄 Ready to Start (2026-01-16)

Implement iOS BLE client to communicate with ESP32 firmware and sync drink history. Will enable end-to-end testing of Phase 3 BLE implementation.

**Tasks:**
- [ ] Implement CoreBluetooth BLE manager with background mode support
- [ ] Enable bluetooth-central background capability in Info.plist
- [ ] Add device scanning and connection with peripheral identifier persistence
- [ ] Parse binary BLE structs using withUnsafeBytes (DrinkRecord, CurrentState, etc.)
- [ ] Implement pull-based drink sync protocol on iOS side
- [ ] Create CoreData models (Bottle, DrinkRecord, DailySummary)
- [ ] Replace mock data with CoreData persistence
- [ ] Handle background reconnection and notification processing
- [ ] Set device time on first connection (critical for drink timestamps)
- [ ] Test background sync: app backgrounded → pick up bottle → verify data synced when foregrounded

**Prerequisites:**
- ✅ Firmware Phase 3A-3D complete (BLE server functional)
- ✅ iOS placeholder UI complete (screens ready for real data)
- ✅ Binary struct definitions documented in Phase 3 plan

**Handoff Point:**
- After Phase 4 complete, return to Phase 3E for power optimization and end-to-end validation

---

## Known Issues

**Wake-on-tilt directional sensitivity:**
- Wake interrupt reliably triggers on left/right tilts
- Forward/backward tilts may not consistently wake from deep sleep
- Root cause: LIS3DH Z-axis low interrupt (INT1_CFG=0x02) doesn't catch all tilt directions equally
- Impact: Minimal - users naturally pick up bottles in ways that trigger wake (left/right grasp)
- Future work: Could investigate 6D orientation detection or multi-axis interrupt configuration

---

## Branch Status

- `master` - **ACTIVE**: All standalone features complete (Phases 1-2.6), ready for Phase 3 BLE
- `comprehensive-fsm-refactoring` - Banked: Full FSM refactor (for future BLE/OTA work)

---

## Reference Documents

- [PRD.md](docs/PRD.md) - Full product requirements
- [Sensor Puck Design](Plans/004-sensor-puck-design.md) - Mechanical design v3.0
- [Standalone Calibration Mode](Plans/005-standalone-calibration-mode.md) - Two-point calibration implementation plan
- [USB Time Setting](Plans/006-usb-time-setting.md) - Serial time configuration for standalone operation
- [Daily Intake Tracking](Plans/007-daily-intake-tracking.md) - Phase 2 implementation plan (COMPLETED)
- [Drink Type Classification](Plans/008-drink-type-classification.md) - Gulp vs Pour implementation (COMPLETED)
- [Smart Display State Tracking](Plans/009-smart-display-state-tracking.md) - Display module with time/battery updates (COMPLETED)
- [Deep Sleep Reinstatement](Plans/010-deep-sleep-reinstatement.md) - Power management with 30s sleep (COMPLETED)
- [Extended Deep Sleep Backpack Mode](Plans/011-extended-deep-sleep-backpack-mode.md) - Dual sleep modes for continuous motion (COMPLETED)
- [iOS Placeholder UI](Plans/012-ios-placeholder-ui.md) - Multi-screen mockup UI (COMPLETED)
- [BLE Integration](Plans/013-ble-integration.md) - Phase 3 firmware BLE implementation plan (IN PROGRESS)
- [Hardware Research](Plans/001-hardware-research.md) - Component selection analysis
- [Adafruit BOM](Plans/002-bom-adafruit-feather.md) - UK parts list for Feather config
- [SparkFun BOM](Plans/003-bom-sparkfun-qwiic.md) - UK parts list for Qwiic config
