# OTA Firmware Update Implementation Plan

## Decision: Defer to ESP32-S3

**Chosen Option**: Wait for ESP32-S3 hardware upgrade before implementing OTA.

**Rationale**:
1. IRAM constraints on ESP32 V2 (94.5% used) make BLE OTA risky
2. BLE transfer speed (~5 minutes for 720KB) provides poor UX
3. ESP32-S3 enables WiFi OTA (~10 seconds) with no memory constraints
4. USB updates work reliably for current development phase

---

## Options Considered (Summary)

| Option | Feasibility | Why Not Chosen |
|--------|-------------|----------------|
| BLE OTA now | Risky | IRAM likely overflows; 5-min transfer too slow |
| WiFi OTA now | Impossible | WiFi stack needs +50KB IRAM (doesn't fit) |
| **Defer to S3** | **Chosen** | Best UX, no constraints, low risk |
| Hybrid mode switch | Possible | Too complex; not worth it given S3 timeline |

---

## Current State (Reference)

| Parameter | Value |
|-----------|-------|
| IRAM Used | 121-125 KB / 131 KB (94.5%) |
| IRAM Headroom | ~6-7 KB only |
| Firmware Size | 711-720 KB |
| OTA Partition Slots | Already configured (app0/app1) |
| ESP32-S3 IRAM | 512 KB (4x current) |

---

## User Experience: OTA Update Flow

### From the iOS App

1. **Update Available Notification**
   - App checks firmware version on connect (via Device Info service)
   - If newer version available, shows badge/banner: "Firmware update available"

2. **Update Screen**
   - User taps notification or goes to Settings > Firmware Update
   - Shows: Current version, New version, What's new (changelog)
   - "Update Now" button (disabled if battery < 50%)

3. **Pre-flight Checks**
   - App verifies: Battery level OK, stable BLE/WiFi connection
   - If WiFi available: "Update will take ~10 seconds"
   - If BLE only: "Update will take ~5 minutes" (fallback)

4. **Update in Progress**
   - Progress bar with percentage
   - "Do not turn off your bottle" warning
   - Cancel button (safe to cancel before 90%)

5. **Completion**
   - "Update complete! Your bottle will restart."
   - Bottle automatically reboots to new firmware
   - App reconnects and confirms new version

### From the Bottle's Perspective

1. Receives OTA start command via BLE/WiFi
2. Display shows: "Updating... 0%" with progress bar
3. Writes firmware chunks to inactive partition (app1)
4. On completion: Validates hash, marks new partition bootable
5. Reboots to new firmware
6. If boot fails 3x: Automatically rolls back to previous version

### Error Handling (User Sees)

| Scenario | User Message |
|----------|--------------|
| Battery too low | "Charge to 50% before updating" |
| Connection lost | "Update paused. Reconnect to continue." |
| Update failed | "Update failed. Your bottle is unchanged." |
| Rollback occurred | "Update unsuccessful. Previous version restored." |

---

## Implementation Plan (When S3 Available)

### Prerequisites
- ESP32-S3 Feather hardware acquired
- Firmware builds for both S3 and Feather V2 (backward compatible)

### Implementation Steps

| Step | Description |
|------|-------------|
| 1 | Port firmware to ESP32-S3, verify all features work |
| 2 | Add WiFi OTA using esp_https_ota |
| 3 | Add BLE OTA as backup (NimBLEOta library) |
| 4 | iOS app: OTA client with progress UI |
| 5 | Testing and validation |

### Conditional Compilation: ESP32_S3 Flag

Replace `IOS_MODE` with `ESP32_S3` flag to maintain backward compatibility:

```cpp
// config.h
#define ESP32_S3  1  // Set to 0 for Feather V2, 1 for S3

#if ESP32_S3
    // S3 has 512KB IRAM - enable all features
    #define ENABLE_BLE                      1
    #define ENABLE_SERIAL_COMMANDS          1  // Can coexist on S3
    #define ENABLE_STANDALONE_CALIBRATION   1
    #define ENABLE_OTA                      1  // NEW: OTA support
    #define ENABLE_WIFI                     1  // NEW: WiFi for OTA
#else
    // Feather V2 has 131KB IRAM - keep existing constraints
    #define ENABLE_BLE                      1
    #define ENABLE_SERIAL_COMMANDS          0
    #define ENABLE_STANDALONE_CALIBRATION   0
    #define ENABLE_OTA                      0  // No OTA on V2
    #define ENABLE_WIFI                     0
#endif
```

**Benefits:**
- Single firmware codebase for both boards
- Feather V2 continues working (no OTA, same as today)
- S3 gets OTA + all features enabled
- Clear compile-time selection via platformio.ini environments

### platformio.ini Environments

```ini
[env:adafruit_feather]
board = adafruit_feather_esp32_v2
build_flags = -DESP32_S3=0
; No OTA, constrained IRAM

[env:adafruit_feather_s3]
board = adafruit_feather_esp32s3
build_flags = -DESP32_S3=1
lib_deps =
    ${env.lib_deps}
    h2zero/NimBLEOta@^1.0.0
; Full OTA support, all features enabled
```

### OTA Architecture on S3

**Primary: WiFi OTA**
- Transfer speed: ~50-100 KB/s (~10 seconds for firmware)
- Use esp_https_ota for secure HTTPS downloads
- Firmware hosted on simple HTTPS server
- Wrapped in `#if ENABLE_WIFI && ENABLE_OTA`

**Backup: BLE OTA**
- For cases where WiFi credentials not configured
- Use NimBLEOta library (no IRAM concerns on S3)
- Wrapped in `#if ENABLE_BLE && ENABLE_OTA`

### Key Files to Modify (When Ready)

- [platformio.ini](firmware/platformio.ini) - Add S3 environment with OTA deps
- [config.h](firmware/include/config.h) - Replace IOS_MODE with ESP32_S3 flag
- [ble_service.cpp](firmware/src/ble_service.cpp) - Add BLE OTA service (`#if ENABLE_OTA`)
- New: `ota_service.cpp` - WiFi OTA handling (`#if ENABLE_OTA`)
- New: `wifi_manager.cpp` - WiFi connection for OTA (`#if ENABLE_WIFI`)
- iOS app - OTAManager and progress UI

### Security

- SHA256 hash validation of firmware before applying
- HTTPS for WiFi downloads
- Consider ECDSA signing for production

### Verification Plan

1. WiFi OTA: Download from test server, verify update applies
2. BLE OTA: Transfer via iOS app, verify update applies
3. Rollback: Flash bad firmware, confirm automatic revert
4. Power loss: Interrupt update, confirm device recovers
5. Version check: Verify downgrade prevention works

---

## Action Items

### Now (No Code Changes)
- [x] Document OTA decision in PROGRESS.md (deferred to S3)
- [x] Save this plan to Plans/ directory for future reference
- [x] Create branch `ota-firmware-updates` for future implementation

### When S3 Hardware Arrives
- [ ] Add `adafruit_feather_s3` environment to platformio.ini
- [ ] Replace IOS_MODE with ESP32_S3 flag in config.h
- [ ] Port firmware to S3, verify all existing features work
- [ ] Implement WiFi manager for OTA connections
- [ ] Implement OTA service (WiFi primary, BLE backup)
- [ ] Add OTA display UI (progress bar during update)
- [ ] iOS app: Build OTA update flow
- [ ] Test rollback mechanism (flash bad firmware, verify recovery)
- [ ] Test both boards build correctly from same codebase
