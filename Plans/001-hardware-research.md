# Smart Water Bottle - Hardware Options Analysis

## Project Overview
A smart water bottle that measures daily water intake using a weight-based sensor system, communicates with an iOS app via Bluetooth/WiFi, and displays status on an e-paper display.

---

## Component Categories

### 1. Microcontroller Options

| Option | Pros | Cons | Cost |
|--------|------|------|------|
| **ESP32-C3/S3** | Built-in WiFi + BLE, large community, cheap, good documentation | Higher power consumption (12μA deep sleep), BLE not as optimized as nRF | ~$3-5 |
| **nRF52840** | Best-in-class BLE 5.2, ultra-low power (2.6μA sleep, 15mA TX), ideal for battery | No WiFi, more expensive, smaller community | ~$10-20 |
| **nRF52832** | Great BLE, lower cost than nRF52840, very low power | No WiFi, less RAM/Flash than nRF52840 | ~$6-12 |
| **STM32WB55** | Dual-core (M4+M0+ for radio), industrial grade, good power management | Steeper learning curve, no WiFi, requires more external components | ~$8-15 |

**Market Reference:** HidrateSpark uses BLE (not WiFi) with 21-day battery life, suggesting nRF52 or similar is likely their choice.

---

### 2. Weight Sensor (Load Cell) Options

| Option | Pros | Cons | Notes |
|--------|------|------|-------|
| **Strain Gauge Load Cell + HX711** | Very cheap (~$2-5), 24-bit ADC, widely documented | Higher power, slower (10-80 SPS), larger form factor | Good for prototyping |
| **Strain Gauge Load Cell + NAU7802** | I2C interface, lower power, better noise performance | Slightly more expensive (~$5-8) | Better for production |
| **Button/Compression Load Cells** | Very small form factor, good for compact designs | May need custom mounting | 50g-5kg range typical |
| **Thin Film Load Cells** | Ultra-thin, flexible mounting options | Less accurate, may need calibration | Emerging technology |

**HidrateSpark Approach:** Uses a "sensor puck" that twists into the bottle base with load cell technology. Claims 97% accuracy, works with ice.

---

### 3. E-Paper Display Options

| Option | Size | Resolution | Interface | Notes |
|--------|------|------------|-----------|-------|
| **Waveshare 1.02"** | 1.02" | 128×80 | SPI | Very small, good for minimal UI |
| **Waveshare 1.54"** | 1.54" | 200×200 | SPI | Good balance of size/readability |
| **Waveshare 2.13"** | 2.13" | 250×122 | SPI | More room for graphics |
| **Good Display GDEW series** | Various | Various | SPI/I2C | Alternative supplier |

**E-Paper Benefits:** Near-zero power when not refreshing, readable in sunlight, perfect for battery-powered devices.

---

### 4. Battery Management Options

| Option | Pros | Cons | Cost |
|--------|------|------|------|
| **TP4056** | Very cheap, 1A charging, common | Cannot power load while charging, 8V max input | ~$0.50-1 |
| **BQ24072/BQ24075** | Power path management (charge + power simultaneously), USB compliant | More expensive, requires more design work | ~$3-5 |
| **MCP73831** | Small, simple, Microchip quality | Only 500mA charge, 6V max input | ~$1-2 |
| **BQ25100** | Ultra-low power, tiny (1x1.5mm), up to 250mA | Limited charge current | ~$2-3 |

**Recommendation:** BQ24072/BQ24075 for production (power path), TP4056 for initial prototyping.

---

### 5. Communication Protocol

| Option | Pros | Cons |
|--------|------|------|
| **BLE (Recommended)** | Low power, excellent iOS support, no network dependency, fast pairing | Limited range (~30m), requires phone proximity |
| **WiFi** | Longer range, direct cloud connectivity, no phone needed for sync | Much higher power, needs network credentials, complex setup |

**Market Standard:** HidrateSpark, Thermos Connected, and most smart bottles use BLE exclusively.

---

### 6. Real-Time Clock (RTC) Options

