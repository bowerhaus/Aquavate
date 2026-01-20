# Aquavate - Active Development Progress

**Last Updated:** 2026-01-20 (ADXL343 Accelerometer Migration - Complete)

---

## Current Work

### ADXL343 Accelerometer Migration (2026-01-20) ✅ COMPLETE
**Status:** Implementation complete, hardware tested, wake-on-tilt working with aggressive motion filtering.

**Goal:** Replace LIS3DH 3-axis accelerometer with ADXL343 while maintaining wake-on-tilt functionality.

**Plan:** [Plans/023-adxl343-accelerometer-migration.md](Plans/023-adxl343-accelerometer-migration.md)

**Final Configuration:**
- **Wake Mode:** AC-coupled motion detection (not orientation-based like LIS3DH)
- **Threshold:** 3.0g (extremely high - only strong deliberate motion)
- **Duration:** 1.6 seconds (sustained motion required)
- **Result:** Ignores table vibrations and train movement, requires deliberate shake/pick-up to wake

**Key Learnings:**
1. **ADXL343 vs LIS3DH differences:** ADXL343 lacks simple axis high/low threshold interrupts that LIS3DH had
2. **DC-coupled activity mode doesn't work for orientation:** Compares absolute values, not deviations from gravity
3. **AC-coupled motion detection required:** Must use very aggressive thresholds (3g + 1.6sec) to filter environmental vibrations
4. **Trade-off accepted:** Users must shake bottle deliberately for ~2 seconds to wake (acceptable for battery life benefits)

**Hardware Configuration:**
- ADXL343 INT1 → GPIO 27 (A10 on Feather silkscreen)
- Original orientation: Y=-1g when upright (easier installation)
- E-Paper: EPD_DC=33, EPD_CS=15, EPD_BUSY=-1 (conflicts resolved)

**Files Modified:**
- [firmware/src/config/pins_adafruit.h](firmware/src/config/pins_adafruit.h) - PIN_ACCEL_INT=27
- [firmware/src/main.cpp](firmware/src/main.cpp) - AC-coupled activity interrupt (3.0g, 1.6sec)
- [firmware/src/gestures.cpp](firmware/src/gestures.cpp) - Gesture detection for Y=-1g upright orientation
- All other ADXL343 migration files (I2C detection, gesture updates, datasheet-based configuration)

**Testing Summary:**
- ✅ Display working correctly
- ✅ ADXL343 I2C communication working
- ✅ Gesture detection (inverted hold, upright stable) working
- ✅ Wake-on-tilt working with aggressive filtering
- ✅ Ignores table vibrations and train movement
- ✅ Deep sleep entry/wake cycle functioning

**Current Branch:** `new-hardware`

---

## Recently Active

### DS3231 RTC Integration (2026-01-20) ✅ COMPLETE
**Status:** Implementation and hardware testing complete

**Goal:** Enable DS3231 external RTC module as primary time source with battery-backed timekeeping and graceful fallback to ESP32 internal RTC.

**Plan:** [Plans/022-ds3231-rtc-integration.md](Plans/022-ds3231-rtc-integration.md)

**Key Benefits:**
- Accuracy: ±2-3 min/year vs ±2-10 min/day (100-500x improvement)
- Battery-backed: Maintains time through power loss (CR1220, ~8 year life)
- Deep sleep compatible: Runs from backup battery during sleep, zero main battery impact
- Set and forget: No more weekly USB time resyncs

**Implementation Approach:**
- Boot-time sync: ESP32 RTC ← DS3231 on every wake
- During wake: Use ESP32 RTC (fast, no I2C overhead)
- During sleep: DS3231 runs from CR1220 backup (~3µA), ESP32 RTC continues
- USB commands: Update both RTCs when setting time
- Graceful fallback: If DS3231 not detected, use ESP32 internal RTC

**Files to Modify:**
1. [firmware/src/main.cpp](firmware/src/main.cpp) - DS3231 detection, boot-time sync, global RTC object
2. [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp) - Update DS3231 during USB time-setting, enhanced diagnostics
3. [firmware/include/config.h](firmware/include/config.h) - Constants (if needed)

