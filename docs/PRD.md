# Aquavate Product Requirements Document (PRD)

## Overview

Aquavate is a smart water bottle tracking system consisting of a sensor "puck" that attaches to the bottle base and an iOS companion app. The puck measures water intake via weight changes and syncs data to the app via Bluetooth Low Energy.

---

## Hardware Platform

### Primary: Adafruit Feather Ecosystem
- **MCU:** Adafruit ESP32 Feather V2 (STEMMA QT)
- **Display:** 2.13" Mono E-Paper FeatherWing (stacked)
- **Load Cell ADC:** NAU7802 24-bit ADC (I2C 0x2A)
- **Accelerometer:** ADXL343 (I2C 0x18) - wake-on-tilt + gesture detection
- **Load Cell:** 1-5kg bar load cell (half-bridge or full-bridge)
- **Power:** LiPo battery (500-1000mAh) via Feather charging circuit

### Future Option: SparkFun Qwiic Ecosystem
- SparkFun ESP32-C6 Qwiic Pocket
- Waveshare 1.54" e-paper (SPI)
- SparkFun Qwiic Scale NAU7802

---

## Hardware Assembly (MVP Puck)

### Bill of Materials

| Component | Part | Supplier | Est. Cost |
|-----------|------|----------|-----------|
| MCU | Adafruit ESP32 Feather V2 | Adafruit / Pimoroni | £20 |
| Display | 2.13" Mono E-Paper FeatherWing | Adafruit / Pimoroni | £22 |
| Load Cell ADC | Adafruit NAU7802 STEMMA QT | Adafruit / Pimoroni | £6 |
| Accelerometer | Adafruit ADXL343 STEMMA QT | Adafruit / Pimoroni | £6 |
| Load Cell | 1kg-5kg bar/disc load cell | Amazon / AliExpress | £5-10 |
| Battery | 3.7V LiPo 500-1000mAh | Pimoroni | £8-12 |
| Connectors | STEMMA QT cables (2x) | Adafruit | £3 |
| Enclosure | 3D printed puck housing | Custom | - |
| **Total** | | | **~£70-80** |

### Wiring Diagram

```
                    ┌─────────────────────┐
                    │  ESP32 Feather V2   │
                    │                     │
                    │  3V ────────────────┼──► ADXL343 VIN (always-on power)
                    │  GPIO27 ◄───────────┼─── ADXL343 INT1 (wake interrupt)
                    │                     │
                    │  STEMMA QT ◄────────┼─── NAU7802 (I2C: SDA, SCL, GND)
                    │      │              │        │
                    │      └──────────────┼────────┴─► ADXL343 (daisy-chain I2C)
                    │                     │
                    │  [E-Paper Wing]     │ (stacked on headers)
                    │                     │
                    │  JST ◄──────────────┼─── LiPo Battery
                    └─────────────────────┘

NAU7802 Load Cell Connections:
┌─────────┐
│ NAU7802 │
│         │
│ E+ ─────┼──► Load Cell E+ (Excitation+, Red)
│ E- ─────┼──► Load Cell E- (Excitation-, Black)
│ A+ ─────┼──► Load Cell A+ (Signal+, White)
│ A- ─────┼──► Load Cell A- (Signal-, Green)
└─────────┘
```

### Physical Construction

#### Puck Enclosure Requirements
- **Diameter:** ~80-100mm (fits under standard water bottles)
- **Height:** ~25-35mm (compact profile)
- **Material:** 3D printed PLA/PETG or injection molded ABS
- **Features:**
  - Recessed top surface for bottle stability
  - Load cell mounting points (typically 2-4 screws)
  - USB-C access hole for charging
  - E-paper display window
  - Non-slip rubber feet on bottom

#### Load Cell Mounting
1. **Platform design:** Rigid top plate transfers bottle weight to load cell
2. **Load cell orientation:** Bar cell mounted horizontally, disc cell centered
3. **Overload protection:** Mechanical stops to prevent damage >5kg
4. **Waterproofing:** Conformal coating on PCB, sealed enclosure edges

#### Assembly Steps
1. Stack E-Paper FeatherWing onto ESP32 Feather (headers)
2. Connect NAU7802 to Feather via STEMMA QT cable
3. Daisy-chain ADXL343 to NAU7802 via second STEMMA QT cable
4. Wire ADXL343 VIN to Feather 3V pin (always-on power for wake)
5. Wire ADXL343 INT1 to Feather GPIO27
6. Connect load cell wires to NAU7802 terminals
7. Mount load cell in enclosure base
8. Secure PCB stack in enclosure
9. Connect LiPo battery to Feather JST
10. Close enclosure, test fit with bottle