For standalone operation (tracking water intake without phone sync), an external RTC is required. The ESP32's internal RTC drifts ~5-10% (30-60 minutes/week), which is unacceptable for accurate daily tracking.

| Option | Accuracy | Power | Backup Battery | Cost |
|--------|----------|-------|----------------|------|
| **DS3231 (Recommended)** | ±2ppm (~1 min/year) | 2-3μA | Built-in CR1220 | ~£11.50 |
| **RV-8803** | ±3ppm | 0.24μA (lowest) | External needed | ~£12 |
| **PCF8523** | ±20ppm (~10 min/year) | 0.1μA | Built-in CR1220 | ~£5-6 |
| **DS1307** | ±20ppm | Higher | CR2032 | ~£3-5 |

**Recommendation:** DS3231 STEMMA QT/Qwiic for both ecosystems. Best accuracy, built-in battery backup, excellent Arduino RTClib support.

**Why not use internal RTC?** After 1 week, ESP32 internal RTC can drift 30-60 minutes, causing daily totals to reset at the wrong time.

---

## Preliminary Hardware Recommendations

### For Prototyping:
- **MCU:** ESP32-C3 (cheap, easy to program, has both WiFi and BLE for testing)
- **Sensor:** Button load cell + HX711 module
- **Display:** Waveshare 1.54" e-paper
- **Battery:** TP4056 module + 500-1000mAh LiPo

### For Production:
- **MCU:** nRF52832 or nRF52840 (optimal BLE + power)
- **Sensor:** Load cell + NAU7802 ADC
- **Display:** Custom e-paper size based on enclosure
- **Battery:** BQ24072 IC + custom battery selection

---

## User Requirements (Confirmed)

| Requirement | Selection |
|-------------|-----------|
| **Battery Life** | 1-2 weeks between charges |
| **Production Scale** | Prototype only |
| **Accuracy** | ±15ml (medium/consumer grade) |
| **Water Resistance** | Not critical (indoor use) |
| **Connectivity** | BLE preferred, open to WiFi |
| **Display Content** | Current level + daily total |
| **Dev Environment** | Arduino/PlatformIO |
| **Power Strategy** | Deep sleep with wake-on-tilt/placement |

---

## Final Prototype Recommendation

Based on your requirements, here is the recommended hardware stack:

### Primary Components

| Component | Recommendation | Cost | Rationale |
|-----------|----------------|------|-----------|
| **Microcontroller** | **ESP32-C3 Mini** | ~$4-5 | Excellent Arduino/PlatformIO support, BLE 5.0, WiFi backup, 5μA deep sleep, large community |
| **Load Cell** | **50kg Bar Load Cell** | ~$2-3 | Standard strain gauge, fits bottle base form factor |
| **Load Cell ADC** | **HX711 Module** | ~$2-3 | 24-bit ADC, simple interface, extensive Arduino libraries |
| **E-Paper Display** | **Waveshare 1.54" B/W** | ~$10-12 | 200×200 resolution, SPI interface, good Arduino support |
| **Accelerometer** | **LIS3DH or MPU6050** | ~$2-4 | Wake-on-motion interrupt for power saving |
| **Real-Time Clock** | **DS3231 STEMMA QT** | ~$12-15 | ±2ppm accuracy, battery backup, enables standalone operation |
| **Battery Charger** | **TP4056 Module** | ~$1-2 | USB-C input, simple for prototype |
| **Battery** | **3.7V 1000mAh LiPo** | ~$5-8 | Sufficient for 1-2 week runtime with sleep strategy |

**Estimated Prototype BOM: ~$40-50**

**Standalone Operation:** With the DS3231 RTC, the bottle can track water intake accurately over multiple days without requiring phone sync. Time is preserved during deep sleep and even if the main battery is depleted (RTC has backup coin cell).

### Why ESP32-C3 Over Alternatives

| MCU Option | Deep Sleep | BLE Idle | Arduino Support | Decision |
|------------|------------|----------|-----------------|----------|
| **ESP32-C3** | ~5μA | ~100μA | Excellent | ✓ Selected |
| nRF52840 | ~2.6μA | ~15mA TX | Good (XIAO) | Backup option |
| Pico 2 W | ~150μA | 30-40mA | Good | Too power-hungry for BLE |
| STM32WB | <2μA | Good | Steeper curve | Overkill for prototype |