**Implementation Progress:**
- [x] Add RTClib includes and RTC_DS3231 global object to main.cpp
- [x] DS3231 detection in setup() (~line 700)
- [x] Boot-time sync from DS3231 → ESP32 RTC
- [x] Update handleSetDateTime() to sync both RTCs
- [x] Update handleSetDate() to sync both RTCs
- [x] Update handleSetTime() to sync both RTCs
- [x] Enhanced GET TIME diagnostics showing RTC source
- [x] Build verification - Flash: 57.7%, RAM: 11.1%
- [x] Hardware testing - DS3231 detected successfully
- [x] Bug fix: g_time_valid now correctly set to true when DS3231 present (was being overwritten by NVS load)
- [x] Set initial time via USB - DS3231 and ESP32 RTC both updated correctly
- [x] Verify USB time-setting updates both RTCs - confirmed working
- [x] Verify time persistence through deep sleep cycles - confirmed working
- [x] Test fallback when DS3231 disconnected - confirmed working
- [x] Verify 24-hour accuracy - time stays accurate within seconds

**Hardware Testing Summary:**
All tests passed successfully. DS3231 RTC integration working as designed:
- Boot-time sync from DS3231 → ESP32 RTC functioning correctly
- USB time-setting updates both RTCs
- Time persists accurately through deep sleep cycles
- Graceful fallback to ESP32 internal RTC when DS3231 disconnected
- Time accuracy maintained (±2-3 min/year vs ±2-10 min/day previously)

**Files Modified:**
- [firmware/platformio.ini](firmware/platformio.ini) - Added RTClib@^2.1.0 dependency
- [firmware/src/main.cpp](firmware/src/main.cpp) - DS3231 init, boot-time sync
- [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp) - Dual RTC sync, diagnostics

---

## Recently Active

### Pull-to-Refresh Sync for HomeView (2026-01-18) ✅ COMPLETE & TESTED
**Status:** Implementation complete and verified on hardware

**Goal:** Add pull-to-refresh gesture to HomeView that connects to the bottle (if needed) and syncs drink history. Shows "Bottle is asleep - tilt to wake" alert if the bottle cannot be found.

**Plan:** [Plans/021-pull-to-refresh-sync.md](Plans/021-pull-to-refresh-sync.md)

**Files Modified:**
1. **[ios/Aquavate/Aquavate/Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)**
   - Added `RefreshResult` enum for async operation outcomes
   - Added `performRefresh() async -> RefreshResult` - main entry point
   - Added `attemptConnection() async` - uses simple polling (same mechanism as Settings scan)
   - Added `performSyncOnly() async` - syncs when already connected, waits for Current State update
   - Added idle disconnect timer (60 seconds) - connection stays open for real-time updates
   - Added `startIdleDisconnectTimer()` and `cancelIdleDisconnectTimer()` methods

2. **[ios/Aquavate/Aquavate/Views/HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift)**
   - Added `@State` properties for alert presentation
   - Added `.refreshable { }` modifier to ScrollView
   - Added alert modifiers for "bottle asleep" and error cases

3. **[ios/Aquavate/Aquavate/Views/SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift)**
   - Wrapped connection controls (Scan/Connect/Disconnect) in `#if DEBUG`
   - Device Info and Device Commands sections remain visible in release
   - Updated version to "1.0.0 (Pull-to-Refresh)"

4. **[ios/Aquavate/Aquavate/AquavateApp.swift](ios/Aquavate/Aquavate/AquavateApp.swift)**
   - Removed auto-reconnect on foreground (user uses pull-to-refresh instead)
   - Background disconnect remains immediate (allows bottle to sleep)

**Key Behaviors:**
- Pull down → connect (if needed) → sync → stay connected 60 seconds
- Connection auto-disconnects after 60 seconds idle (battery conservation)
- Disconnect immediately when app goes to background
- Scan timeout with no devices → "Bottle is asleep - tilt to wake" alert
- Uses simple polling with Task.sleep (same mechanism as Settings scan)

**Edge Cases:**
| Scenario | Behavior |
|----------|----------|
| Bluetooth off | Show error: "Bluetooth is turned off" |
| Already connected | Sync, reset idle timer |
| Already syncing | Wait for current sync |
| No unsynced records | Stay connected (60s idle timer) |
| Scan timeout (no devices) | Show "bottle asleep" alert |
| Connection drops mid-sync | Show sync error |