#### Power Considerations for Assembly
- **ADXL343 must have always-on power** (Feather 3V, not STEMMA QT which powers down in deep sleep)
- **Battery placement:** Away from load cell to avoid affecting weight readings

**IMPORTANT - Disable LEDs on Breakout Boards:**
Since we supply constant 3V power to the ADXL343 (and potentially NAU7802) during deep sleep, onboard LEDs will drain the battery continuously. Each LED draws ~1-2mA, which would reduce battery life from weeks to days.

| Board | LED Jumper Location | Action |
|-------|---------------------|--------|
| Adafruit ADXL343 | Back of board, trace labeled "LED" | Cut trace with hobby knife |
| Adafruit NAU7802 | Back of board (if present) | Cut trace if LED exists |
| ESP32 Feather V2 | CHG LED stays (only on when charging) | No action needed |

After cutting, verify LED no longer illuminates when board is powered.

### Prototype vs Production

| Aspect | MVP Prototype | Production |
|--------|---------------|------------|
| Enclosure | 3D printed | Injection molded |
| PCB | Breakout boards + wires | Custom PCB |
| Load cell | Generic bar cell | Custom disc cell |
| Assembly | Hand soldered | SMT assembly |
| Cost per unit | ~£70-80 | ~£15-25 |
| Waterproofing | Silicone sealant | IP67 gaskets |

---

## Firmware Requirements

### 1. Power States

| State | Trigger | Behavior | Power |
|-------|---------|----------|-------|
| **Deep Sleep** | Default | MCU sleeping, ADXL343 monitoring motion | ~100µA |
| **Wake** | Tilt detected | Initialize sensors, start measurement cycle | ~50mA |
| **Measuring** | Vertical + stable | Take weight readings, calculate drink | ~80mA |
| **Advertising** | After measurement | BLE advertising for iOS connection | ~20mA |
| **Connected** | iOS app connects | Sync drink history, receive config | ~30mA |

### 2. Measurement Logic

#### Wake Triggers
- **Motion wake (normal sleep):** ADXL343 activity interrupt (>1.5g threshold, AC-coupled) - tilt, pick-up, or tap wakes device. A single tap exceeds the 1.5g activity threshold so a dedicated single-tap interrupt is not needed.
- **Rollover wake:** Timer-based wake at midnight daily reset to refresh display with 0ml daily total
  - Ensures display shows correct daily total even if bottle sleeps through rollover
  - Returns to sleep immediately after display refresh (no BLE advertising)
- **Health-check wake:** Periodic timer wake (every 2 hours) in both normal and extended sleep modes
  - Ensures device auto-recovers after battery depletion + recharge (ADXL343 loses interrupt config when power drops)
  - Device boots normally, auto-sleeps after 30s inactivity timeout
  - In normal sleep, timer is `min(rollover, health-check)` — rollover takes priority when closer
  - Battery impact: ~1mAh/day (negligible vs 400-1000mAh LiPo)
- **Double-tap wake (backpack mode only):** ADXL343 double-tap interrupt for waking from extended sleep

#### Backpack Mode (Extended Sleep)
When the bottle hasn't been placed on a stable surface (UPRIGHT_STABLE) for 3 minutes, it enters "backpack mode":
- Display shows "backpack mode" with instructions: "double-tap firmly to wake up"
- ADXL343 reconfigured from motion detection to double-tap detection
- Health-check timer wake every 2 hours (auto-recovery from battery depletion)
- User double-taps bottle firmly to exit backpack mode
- Immediate "waking" feedback shown before sensor initialization
- After wake, ADXL343 restored to normal motion detection

**Manual entry:** User can also double-tap the bottle while awake to immediately enter backpack mode (same 3.0g threshold as wake). This allows entering backpack mode without waiting for the 3-minute auto-detection. Blocked during calibration. Gated on prior UPRIGHT_STABLE detection within the current wake cycle to prevent false triggers from the impact of setting the bottle down on a surface.

#### Stability Detection (Both Combined)
1. Detect vertical orientation: Z-axis dominant (>0.9g), X/Y near zero
2. Monitor accelerometer variance over 2-second window
3. Require variance < threshold for 1 second before measuring
4. Timeout after 10 seconds if stability not achieved

#### Weight Measurement
- Take 10 samples from NAU7802, discard outliers, average remaining
- Convert ADC value to grams using calibration factor
- Subtract tare weight (empty bottle) to get water weight
- Convert grams to ml (1g = 1ml for water)

