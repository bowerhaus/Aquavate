# Smart Water Bottle - Bill of Materials

## SparkFun Qwiic Ecosystem (UK Split Order)

**Created:** 2026-01-07
**Total Cost:** £75.00 (including shipping)

---

## Order 1: Pimoroni

**Website:** https://shop.pimoroni.com
**Shipping:** Royal Mail Tracked (1-2 days) - £3.90

| Qty | Component | Product | Price | Link |
|-----|-----------|---------|-------|------|
| 1 | Accelerometer | Adafruit LIS3DH Triple-Axis Accelerometer (STEMMA QT) | £4.25 | [Shop](https://shop.pimoroni.com/products/adafruit-lis3dh-triple-axis-accelerometer-2g-4g-8g-16g) |
| 1 | Battery | LiPo Battery Pack 1200mAh 3.7V (JST-PH connector) | £8.25 | [Shop](https://shop.pimoroni.com/products/lipo-battery-pack) |

| | |
|---|---|
| **Subtotal** | £12.50 |
| **Shipping** | £3.90 |
| **Order Total** | **£16.40** |

**Why Pimoroni:** Ordering the battery from Pimoroni allows Royal Mail shipping. The Pi Hut requires DHL (£9.99) for LiPo batteries.

---

## Order 2: The Pi Hut

**Website:** https://thepihut.com
**Shipping:** Royal Mail 1st Class (1-2 days) - £3.90

| Qty | Component | Product | Price | Link |
|-----|-----------|---------|-------|------|
| 1 | Microcontroller | SparkFun Qwiic Pocket Development Board - ESP32-C6 | £20.50 | [Shop](https://thepihut.com/products/sparkfun-qwiic-pocket-development-board-esp32-c6) |
| 1 | Load Cell ADC | SparkFun Qwiic Scale - NAU7802 | £17.80 | [Shop](https://thepihut.com/products/sparkfun-qwiic-scale-nau7802) |
| 1 | E-Paper Display | Waveshare E-Ink Display Module SPI - 1.54" (200x200) | £9.60 | [Shop](https://thepihut.com/products/eink-display-module-spi-1-54-200x200) |
| 1 | Load Cell | Strain Gauge Load Cell - 4 Wires - 5Kg | £3.80 | [Shop](https://thepihut.com/products/strain-gauge-load-cell-4-wires-5kg) |
| 3 | Cables | STEMMA QT / Qwiic JST SH 4-pin Cable - 100mm | £3.00 | [Shop](https://thepihut.com/products/stemma-qt-qwiic-jst-sh-4-pin-cable-100mm-long) |

| | |
|---|---|
| **Subtotal** | £54.70 |
| **Shipping** | £3.90 |
| **Order Total** | **£58.60** |

---

## Grand Total

| | |
|---|---|
| Pimoroni Order | £16.40 |
| The Pi Hut Order | £58.60 |
| **Grand Total** | **£75.00** |

---

## Stock Levels (as of 2026-01-07)

| Component | Stock Level | Status |
|-----------|-------------|--------|
| ESP32-C6 Qwiic Pocket | 14 units | Low stock |
| SparkFun Qwiic Scale NAU7802 | 11 units | Low stock |
| SparkFun LIS3DH Qwiic | 13 units | Low stock |
| Waveshare 1.54" E-Paper | 6 units | Low stock |
| 5kg Load Cell | 53 units | Good |
| LIS3DH (Pimoroni) | In stock | Good |
| 1200mAh LiPo (Pimoroni) | In stock | Good |
| STEMMA QT Cables | 275 units | Good |

---

## Key Differences from Adafruit Option

| Feature | SparkFun Qwiic | Adafruit Feather |
|---------|----------------|------------------|
| **Total Cost** | £75.00 | £70.80 |
| **MCU** | ESP32-C6 (RISC-V, WiFi 6) | ESP32 (Xtensa, WiFi 4) |
| **Form Factor** | 1" × 1" Qwiic standard | Feather format |
| **E-Paper** | Requires SPI wiring (6 wires) | Stacks on top (no wires) |
| **Battery Charging** | Built-in (213mA) | Built-in (500mA) |
| **Deep Sleep** | ~14μA | ~5μA |
| **Soldering** | Load cell wires only | Stacking headers + load cell |

---

## Assembly Notes

### Wiring Required

Unlike the Adafruit Feather option, the SparkFun Qwiic setup requires manual wiring for the e-paper display.

```
┌─────────────────────────────────────────────────────────────┐
│      SparkFun ESP32-C6 Qwiic Pocket                         │
│                                                             │
│  Qwiic ──┬── LIS3DH (INT1 → GPIO for wake)                  │◄── 1 Qwiic cable
│          └── NAU7802 ◄── Load Cell                          │◄── Daisy-chain + 4 wires
│                                                             │
│  GPIO Pins ── Waveshare 1.54" E-Paper (SPI)                 │◄── 7 wires (see below)
│                                                             │
│  JST Battery ◄── 1200mAh LiPo                               │◄── Plug in (built-in charging)
└─────────────────────────────────────────────────────────────┘
```

### E-Paper SPI Wiring (ESP32-C6 to Waveshare 1.54")

| Waveshare Pin | ESP32-C6 Pin | Notes |
|---------------|--------------|-------|
| VCC | 3V3 | 3.3V power |
| GND | GND | Ground |
| DIN (MOSI) | GPIO7 | SPI data |
| CLK (SCK) | GPIO6 | SPI clock |
| CS | GPIO5 | Chip select |
| DC | GPIO4 | Data/Command |
| RST | GPIO3 | Reset |
| BUSY | GPIO2 | Busy signal |

### Load Cell Wiring (to NAU7802 terminal block)

| Load Cell Wire | NAU7802 Terminal |
|----------------|------------------|
| Red | E+ |
| Black | E- |
| White | A- |
| Green | A+ |

### I2C Addresses

| Device | Address |
|--------|---------|
| NAU7802 | 0x2A |
| LIS3DH | 0x18 (default) or 0x19 |

---

## Component Specifications

### SparkFun ESP32-C6 Qwiic Pocket
- Single-core RISC-V @ 160MHz
- 4MB Flash
- WiFi 6 (802.11ax) + Bluetooth 5.3 + Zigbee/Thread
- Built-in MCP73831 LiPo charger (213mA)
- USB-C for programming and charging
- Qwiic connector for I2C
- 2-pin JST battery connector
- Deep sleep: ~14μA
- Board size: 1" × 1" (25.4mm × 25.4mm)

### SparkFun Qwiic Scale NAU7802
- 24-bit ADC resolution
- Up to 320 samples/second
- On-chip PGA (1x to 128x gain)
- I2C interface (Qwiic connector)
- Spring terminal connectors for load cell
- On-chip temperature sensor

### LIS3DH Accelerometer (Adafruit STEMMA QT version)
- ±2g/±4g/±8g/±16g selectable range
- 10-bit resolution
- I2C or SPI interface
- Interrupt pins for wake-on-motion
- Ultra-low power: 2μA in low-power mode
- STEMMA QT connector (Qwiic compatible)

### Waveshare 1.54" E-Paper Display
- 200×200 pixels, monochrome
- SPI interface
- Embedded controller
- Partial refresh support
- Near-zero power when static
- Wide viewing angle

### Load Cell
- Capacity: 5kg
- 4-wire Wheatstone bridge
- Aluminum alloy construction
- Output: ~1mV/V

### Battery
- 1200mAh capacity
- 3.7V nominal (4.2V full, 3.0V cutoff)
- JST-PH 2mm connector
- Built-in protection circuit

---

## PlatformIO Configuration

```ini
; platformio.ini for SparkFun ESP32-C6 Qwiic Pocket
[env:esp32-c6-devkitc-1]
platform = espressif32
board = esp32-c6-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps =
    sparkfun/SparkFun Qwiic Scale NAU7802@^1.0.0
    adafruit/Adafruit LIS3DH@^1.2.0
    zinggjm/GxEPD2@^1.5.0
    h2zero/NimBLE-Arduino@^1.4.0
```

---

## Comparison: Single vs Split Order

| | Single (Pi Hut only) | Split Order |
|---|---|---|
| Components | £67.50 | £67.20 |
| Shipping | £9.99 (DHL required for LiPo) | £7.80 (Royal Mail both) |
| **Total** | **£77.49** | **£75.00** |
| Delivery | Next working day | 1-2 days each |

Split order saves **£2.49** and allows Royal Mail for both orders.

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2026-01-07 | 1.0 | Initial BOM - UK split order (Pimoroni + The Pi Hut) |