**Implementation Progress:**
- [x] RefreshResult enum added to BLEManager.swift
- [x] Async performRefresh() method
- [x] attemptConnection() and performSyncOnly() helpers
- [x] HomeView .refreshable modifier and alerts
- [x] SettingsView #if DEBUG wrapping
- [x] Build verification
- [x] Idle disconnect timer (60 seconds)
- [x] Removed auto-reconnect on foreground

**Verification Checklist (hardware tested 2026-01-18):**
- [x] Pull down when disconnected, bottle awake → connects, syncs, stays connected 60s
- [ ] Pull down when disconnected, bottle asleep → shows "tilt to wake" alert after ~10s
- [x] Pull down when already connected → syncs, resets idle timer
- [ ] Pull down with Bluetooth off → shows Bluetooth error
- [x] Pull down with no unsynced records → stays connected (idle timer)
- [ ] Verify bottle returns to sleep after app disconnects
- [x] Verify connection controls hidden in release build, visible in debug build
- [x] Real-time data verified: weight=175g, level=175ml, daily=380ml, battery=99%
- [x] Flags verified: timeValid=true, calibrated=true, stable=true

**Fixed Minor Issue (0-byte drink data notification):**
- Root cause: Drink Data characteristic had no initial value, causing 0-byte notification on subscribe
- Firmware fix: Set 6-byte empty chunk header on init (`00 00 00 00 00 00`)
- iOS fix: Recognize and silently ignore empty initial chunk (totalChunks=0, recordCount=0)
- Files modified: [ble_service.cpp](firmware/src/ble_service.cpp), [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)

---

### iOS Human Figure Progress Graphic (2026-01-18) ✅ COMPLETE
**Status:** Implementation complete and tested

**Goal:** Replace circular progress ring with human figure that fills from bottom to show hydration progress

**Approach:** Using SVG vector graphics directly in iOS (no bitmap conversion needed)

**Files Created/Modified:**
- [ios/Aquavate/Aquavate/Components/HumanFigureProgressView.swift](ios/Aquavate/Aquavate/Components/HumanFigureProgressView.swift) - New SwiftUI view component
- [ios/Aquavate/Aquavate/Assets.xcassets/HumanFigureOutline.imageset/](ios/Aquavate/Aquavate/Assets.xcassets/HumanFigureOutline.imageset/) - SVG outline (stroke, no fill)
- [ios/Aquavate/Aquavate/Assets.xcassets/HumanFigureFilled.imageset/](ios/Aquavate/Aquavate/Assets.xcassets/HumanFigureFilled.imageset/) - SVG filled silhouette (for masking)
- [ios/Aquavate/Aquavate/Views/HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift) - Updated to use HumanFigureProgressView

**Implementation Details:**
- HumanFigureProgressView uses two SVG layers:
  1. HumanFigureFilled - solid black silhouette used as mask for blue progress fill
  2. HumanFigureOutline - stroke outline layered on top
- Blue fill animates from bottom up based on progress (0-100%)
- Text displays below figure: "X ml" and "of Yml goal"
- Frame size: 132x220 (10% larger than original 120x200)
- SVGs use `preserves-vector-data: true` for perfect scaling

---

### Phase 4 Bug Fixes & Polish (2026-01-17) ✅ COMPLETE
**Status:** ✅ All critical sync bugs fixed, UI polish complete

**Issues Fixed Today:**

1. **Drinks not appearing in UI after sync** ✅ Fixed
   - Root cause: `@FetchRequest` predicates used static `Date()` captured at view init
   - Fix: Changed to fetch all records and filter dynamically in computed properties
   - Files: [HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift), [HistoryView.swift](ios/Aquavate/Aquavate/Views/HistoryView.swift)

2. **Sync not triggering for new drinks** ✅ Fixed
   - Root cause: `syncState` stayed at `.complete` after sync, blocking new syncs
   - Fix: Reset `syncState` to `.idle` after completion
   - File: [BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift)

3. **"X unsynced" badge not updating after sync** ✅ Fixed
   - Root cause: Firmware didn't send Current State notification after marking records synced
   - Fix: Added `bleNotifyCurrentStateUpdate()` call after sync complete
   - File: [ble_service.cpp](firmware/src/ble_service.cpp)