#### Drink Detection
- Compare current weight to last recorded weight
- **Drink consumed:** Weight decreased by >= 30ml
- **Bottle filled:** Weight increased by >= 30ml
- **No change:** Weight delta < 30ml (ignore)

#### Empty Gesture (Inverted + Shake)
1. Detect inverted orientation: Z-axis strongly negative (<-0.8g)
2. While inverted, detect shake: acceleration variance > threshold for 1.5 seconds
3. On returning upright: Show "Bottle Emptied" confirmation, reset drink detection baseline to current level

**Note:** The baseline is reset to the current water level (not tare/empty) to prevent false positives if the user shakes mid-pour. This means refill detection won't trigger after shake-to-empty, but avoids incorrectly marking a half-full bottle as empty.

### 3. Data Storage (Offline Mode)

#### Drink Record Structure
```c
typedef struct {
    uint32_t record_id;      // Unique record ID (never reused)
    uint32_t timestamp;      // Unix epoch timestamp
    int16_t  amount_ml;      // Positive = drink, negative = refill
    int16_t  bottle_level;   // Current ml remaining after event
    uint8_t  synced;         // 1 = synced to iOS, 0 = pending
    uint8_t  type;           // DrinkType: SIP=1, GULP=2, POUR=3, REFILL=4
    uint8_t  reserved[2];    // Future extensibility
} DrinkRecord;  // 16 bytes total
```

#### Storage Implementation
- **Drink Records:** LittleFS file storage (`/drinks.bin`) - supports true in-place overwrites, no fragmentation
- **Metadata/Settings:** NVS - calibration, daily state, debug level (rarely written, no fragmentation risk)
- Circular buffer with 600 record capacity (~30 days history)
- Oldest records overwritten when full
- Sync flag tracks what's been sent to iOS

### 4. BLE Communication

#### Advertising
- Start advertising when bottle picked up (wake event)
- Stop after 30 seconds if no connection (no unsynced records)
- Extended 4-minute advertising if unsynced records exist (enables iOS background sync)
- Device name: "Aquavate-XXXX" (last 4 of MAC)

#### GATT Services

**Device Info Service (0x180A)**
- Manufacturer Name: "Aquavate"
- Model Number: "Puck v1"
- Firmware Version: "1.0.0"
- Serial Number: (from ESP32 MAC)

**Battery Service (0x180F)**
- Battery Level (0x2A19): 0-100%

**Aquavate Service (Custom UUID: 12345678-1234-5678-1234-56789abcdef0)**

| Characteristic | UUID | Properties | Description |
|----------------|------|------------|-------------|
| Current Weight | ...f1 | Read, Notify | Current bottle weight in grams |
| Bottle Config | ...f2 | Read, Write | Bottle capacity, tare weight |
| Drink History | ...f3 | Read, Notify | Bulk transfer of drink records |
| Sync Status | ...f4 | Read, Write | Sync control and acknowledgment |
| Device Status | ...f5 | Read, Notify | Battery, sensor status, errors |

#### Sync Protocol
1. iOS connects, reads Sync Status for pending record count
2. iOS reads Drink History in chunks (20 records per read)
3. iOS writes acknowledgment to Sync Status after processing
4. Puck marks acknowledged records as synced

### 5. E-Paper Display

#### Update Triggers (Wake on Tilt Only)
- Update display when bottle is picked up
- Show for duration of wake period
- No updates during deep sleep (e-paper retains image)

#### Display Content
```
+---------------------------+
|  [Battery Icon]     75%   |
|                           |
|      Current: 450ml       |
|      Today:   1.2L        |
|                           |
|  [Water Level Graphic]    |
|                           |
+---------------------------+
```

#### Status Indicators
- Battery icon with fill level
- BLE icon when advertising/connected
- Warning icon for low battery (<20%)

### 6. Calibration

#### Load Cell Calibration (Two-tier approach)

**Default Scale Factor (ships in firmware):**
- Typical scale factor for the load cell model used
- Provides ~±15-20% accuracy out of box
- Sufficient for most users

**Precision Calibration (optional, via iOS app):**

*Architecture: Bottle-driven, iOS-mirrored* - iOS sends START/CANCEL commands, bottle runs its state machine and broadcasts state changes, iOS mirrors with rich UI.

1. iOS sends START_CALIBRATION → Bottle enters calibration mode
2. Bottle prompts "Empty bottle" → waits for stable upright position
3. Bottle auto-measures empty ADC when stable
4. Bottle prompts "Fill to Xml" → waits for stable upright position
5. Bottle auto-measures full ADC when stable
6. Bottle calculates scale factor and stores in NVS
7. Bottle broadcasts COMPLETE state → iOS shows success
8. Achieves ~±5ml accuracy