### Fallback Option

If BLE range or power proves insufficient with ESP32-C3:

| Component | Alternative | Cost | When to Consider |
|-----------|-------------|------|------------------|
| **Microcontroller** | **XIAO nRF52840** | ~$15-25 | If BLE connectivity is problematic |

The XIAO nRF52840 has good Arduino support and is pin-compatible with XIAO ESP32C3.

---

## Power Budget Analysis

### Estimated Current Draw (ESP32-C3)

| State | Current | Duration | Notes |
|-------|---------|----------|-------|
| Deep Sleep | ~5μA | 99% of time | Internal RTC + accelerometer interrupt active |
| Wake + Measure | ~30mA | ~500ms | Load cell reading + RTC query + processing |
| BLE Sync | ~100mA | ~1-2s | When phone app requests data |
| E-Paper Refresh | ~20mA | ~2s | Only when display changes |

**External RTC (DS3231) Power:**
- The DS3231 has its own backup coin cell battery (CR1220)
- During normal operation, it draws ~2-3μA from the I2C bus
- During deep sleep, it runs from its own battery (0μA from main system)
- Backup battery lasts years, preserving time even if main battery is depleted

### Battery Life Estimate (1000mAh)

With wake-on-tilt (assuming 20 drink events/day):
- Sleep: 23.9 hours × 5μA = 0.12mAh/day
- Wake cycles: 20 × 0.5s × 30mA = 0.08mAh/day
- BLE syncs: 5 × 2s × 100mA = 0.28mAh/day
- Display: 5 × 2s × 20mA = 0.06mAh/day
- **Total: ~0.5mAh/day → ~2000 days theoretical**

Reality will be higher due to inefficiencies, but **1-2 weeks is very achievable**.

**Note:** The DS3231 RTC adds negligible power draw (<5% impact on battery life) while enabling standalone time tracking.

---

## Wake-on-Event Strategy

The accelerometer enables smart power management:

```
[Deep Sleep] → [Tilt Detected] → [Wake Up]
                                    ↓
                            [Read Load Cell]
                                    ↓
                            [Weight Changed?]
                              ↓           ↓
                            [Yes]       [No]
                              ↓           ↓
                    [Update Display]  [Back to Sleep]
                    [Queue BLE Data]
                              ↓
                    [Back to Sleep]
```

**Sync Strategy Options:**
1. **On-demand**: Phone app requests data via BLE when opened
2. **Periodic beacon**: Wake every 15-30 min to advertise BLE briefly
3. **Event-triggered**: Sync after each significant drink event

---

## Hardware Ecosystem Options

Two plug-and-play ecosystems are recommended. Both use I2C connectors (STEMMA QT / Qwiic are cross-compatible).

---

### Option A: Adafruit Feather Ecosystem (Minimal Wiring)

**Best for:** Easiest prototyping - e-paper stacks directly on board, no display wiring needed.