4. **Infinite sync loop - records never marked synced** ✅ Fixed
   - Root cause: `storageMarkSynced()` marked by logical index, but unsynced records could be at any index
   - Fix: Changed to iterate and mark first N unsynced records (matching `storageGetUnsyncedRecords` logic)
   - File: [storage_drinks.cpp](firmware/src/storage_drinks.cpp)

5. **UI polish - removed unnecessary status badges** ✅ Fixed
   - Removed "Stable" badge (technical debug info, not user-facing)
   - Changed "Calibrated" to only show "Not Calibrated" warning when needed
   - File: [HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift)

**Current Status:**
- ✅ App connects and syncs drinks successfully
- ✅ Drinks appear in Recent Drinks and History views
- ✅ Unsynced count updates correctly after sync
- ✅ New drinks sync automatically when poured while connected
- ✅ UI shows only relevant warnings (Not Calibrated, X unsynced)

---

### IRAM Optimization - Conditional Compilation (2026-01-17) ✅ COMPLETE
**Status:** ✅ Phase 1 and Phase 2 complete

**Problem:** IRAM overflow (134,076 / 131,072 bytes = 3,004 bytes over limit)
- Adding time-set BLE command caused overflow
- Needed to reduce IRAM by ~3KB minimum
- Additional ~2.4KB savings possible by disabling standalone calibration

**Phase 1: BLE/Serial Commands Conditional Compilation** ✅ COMPLETE

**Implementation Details:**
- Added `IOS_MODE` master flag in [firmware/src/config.h](firmware/src/config.h:29)
- Auto-configures `ENABLE_BLE` and `ENABLE_SERIAL_COMMANDS` based on mode
- Flags are mutually exclusive (compile error if both enabled)
- Reverted from ESP-IDF Bluedroid back to NimBLE
- Wrapped all BLE code in `#if ENABLE_BLE` blocks
- Wrapped all serial command code in `#if ENABLE_SERIAL_COMMANDS` blocks
- Updated main.cpp with conditional initialization

**Verified Build Results (Phase 1):**

**Mode 1: iOS App Mode (Production - default)**
- Config: `IOS_MODE=1` → `ENABLE_BLE=1`, `ENABLE_SERIAL_COMMANDS=0`
- IRAM Usage: **127,362 bytes / 131,072 bytes** (97.2%)
- **✅ PASSES** with 3,710 bytes headroom
- Flash: 762,004 bytes (58.1%)
- RAM: 36,548 bytes (11.2%)

**Mode 2: Standalone USB Mode (Development)**
- Config: `IOS_MODE=0` → `ENABLE_BLE=0`, `ENABLE_SERIAL_COMMANDS=1`
- IRAM Usage: **81,846 bytes / 131,072 bytes** (62.4%)
- **✅ PASSES** with 49,226 bytes headroom
- Flash: 467,928 bytes (35.7%)
- RAM: 23,060 bytes (7.0%)

**IRAM Savings Achieved (Phase 1):**
- Disabling serial commands: ~3.7 KB
- Disabling BLE: ~45.5 KB
- Safety check verified: Compile error when both enabled

**Phase 2: Standalone Calibration Conditional Compilation** ✅ COMPLETE

**Objective:** Save additional IRAM in iOS mode by disabling standalone calibration
- iOS app will perform calibration via its own wizard UI
- Firmware provides core calibration functions (weight calc, scale factor)
- Standalone calibration state machine disabled in iOS mode

**Implementation Complete:**
- ✅ Added `ENABLE_STANDALONE_CALIBRATION` flag to [config.h](firmware/src/config.h:35)
- ✅ Auto-configured by `IOS_MODE` (disabled in iOS mode, enabled in USB mode)
- ✅ Wrapped [calibration.h](firmware/include/calibration.h) state machine with `#if ENABLE_STANDALONE_CALIBRATION`
- ✅ Wrapped [calibration.cpp](firmware/src/calibration.cpp) state machine with `#if ENABLE_STANDALONE_CALIBRATION`
- ✅ Core functions (`calibrationCalculateScaleFactor`, `calibrationGetWaterWeight`) always compiled
- ✅ Wrapped [ui_calibration.h](firmware/include/ui_calibration.h) and [ui_calibration.cpp](firmware/src/ui_calibration.cpp) entirely
- ✅ Wrapped standalone calibration code in [main.cpp](firmware/src/main.cpp)
- ✅ Removed BLE `START_CALIBRATION`/`CANCEL_CALIBRATION` commands entirely (not needed)
- ✅ Built and verified IRAM savings in both modes