#### Tare Calibration (required, per-bottle setup)
1. User places empty bottle on puck
2. App sends "Tare" command
3. Puck records current gram reading as `tare_weight`
4. Stored in NVS

#### User Configuration (via iOS app)
- Bottle capacity (ml) - user enters
- Daily goal (ml) - user sets

#### Calibration Data Structure (NVS)
```c
typedef struct {
    float    scale_factor;      // ADC counts per gram
    int32_t  zero_offset;       // ADC reading at zero weight
    int32_t  tare_weight_grams; // Empty bottle weight
    uint16_t bottle_capacity_ml;// User-entered capacity
    uint16_t daily_goal_ml;     // User's daily target
    uint8_t  calibration_valid; // Flag: 0=default, 1=precision calibrated
} CalibrationData;
```

---

## iOS App Requirements

### 1. Architecture
- **UI Framework:** SwiftUI
- **BLE:** CoreBluetooth
- **Storage:** CoreData (local) + HealthKit (sync)
- **Pattern:** MVVM with ObservableObject

### 2. Data Model

#### Bottle Entity
```swift
struct Bottle {
    let id: UUID
    var name: String
    var capacityMl: Int
    var tareWeight: Int
    var deviceIdentifier: String?  // BLE peripheral ID
    var lastSyncDate: Date?
}
```

#### DrinkRecord Entity
```swift
struct DrinkRecord {
    let id: UUID
    let bottleId: UUID
    let timestamp: Date
    let amountMl: Int           // Positive = consumed, negative = refill
    let bottleLevelMl: Int      // Level after this event
    var syncedToHealth: Bool
}
```

#### DailySummary Entity
```swift
struct DailySummary {
    let date: Date              // Day (midnight)
    var totalConsumedMl: Int
    var goalMl: Int
    var drinkCount: Int
    var refillCount: Int
}
```

### 3. Screens

For detailed screen specifications, layouts, and UX flows, see [iOS-UX-PRD.md](iOS-UX-PRD.md).

**Screen Summary:**
- **Home Screen:** Daily goal progress (large circular, PRIMARY), bottle level (small bar, SECONDARY), recent drinks
- **History Screen:** 7-day calendar with daily totals, drink list for selected day
- **Settings Screen:** Device configuration, commands, preferences
- **Pairing Screen:** BLE device discovery and connection
- **Calibration Wizard:** Two-point calibration flow (bottle-driven, iOS-mirrored)

### 4. BLE Manager

#### Connection Flow
1. Scan for devices advertising Aquavate service
2. Connect to known device (or show picker for new)
3. Discover services and characteristics
4. Subscribe to notifications (weight, status)
5. Initiate sync if pending records

#### Auto-Reconnection (Issue #114)
- Use `CBCentralManager.connect()` for persistent auto-reconnection in both foreground and background
- When app becomes active: set up persistent connect request + scan burst as fast-path
- When app goes to background: cancel foreground reconnect, set up background reconnect after disconnect
- On unexpected disconnect: set up persistent connect request + delayed scan burst
- Use CoreBluetooth state restoration for reconnection after app termination
- Extended firmware awake duration (4 min) when unsynced records exist enables opportunistic background sync
- No continuous background BLE scanning (iOS limitation)

### 5. HealthKit Integration

#### Data Written
- **Dietary Water** (HKQuantityTypeIdentifier.dietaryWater)
- One entry per drink event
- Timestamp from drink record
- Amount in ml

#### Sync Logic
- Sync to HealthKit after successful puck sync
- Mark records as synced to avoid duplicates
- Store HealthKit sample UUID for deletion support
- Request HealthKit authorization from Settings (opt-in)

#### Day Boundary Alignment

Aquavate uses **midnight** as the daily reset boundary, matching Apple HealthKit's day boundaries. Daily totals in Aquavate and Apple Health will align correctly.

### 6. Notifications

#### Hydration Reminders (Pace-Based Model)
Smart reminders based on whether user is on pace to meet daily goal:

| Urgency | Condition | Notification |
|---------|-----------|--------------|
| On Track (Blue) | deficit ≤ 0 | No notification |
| Attention (Amber) | 0 < deficit < 20% of goal | "Time to hydrate! You're Xml behind pace." |
| Overdue (Red) | deficit ≥ 20% of goal | "You're falling behind! Drink Xml to catch up." |