| Component | Product | Price | Link |
|-----------|---------|-------|------|
| **Main Board** | Adafruit ESP32 Feather V2 (STEMMA QT) | ~$20 | [Adafruit 5400](https://www.adafruit.com/product/5400) |
| **E-Paper Display** | 2.13" Mono FeatherWing (250x122) | ~$22 | [Adafruit 4195](https://www.adafruit.com/product/4195) |
| **Load Cell ADC** | NAU7802 24-bit ADC STEMMA QT | ~$10 | [Adafruit 4538](https://www.adafruit.com/product/4538) |
| **Accelerometer** | LIS3DH Triple-Axis STEMMA QT | ~$5 | [Adafruit 2809](https://www.adafruit.com/product/2809) |
| **Real-Time Clock** | DS3231 Precision RTC STEMMA QT | ~$14 | [Adafruit 5188](https://www.adafruit.com/product/5188) |
| **Load Cell** | Strain Gauge Load Cell (1-5kg) | ~$8 | [Adafruit 4540](https://www.adafruit.com/product/4540) |
| **Battery** | 3.7V 1200mAh LiPo | ~$10 | [Adafruit 258](https://www.adafruit.com/product/258) |
| **Cables** | STEMMA QT cables (100mm, 5-pack) | ~$4 | [Adafruit 4399](https://www.adafruit.com/product/4399) |

**Total: ~$93**

**Wiring Diagram:**
```
┌─────────────────────────────────────────┐
│      Adafruit ESP32 Feather V2          │
│  ┌─────────────────────────────────┐    │
│  │   2.13" E-Paper FeatherWing     │    │◄── Stacks on top (NO WIRES!)
│  └─────────────────────────────────┘    │
│                                         │
│  STEMMA QT ──┬── LIS3DH ────────────────│◄── Qwiic cable (daisy-chain)
│              ├── NAU7802 ◄── Load Cell  │◄── Qwiic cable + 4 wires to cell
│              └── DS3231 RTC ────────────│◄── Qwiic cable (daisy-chain)
│                                         │
│  JST Battery ◄── 1200mAh LiPo           │◄── Plug in (built-in charging)
└─────────────────────────────────────────┘

Manual wiring: 4 wires (load cell to NAU7802 terminal block)
```

**Pros:**
- ✅ E-paper stacks directly - zero display wiring
- ✅ Built-in LiPo charging via USB-C
- ✅ Built-in battery voltage monitoring
- ✅ STEMMA QT daisy-chaining
- ✅ Excellent Adafruit libraries
- ✅ 8MB Flash, 2MB PSRAM

**Cons:**
- ❌ More expensive (~$79)
- ❌ Larger form factor than XIAO
- ❌ ESP32 (not C3) - slightly higher power, but still good

---

### Option B: SparkFun Qwiic Ecosystem (More Flexible)

**Best for:** Smaller form factor, more display size options, ESP32-C6 (newer chip).

| Component | Product | Price | Link |
|-----------|---------|-------|------|
| **Main Board** | SparkFun ESP32-C6 Qwiic Pocket | ~$18 | [SparkFun 22925](https://www.sparkfun.com/products/22925) |
| **E-Paper Display** | Waveshare 1.54" or 2.13" B/W | ~$12 | [Waveshare](https://www.waveshare.com/1.54inch-e-paper-module.htm) |
| **Load Cell ADC** | SparkFun Qwiic Scale NAU7802 | ~$17 | [SparkFun 15242](https://www.sparkfun.com/products/15242) |
| **Accelerometer** | SparkFun LIS3DH Qwiic Mini | ~$10 | [SparkFun 18403](https://www.sparkfun.com/products/18403) |
| **Real-Time Clock** | DS3231 Precision RTC STEMMA QT | ~$14 | [Adafruit 5188](https://www.adafruit.com/product/5188) |
| **Load Cell** | Mini Load Cell (5kg) | ~$10 | [SparkFun 14728](https://www.sparkfun.com/products/14728) |
| **Battery Charger** | TP4056 USB-C Module | ~$2 | Amazon/AliExpress |
| **Battery** | 3.7V 1000mAh LiPo | ~$6 | Various |
| **Qwiic Cables** | 100mm cables (5-pack) | ~$2 | [SparkFun 17259](https://www.sparkfun.com/products/17259) |

**Total: ~$91**

**Wiring Diagram:**
```
┌─────────────────────────────────────────┐
│      SparkFun ESP32-C6 Qwiic Pocket     │
│                                         │
│  Qwiic ──┬── LIS3DH (INT → GPIO wake)   │◄── Qwiic cable + 1 INT wire
│          ├── NAU7802 ◄── Load Cell      │◄── Daisy-chain + 4 wires to cell
│          └── DS3231 RTC ────────────────│◄── Daisy-chain (Qwiic compatible)
│                                         │
│  SPI Pins ── Waveshare E-Paper          │◄── 6 wires (MOSI,CLK,CS,DC,RST,BUSY)
│                                         │
│  Power: TP4056 → Battery → Board        │◄── Power circuit wiring
└─────────────────────────────────────────┘

Manual wiring: ~12 wires total (e-paper SPI + power + INT)
Note: STEMMA QT and Qwiic use the same connector - fully compatible.
```

**Pros:**
- ✅ ESP32-C6 (newer, WiFi 6, Thread/Zigbee capable)
- ✅ Smaller Qwiic Pocket form factor
- ✅ More e-paper size options (Waveshare variety)
- ✅ Slightly cheaper (~$77)
- ✅ SparkFun Qwiic Scale has built-in load cell connector

**Cons:**
- ❌ E-paper requires manual SPI wiring (6 wires)
- ❌ External battery charging circuit needed
- ❌ No built-in battery monitoring
- ❌ More total wiring than Adafruit

---

### Ecosystem Comparison Summary

| Factor | Adafruit Feather | SparkFun Qwiic |
|--------|------------------|----------------|
| **Total Cost** | ~$93 | ~$91 |
| **Wiring Complexity** | **Lowest** (~4 wires) | Medium (~12 wires) |
| **E-Paper Setup** | **Stacks on top** | Manual SPI |
| **Battery Charging** | **Built-in** | External TP4056 |
| **MCU** | ESP32 (dual-core) | ESP32-C6 (RISC-V) |
| **Form Factor** | Medium | Smaller |
| **RTC** | DS3231 STEMMA QT | DS3231 STEMMA QT (Qwiic compatible) |
| **Standalone Operation** | ✅ Yes | ✅ Yes |
| **Best For** | Fastest prototype | Smaller final product |

---

## Development Environment: PlatformIO + C++

### Setup Instructions

**1. Install VS Code + PlatformIO**
```bash
# Install VS Code from https://code.visualstudio.com
# Install PlatformIO IDE extension from VS Code marketplace
```

**2. Create Project (Adafruit Feather)**
```ini
; platformio.ini
[env:adafruit_feather_esp32_v2]
platform = espressif32
board = adafruit_feather_esp32_v2
framework = arduino
monitor_speed = 115200
lib_deps =
    adafruit/Adafruit NAU7802@^1.0.0
    adafruit/Adafruit LIS3DH@^1.2.0
    adafruit/Adafruit EPD@^4.5.0
    adafruit/RTClib@^2.1.0
    h2zero/NimBLE-Arduino@^1.4.0
```

**3. Create Project (SparkFun ESP32-C6)**
```ini
; platformio.ini
[env:esp32-c6-devkitc-1]
platform = espressif32
board = esp32-c6-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps =
    sparkfun/SparkFun Qwiic Scale NAU7802@^1.0.0
    sparkfun/SparkFun LIS3DH@^1.0.0
    zinggjm/GxEPD2@^1.5.0
    adafruit/RTClib@^2.1.0
    h2zero/NimBLE-Arduino@^1.4.0
```

### Key Libraries

| Function | Library | Notes |
|----------|---------|-------|
| **BLE** | NimBLE-Arduino | Lighter than default ESP32 BLE |
| **E-Paper** | GxEPD2 or Adafruit EPD | Waveshare / Adafruit displays |
| **Load Cell** | SparkFun NAU7802 | Works with both ecosystems |
| **Accelerometer** | Adafruit LIS3DH | I2C, interrupt support |
| **Real-Time Clock** | Adafruit RTClib | DS3231 support, time management |
| **Deep Sleep** | ESP32 built-in | `esp_deep_sleep_start()` |

### Development Workflow

```
┌─────────────────────────────────────────────────────────────┐
│  1. EDIT in VS Code                                         │
│     └── IntelliSense, auto-complete, error highlighting     │
│                                                             │
│  2. BUILD (Ctrl+Alt+B)                    ⏱️ 5-10s           │
│     └── Incremental compilation (cached)                    │
│                                                             │
│  3. UPLOAD (Ctrl+Alt+U)                   ⏱️ 10-15s          │
│     └── USB @ 460800 baud (reliable)                        │
│                                                             │
│  4. MONITOR (Ctrl+Alt+S)                  Instant           │
│     └── Serial output for debugging                         │
│                                                             │
│  Total iteration: ~20-30 seconds                            │
└─────────────────────────────────────────────────────────────┘
```

### Debugging Strategy

**Primary: Serial.print()**
```cpp
#define DEBUG 1
#if DEBUG
  #define DEBUG_PRINT(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
#endif

void loop() {
    float weight = scale.getWeight();
    DEBUG_PRINT("Weight: " + String(weight) + "g");
}
```

**Later: OTA Updates**
```cpp
#include <ArduinoOTA.h>
// Upload wirelessly when device is in enclosure
```

---

## Suppliers Summary

| Component | Adafruit | SparkFun | Alternative |
|-----------|----------|----------|-------------|
| Main Board | [Feather ESP32 V2](https://www.adafruit.com/product/5400) | [Qwiic ESP32-C6](https://www.sparkfun.com/products/22925) | - |
| E-Paper | [FeatherWing 2.13"](https://www.adafruit.com/product/4195) | [Waveshare](https://www.waveshare.com) | Amazon |
| NAU7802 | [STEMMA QT](https://www.adafruit.com/product/4538) | [Qwiic Scale](https://www.sparkfun.com/products/15242) | - |
| LIS3DH | [STEMMA QT](https://www.adafruit.com/product/2809) | [Qwiic Mini](https://www.sparkfun.com/products/18403) | - |
| Load Cell | [Adafruit](https://www.adafruit.com/product/4540) | [SparkFun](https://www.sparkfun.com/products/14728) | Amazon |
| Battery | [Adafruit](https://www.adafruit.com/product/258) | Various | Amazon |

---

## iOS App Development

### Recommended Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| **UI Framework** | SwiftUI | Modern, declarative, easy state management |
| **BLE Framework** | CoreBluetooth (native) | Full control, no dependencies, well-documented |
| **Data Storage** | CoreData | Local history, offline support |
| **Charts** | SwiftCharts (iOS 16+) | Native Apple charting |
| **Architecture** | MVVM + ObservableObject | Clean separation, reactive updates |

### App Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     iOS App Structure                        │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐   │
│  │   SwiftUI    │    │  BLEManager  │    │  DataStore   │   │
│  │    Views     │◄──►│ (Observable) │◄──►│  (CoreData)  │   │
│  └──────────────┘    └──────────────┘    └──────────────┘   │
│                              │                               │
│                              ▼                               │
│                    ┌──────────────────┐                      │
│                    │  CoreBluetooth   │                      │
│                    │  CBCentralManager│                      │
│                    └──────────────────┘                      │
│                              │                               │
│                              ▼                               │
│                    ┌──────────────────┐                      │
│                    │   Smart Bottle   │                      │
│                    │   (ESP32 BLE)    │                      │
│                    └──────────────────┘                      │
└─────────────────────────────────────────────────────────────┘
```

### BLE Service Design (Custom GATT)

```
Service: Water Tracking Service
UUID: Custom (e.g., 0x1234)

Characteristics:
├── Current Weight (Read, Notify)
│   UUID: 0x1235 | Format: Float32 (grams)
│
├── Daily Total (Read, Notify)
│   UUID: 0x1236 | Format: Float32 (ml)
│
├── Daily Goal (Read, Write)
│   UUID: 0x1237 | Format: UInt16 (ml)
│
├── Battery Level (Read, Notify)
│   UUID: 0x2A19 (Standard) | Format: UInt8 (%)
│
└── Timestamp (Read)
    UUID: 0x1238 | Format: UInt32 (Unix time)
```

### Core BLEManager Class

```swift
import CoreBluetooth
import Combine

class BLEManager: NSObject, ObservableObject {
    @Published var isConnected = false
    @Published var currentWeight: Float = 0
    @Published var dailyTotal: Float = 0
    @Published var batteryLevel: Int = 100

    private var centralManager: CBCentralManager!
    private var waterBottle: CBPeripheral?

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    func startScanning() {
        centralManager.scanForPeripherals(
            withServices: [waterServiceUUID],
            options: nil
        )
    }
}

extension BLEManager: CBCentralManagerDelegate, CBPeripheralDelegate {
    // Implement discovery, connection, characteristic reading
}
```

### App Features by Priority

| Priority | Feature | Implementation |
|----------|---------|----------------|
| **P0** | Device Pairing | Scan + Connect via BLE |
| **P0** | Live Weight | Subscribe to BLE notifications |
| **P0** | Daily Progress | Ring chart (SwiftUI) |
| **P1** | History Charts | SwiftCharts weekly/monthly |
| **P1** | Reminders | Local notifications |
| **P1** | Goal Setting | Write to BLE characteristic |
| **P2** | Home Widget | WidgetKit |
| **P2** | Health Sync | HealthKit water intake |
| **P3** | Apple Watch | WatchKit companion app |

### Reference Projects (Open Source)

| Project | Description |
|---------|-------------|
| [Swift-ESP32-BLE-Demo](https://github.com/ArtsemiR/Swift-ESP32-BLE-Remote-Control-Demo) | SwiftUI + ESP32 BLE control |
| [BLE-iOS-demo](https://github.com/dzindra/BLE-iOS-demo) | ESP32 sensor communication |
| [ESP32-BLE-OTA-SwiftUI](https://github.com/ClaesClaes/Arduino-ESP32-BLE-OTA-iOS-SwiftUI) | OTA updates from iOS |
| [Nordic BLE Library](https://github.com/NordicSemiconductor/IOS-BLE-Library) | Modern async BLE wrapper |

### Testing Tools

| Tool | Purpose |
|------|---------|
| **nRF Connect (iOS)** | Debug BLE services on ESP32 |
| **LightBlue** | Alternative BLE scanner |
| **CoreBluetoothMock** | Unit testing without hardware |
| **SwiftUI Previews** | UI iteration without device |

---

## Next Steps

### Immediate (Hardware Selection)
1. **Choose ecosystem** - Adafruit (easier) or SparkFun (smaller)
2. **Order components** - See shopping lists above
3. **Set up PlatformIO** - Install VS Code + extension

### Phase 1: Breadboard Prototype
4. **Assemble hardware** - Connect all components
5. **Test individual sensors** - Verify each works independently
6. **Calibrate load cell** - Use known weights
7. **Test deep sleep + wake** - Verify power management

### Phase 2: Firmware Development
8. **Implement core logic** - Weight measurement + change detection
9. **Add BLE service** - Custom GATT for water tracking
10. **Add e-paper UI** - Display current level + daily total
11. **Optimize power** - Tune sleep/wake thresholds

### Phase 3: iOS App
12. **Create iOS app** - SwiftUI + CoreBluetooth
13. **Implement BLE pairing** - Connect to bottle
14. **Add data visualization** - Daily/weekly charts
15. **Add notifications** - Hydration reminders

### Phase 4: Enclosure & Polish
16. **Design 3D enclosure** - Bottle base attachment
17. **Final assembly** - Fit all components
18. **End-to-end testing** - Real-world usage
19. **Create PRD** - Product Requirements Document

---

## Sources
- [ESP32 vs nRF52 vs STM32 Comparison](https://www.socketxp.com/iot/esp32-vs-arduino-stm32-raspberry-pi-pico-nrf52-best-iot-microcontroller/)
- [STM32 vs ESP32 2025 Guide](https://medium.com/@QuarkAndCode/stm32-vs-esp32-2025-which-microcontroller-for-iot-projects-65c8157e98ef)
- [TP4056 Battery Charger Guide](https://www.best-microcontroller-projects.com/tp4056.html)
- [BQ24072 Datasheet](https://cdn.sparkfun.com/assets/learn_tutorials/5/3/0/bq24075.pdf)
- [HidrateSpark Official Site](https://hidratespark.com/)
- [HidrateSpark Technology](https://hidratespark.com/blogs/hidrate-spark/introducing-hidratespark-pro-32oz-the-largest-smart-water-bottle-yet)