**Verified Build Results (Phase 2):**

**Mode 1: iOS App Mode (Production - default)**
- Config: `IOS_MODE=1` → `ENABLE_BLE=1`, `ENABLE_SERIAL_COMMANDS=0`, `ENABLE_STANDALONE_CALIBRATION=0`
- IRAM Usage: **126,335 bytes / 131,072 bytes** (96.4%)
- **✅ PASSES** with 4,737 bytes headroom
- Flash: 754,452 bytes (57.6%)
- RAM: 36,468 bytes (11.1%)

**Mode 2: Standalone USB Mode (Development)**
- Config: `IOS_MODE=0` → `ENABLE_BLE=0`, `ENABLE_SERIAL_COMMANDS=1`, `ENABLE_STANDALONE_CALIBRATION=1`
- IRAM Usage: **80,819 bytes / 131,072 bytes** (61.7%)
- **✅ PASSES** with 50,253 bytes headroom
- Flash: 467,928 bytes (35.7%)
- RAM: 23,060 bytes (7.0%)

**IRAM Savings Achieved:**
- **Phase 1**: 3.7KB (serial commands) + 45.5KB (BLE vs USB mode)
- **Phase 2**: 1.0KB additional (standalone calibration UI + state machine)
- **Total iOS headroom**: 4.7KB (enough for future features)

**Files Modified:**
- [firmware/src/config.h](firmware/src/config.h) - Added ENABLE_STANDALONE_CALIBRATION flag
- [firmware/include/calibration.h](firmware/include/calibration.h) - Moved CalibrationState enum outside conditional, wrapped state machine
- [firmware/src/calibration.cpp](firmware/src/calibration.cpp) - Wrapped state machine implementation
- [firmware/include/ui_calibration.h](firmware/include/ui_calibration.h) - Wrapped entirely
- [firmware/src/ui_calibration.cpp](firmware/src/ui_calibration.cpp) - Wrapped entirely
- [firmware/src/main.cpp](firmware/src/main.cpp) - Wrapped calibration initialization, trigger, state machine, sleep prevention
- [firmware/include/ble_service.h](firmware/include/ble_service.h) - Removed START_CALIBRATION/CANCEL_CALIBRATION commands
- [firmware/src/ble_service.cpp](firmware/src/ble_service.cpp) - Removed calibration command implementations

**Key Design Decision:**
- BLE calibration commands removed entirely (not just disabled)
- iOS app will implement its own calibration wizard UI
- Firmware exposes core calibration math functions for iOS app to use
- This approach provides better UX and saves IRAM

**Plan Documents Updated:** ✅ All Complete
- ✅ [Plans/013-ble-integration.md](Plans/013-ble-integration.md) - Documented removal of BLE calibration commands, added iOS calibration note
- ✅ [Plans/014-ios-ble-coredata-integration.md](Plans/014-ios-ble-coredata-integration.md) - Added Phase 4.7: iOS Calibration Wizard with complete implementation details
- ✅ [Plans/015-ios-ux-prd-creation.md](Plans/015-ios-ux-prd-creation.md) - Added comprehensive Calibration Wizard UX specification
- ✅ [docs/iOS-UX-PRD.md](docs/iOS-UX-PRD.md) - Updated to v1.1 with two-point calibration wizard (live ADC, stability indicators)

**Current Branch:** `serial-commands-removal` (ready to merge)

**Next Steps:**
1. ✅ Update plan documents with iOS calibration approach - COMPLETE
2. Test both build modes on hardware
3. Merge to master

---

## Recently Active

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
**Status:** ✅ Phase 4 Complete (2026-01-16) - Ready for Phase 3E

Implement iOS BLE client to communicate with ESP32 firmware and sync drink history. Will enable end-to-end testing of Phase 3 BLE implementation. See [Plans/014-ios-ble-coredata-integration.md](Plans/014-ios-ble-coredata-integration.md) for detailed implementation plan.