**Configuration:**
- Active hours: 7am-10pm (15 hours)
- Quiet hours: 10pm-7am (no reminders)
- Max 12 reminders per day
- Escalation model: Only notify when urgency increases
- 50ml rounding: Deficits rounded to nearest 50ml, suppressed if <150ml (50ml in DEBUG with Early Notifications enabled)

**Notification Types:**
- Hydration reminders (pace-based, during active hours)
- Goal achieved celebration ("Goal Reached! 💧")
- Back on track (optional, when user catches up after falling behind)
- Low battery warning (from puck status)
- Sync reminder if not connected for 24h

See [Plans/036-watch-hydration-reminders.md](../Plans/036-watch-hydration-reminders.md) for full implementation details.

---

## Implementation Phases

### Phase 1: Firmware Foundation (Current + Refactor)
**Files to modify/create:**
- `firmware/src/main.cpp` - Refactor to state machine
- `firmware/src/sensors/weight.cpp` - NAU7802 driver with calibration
- `firmware/src/sensors/motion.cpp` - ADXL343 driver with gesture detection
- `firmware/src/power/sleep.cpp` - Power state management

**Deliverables:**
- Stable weight measurement with tare
- Vertical detection and stability check
- Drink detection algorithm
- Wake/sleep state machine

### Phase 2: Firmware Storage & Display
**Files to create:**
- `firmware/src/storage/drinks.cpp` - NVS drink record storage
- `firmware/src/display/ui.cpp` - E-paper rendering abstraction

**Deliverables:**
- Offline drink storage (7 days)
- E-paper status display
- Empty gesture detection

### Phase 3: Firmware BLE
**Files to create:**
- `firmware/src/ble/service.cpp` - GATT service setup
- `firmware/src/ble/sync.cpp` - Drink history sync protocol

**Deliverables:**
- BLE advertising and connection
- GATT characteristics
- Drink sync protocol

### Phase 4: iOS App Foundation
**Files to create:**
- `ios/Aquavate/` - New Xcode project
- `ios/Aquavate/Models/` - CoreData models
- `ios/Aquavate/Views/` - SwiftUI views
- `ios/Aquavate/Services/BLEManager.swift`

**Deliverables:**
- Basic app structure
- BLE scanning and connection
- Local drink storage

### Phase 5: iOS App Complete
**Files to create:**
- `ios/Aquavate/Services/HealthKitManager.swift`
- `ios/Aquavate/Views/History/`
- `ios/Aquavate/Views/Setup/`

**Deliverables:**
- HealthKit integration
- Full UI implementation
- Bottle setup wizard

---

## Verification & Testing

### Firmware Testing
1. **Weight accuracy:** Compare readings to calibrated scale (target ±15ml)
2. **Wake reliability:** Verify wake from various tilt angles
3. **Stability detection:** Test measurement rejection during motion
4. **Drink detection:** Test various drink sizes (30ml, 100ml, 250ml)
5. **Empty gesture:** Verify inverted shake detection
6. **Power consumption:** Measure deep sleep current (<200µA target)
7. **BLE range:** Test connection at 5m, 10m distances

### iOS Testing
1. **BLE pairing:** Test fresh pair and reconnection
2. **Sync reliability:** Test with various record counts
3. **HealthKit:** Verify entries appear in Health app
4. **Offline handling:** Test app behavior when puck unavailable
5. **Background:** Verify reconnection after app backgrounded

---

## Design Decisions

- **OTA Updates:** Plan architecture for future OTA support but implement USB-only updates for MVP
- **Multi-bottle:** Single bottle per app install for v1 (simplifies UI and sync logic)
- **RTC:** Not included in MVP - use relative timestamps, sync absolute time from phone

## Future Considerations

### Implemented
1. ✅ **Apple Watch companion app** (Issue #27) - Syncs via iPhone using WatchConnectivity. Shows today's intake, pace-based deficit ("Xml to catch up"), and goal progress complication. Includes local notifications with haptic feedback. See [Plans/036-watch-hydration-reminders.md](../Plans/036-watch-hydration-reminders.md).

### Low Complexity (Post-MVP)
1. **iOS Home Screen Widget** - Quick glance at daily progress

### Medium Complexity
3. **OTA firmware updates** via BLE (architecture prepared, implement post-MVP)
4. **Multiple bottle support** per user account
5. **iOS 14+ App Clips** - Quick onboarding via NFC tag on bottle

### Higher Complexity / Future Hardware
6. **Water temperature sensor** for hot/cold tracking
7. **Standalone Apple Watch BLE** - Watch connects directly to puck (not recommended)
8. **Social features** - Share hydration goals with friends