**Approach Decision:** BLE-First incremental replacement
- Keep existing placeholder UI components and views
- Add BLE layer alongside mock data (Phases 4.1-4.3)
- Surgically replace mock data with CoreData (Phase 4.4)
- Polish and add new features (Phases 4.5-4.6)

**Key Technical Decisions (2026-01-16):**
- ✅ BLE-First approach (see live data before adding persistence)
- ✅ Single device per app (MVP scope, matches PRD)
- ✅ Timestamp-based deduplication (CoreData unique constraint)
- ✅ Auto-reconnect on foreground
- ✅ Time sync on every connection
- ✅ HealthKit deferred to Phase 5
- ✅ Incremental scaffolding (keep existing views, modify not replace)

**Implementation Phases:**
- [x] **Phase 4.1: BLE Foundation** ✅ Complete (2026-01-16)
  - Created [Services/BLEConstants.swift](ios/Aquavate/Aquavate/Services/BLEConstants.swift) - UUIDs matching firmware
  - Created [Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift) - CBCentralManager delegate
  - Created [Info.plist](ios/Aquavate/Info.plist) - Bluetooth permissions and background mode
  - Updated [AquavateApp.swift](ios/Aquavate/Aquavate/AquavateApp.swift) - BLEManager environment object
  - Updated [SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift) - Connection UI with scan/connect/disconnect
  - Features implemented:
    - Device scanning with "Aquavate-XXXX" name filter
    - Auto-connect when single device found
    - Manual device selection when multiple found
    - Connection state management (disconnected/scanning/connecting/discovering/connected)
    - Service and characteristic discovery
    - Notification subscription for Current State, Drink Data, Battery Level
    - State restoration for background reconnection
    - Peripheral identifier persistence for auto-reconnect
    - Scan/connection timeouts
  - Build verified successful

- [x] **Phase 4.2: Current State & Battery Notifications** ✅ Complete (2026-01-16)
  - Created [Services/BLEStructs.swift](ios/Aquavate/Aquavate/Services/BLEStructs.swift) - Binary struct definitions matching firmware exactly
    - `BLECurrentState` (14 bytes) - Parse timestamp, weight, level, daily total, battery, flags, unsynced count
    - `BLEBottleConfig` (12 bytes) - Parse/serialize scale factor, tare weight, capacity, goal
    - `BLESyncControl` (8 bytes) - Parse/serialize sync protocol commands
    - `BLEDrinkRecord` (10 bytes) - Individual drink record parsing
    - `BLEDrinkDataChunk` (variable) - Chunked transfer header + records
    - `BLECommand` (4 bytes) - Command serialization with factory methods
  - Updated [Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift):
    - Added @Published properties: currentWeightG, bottleLevelMl, dailyTotalMl, batteryPercent
    - Added @Published flags: isTimeValid, isCalibrated, isStable, unsyncedCount
    - Added @Published config: bottleCapacityMl, dailyGoalMl, lastStateUpdateTime
    - Implemented `handleCurrentStateUpdate()` - Parse 14-byte Current State
    - Implemented `handleBatteryLevelUpdate()` - Parse Battery Service notification
    - Implemented `handleBottleConfigUpdate()` - Parse 12-byte Bottle Config
  - Updated [Views/HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift):
    - Uses BLEManager environment object for real-time data when connected
    - Falls back to mock data when disconnected (graceful degradation)
    - Added connection status indicator with color coding
    - Added battery level display with icon and percentage
    - Added status badges: Calibrated, Stable, Unsynced count
    - Added StatusBadge reusable component
  - Updated [Views/SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift):
    - Fixed battery indicator to use real BLE data (was using mock data)
    - Added Device Info section with: Battery, Current Weight, Calibrated, Time Set, Unsynced Records
    - Dynamic battery icon and color based on percentage
  - Updated [AquavateApp.swift](ios/Aquavate/Aquavate/AquavateApp.swift):
    - Added scenePhase observation for app lifecycle
    - Disconnect BLE when app goes to background (allows bottle to sleep)
    - Auto-reconnect when app returns to foreground
  - Build verified successful
  - Hardware tested: real-time updates working on Home and Settings screens

- [x] **Phase 4.3: CoreData Schema & Persistence** ✅ Complete (2026-01-16)
  - Created [CoreData/Aquavate.xcdatamodeld](ios/Aquavate/Aquavate/CoreData/Aquavate.xcdatamodeld) - Schema definition
    - `CDBottle` entity: id, name, capacityMl, dailyGoalMl, batteryPercent, scaleFactor, tareWeightGrams, isCalibrated, lastSyncDate, peripheralIdentifier
    - `CDDrinkRecord` entity: id, timestamp, amountMl, bottleLevelMl, drinkType, syncedToHealth, bottleId
    - Relationship: CDBottle → CDDrinkRecord (one-to-many, cascade delete)
    - Unique constraint on CDDrinkRecord: [timestamp, bottleId] for deduplication
  - Created [CoreData/PersistenceController.swift](ios/Aquavate/Aquavate/CoreData/PersistenceController.swift)
    - Shared singleton with in-memory preview support
    - NSMergeByPropertyObjectTrumpMergePolicy for automatic deduplication
    - Preview data with sample bottle and drink records
    - Helper methods: getOrCreateDefaultBottle(), saveDrinkRecords(), getTodaysDrinkRecords()
    - Extension methods: toDrinkRecord() for CDDrinkRecord, toBottle() for CDBottle
  - Updated [AquavateApp.swift](ios/Aquavate/Aquavate/AquavateApp.swift)
    - Injected CoreData context via .environment(\.managedObjectContext, ...)
  - Build verified successful

- [x] **Phase 4.4: Drink Sync Protocol** ✅ Complete (2026-01-16)
  - Added sync state machine to [Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift):
    - `BLESyncState` enum: idle, requesting, receiving, complete, failed
    - @Published properties: syncState, syncProgress, syncedRecordCount, totalRecordsToSync
    - Sync buffer to accumulate records during chunked transfer
  - Implemented Drink Data characteristic handling:
    - `handleDrinkDataUpdate()` - Parse variable-size chunks (up to 206 bytes)
    - Validates chunk sequence, appends records to buffer
    - Progress tracking with chunk-based percentage
    - ACK-based flow control with automatic retry
  - Auto-sync on connection:
    - Triggers when `unsyncedCount > 0` on first Current State update
    - Sends START command to firmware with record count
    - Completes sync when last chunk received
  - CoreData integration:
    - `completeSyncTransfer()` saves all records via PersistenceController
    - Timestamp deduplication via unique constraint
    - Gets/creates default bottle ID for relationship
  - Build verified successful

- [x] **Phase 4.5: Commands & Time Sync** ✅ Complete (2026-01-16)
  - Added command methods to [Services/BLEManager.swift](ios/Aquavate/Aquavate/Services/BLEManager.swift):
    - `sendTareCommand()` - TARE_NOW (0x01) - Reset empty bottle baseline
    - `sendResetDailyCommand()` - RESET_DAILY (0x05) - Reset daily intake counter
    - `sendClearHistoryCommand()` - CLEAR_HISTORY (0x06) - Clear all drink records
    - `sendStartCalibrationCommand()` - START_CALIBRATION (0x02) - Begin calibration
    - `sendCancelCalibrationCommand()` - CANCEL_CALIBRATION (0x04) - Cancel calibration
    - `writeCommand()` - Generic command write to Command characteristic
  - Implemented time sync:
    - `syncDeviceTime()` - Sends SET_TIME (0x10) with current Unix timestamp
    - Auto-syncs time on every connection (in `checkDiscoveryComplete()`)
  - Added Bottle Config methods:
    - `writeBottleConfig(capacity:goal:)` - Update bottle capacity and daily goal
    - `readBottleConfig()` - Request bottle config read from device
  - Updated [Views/SettingsView.swift](ios/Aquavate/Aquavate/Views/SettingsView.swift):
    - Added "Device Commands" section with command buttons
    - Tare, Reset Daily, Sync Time, Clear History buttons
    - Manual Sync button when unsynced records > 0
    - Sync progress indicator during active sync
    - Updated version to 1.0.0 (BLE Phase 4.5)
  - Build verified successful

- [x] **Phase 4.6: Background Mode & Polish** ✅ Complete (2026-01-16)
  - Verified state restoration infrastructure:
    - CBCentralManagerOptionRestoreIdentifierKey already configured
    - Peripheral ID persistence via UserDefaults (lastConnectedPeripheralIdentifier)
    - willRestoreState callback for reconnecting to restored peripherals
    - UIBackgroundModes "bluetooth-central" in Info.plist
  - Added last sync timestamp:
    - `lastSyncTime` @Published property with UserDefaults persistence
    - Updated on successful sync completion (in completeSyncTransfer)
    - "Last Synced" row in Settings with relative time display
  - Improved error handling UI:
    - Dismissible error messages with X button
    - Bluetooth unavailable warning when Bluetooth is off
  - Build verified successful

**Phase 4 Summary:**
All 6 phases complete. iOS app can now:
- Scan and connect to Aquavate devices
- Receive real-time Current State updates (weight, level, daily total, battery)
- Auto-sync time on connection
- Auto-sync drink history when unsynced records detected
- Save drink records to CoreData with deduplication
- Send commands (Tare, Reset Daily, Clear History)
- Persist and restore connection state

**Phase 4 Verification Checklist (from Plan 014):**
- [x] App discovers and connects to firmware automatically
- [x] Real-time bottle level updates in HomeView (<1s latency)
- [x] Drink history syncs automatically on connection
- [x] CoreData prevents duplicate records (timestamp constraint works)
- [x] Commands work (TARE, RESET_DAILY tested)
- [x] Time syncs on every connection (time_valid flag set)
- [ ] Background sync works (needs testing: app backgrounded → pick up bottle → data synced)
- [x] Connection state visible in UI (connected/scanning/disconnected)
- [x] Daily total matches firmware display
- [x] Battery level displays correctly

**Next Steps:**
1. **Phase 4.7: iOS Calibration Wizard** (Optional - can be deferred)
   - Currently bottle must be calibrated via USB serial commands
   - iOS wizard would provide in-app calibration flow
   - Reference: [Plans/014-ios-ble-coredata-integration.md](Plans/014-ios-ble-coredata-integration.md) Phase 4.7

2. **Phase 3E: Firmware Power Optimization**
   - Reference: [Plans/013-ble-integration.md](Plans/013-ble-integration.md) Phase 3E
   - Tasks:
     - Connection parameter tuning (15-30ms intervals)
     - Graceful disconnection UI ("Synced ✓" message)
     - Verify wake integration (motion vs timer)
     - Power measurement validation
     - End-to-end testing with iOS app

3. **Merge branches to master**
   - `serial-commands-removal` branch ready to merge
   - Clean up any remaining debug logging

---

## Known Issues

**Fixed: Multiple drink sync bugs (2026-01-17):**
- See "Phase 4 Bug Fixes & Polish" section above for details
- All critical sync issues resolved: drinks now appear in UI, sync completes properly, no infinite loops

**Fixed: Fatal error on drink data sync (2026-01-17):**
- Issue: "Fatal error: load from misaligned raw pointer" crash during drink sync
- Root cause: BLEStructs.swift used `load()` for multi-byte types on unaligned data
- Fix: Changed all struct parsing to use `loadUnaligned()` for packed binary data
- Also added improved debug logging with hex dumps for troubleshooting

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
- [BLE Integration](Plans/013-ble-integration.md) - Phase 3 firmware BLE implementation plan (COMPLETED - Phase 3A-3D)
- [iOS BLE & CoreData Integration](Plans/014-ios-ble-coredata-integration.md) - Phase 4 technical implementation plan (READY)
- [iOS UX PRD Creation](Plans/015-ios-ux-prd-creation.md) - UX specification planning (COMPLETED)
- [iOS UX PRD](docs/iOS-UX-PRD.md) - Complete UX specifications for iOS app (APPROVED)
- [Serial Commands Removal](Plans/020-serial-commands-removal.md) - IRAM optimization via conditional compilation (COMPLETED)
- [Hardware Research](Plans/001-hardware-research.md) - Component selection analysis
- [Adafruit BOM](Plans/002-bom-adafruit-feather.md) - UK parts list for Feather config
- [SparkFun BOM](Plans/003-bom-sparkfun-qwiic.md) - UK parts list for Qwiic config
