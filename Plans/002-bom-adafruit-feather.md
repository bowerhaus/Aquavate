# Smart Water Bottle - Bill of Materials

## Adafruit Feather Ecosystem (UK Split Order)

**Created:** 2026-01-07
**Total Cost:** £70.80 (including shipping)

---

## Order 1: Pimoroni

**Website:** https://shop.pimoroni.com
**Shipping:** Royal Mail Tracked (1-2 days) - £3.90

| Qty | Component | Product | Price | Link |
|-----|-----------|---------|-------|------|
| 1 | Microcontroller | Adafruit ESP32 Feather V2 - 8MB Flash + 2MB PSRAM (STEMMA QT) | £16.25 | [Shop](https://shop.pimoroni.com/products/adafruit-esp32-feather-v2-8mb-flash-2-mb-psram-stemma-qt) |
| 1 | Load Cell ADC | Adafruit NAU7802 24-Bit ADC - STEMMA QT / Qwiic | £4.75 | [Shop](https://shop.pimoroni.com/products/adafruit-nau7802-24-bit-adc-stemma-qt-qwiic) |
| 1 | Accelerometer | Adafruit LIS3DH Triple-Axis Accelerometer (STEMMA QT) | £4.25 | [Shop](https://shop.pimoroni.com/products/adafruit-lis3dh-triple-axis-accelerometer-2g-4g-8g-16g) |
| 1 | Battery | LiPo Battery Pack 1200mAh 3.7V (JST-PH connector) | £8.25 | [Shop](https://shop.pimoroni.com/products/lipo-battery-pack) |

| | |
|---|---|
| **Subtotal** | £33.50 |
| **Shipping** | £3.90 |
| **Order Total** | **£37.40** |

---

## Order 2: The Pi Hut

**Website:** https://thepihut.com
**Shipping:** Royal Mail 1st Class (1-2 days) - £3.90

| Qty | Component | Product | Price | Link |
|-----|-----------|---------|-------|------|
| 1 | E-Paper Display | Adafruit 2.13" Monochrome eInk/ePaper Display FeatherWing (250x122, SSD1680) | £21.60 | [Shop](https://thepihut.com/products/adafruit-2-13-monochrome-eink-epaper-display-featherwing-ada4195) |
| 1 | Headers | Stacking Headers for Feather - 12-pin and 16-pin female headers | £1.10 | [Shop](https://thepihut.com/products/adafruit-feather-stacking-headers-12-pin-and-16-pin-female-headers) |
| 1 | Load Cell | Strain Gauge Load Cell - 4 Wires - 5Kg | £3.80 | [Shop](https://thepihut.com/products/strain-gauge-load-cell-4-wires-5kg) |
| 3 | Cables | STEMMA QT / Qwiic JST SH 4-pin Cable - 100mm | £3.00 | [Shop](https://thepihut.com/products/stemma-qt-qwiic-jst-sh-4-pin-cable-100mm-long) |

| | |
|---|---|
| **Subtotal** | £29.50 |
| **Shipping** | £3.90 |
| **Order Total** | **£33.40** |

---

## Grand Total

| | |
|---|---|
| Pimoroni Order | £37.40 |
| The Pi Hut Order | £33.40 |
| **Grand Total** | **£70.80** |

---

## Stock Warnings

| Component | Stock Level | Action |
|-----------|-------------|--------|
| 2.13" E-Paper FeatherWing | **2 units only** | Order immediately |
| SparkFun Qwiic Scale NAU7802 | 11 units | Low stock |

---

## Assembly Notes

### Soldering Required
- Solder the stacking headers to the ESP32 Feather V2
- This allows the e-paper FeatherWing to stack directly on top

### Wiring Diagram
```
┌─────────────────────────────────────────┐
│      Adafruit ESP32 Feather V2          │
│  ┌─────────────────────────────────┐    │
│  │   2.13" E-Paper FeatherWing     │    │◄── Stacks on top (via stacking headers)
│  └─────────────────────────────────┘    │
│                                         │
│  STEMMA QT ──┬── LIS3DH (INT1 → GPIO)   │◄── 1 cable + optional INT wire
│              └── NAU7802 ◄── Load Cell  │◄── 1 cable + 4 wires to load cell
│                                         │
│  JST Battery ◄── 1200mAh LiPo           │◄── Plug in (built-in charging via USB-C)
└─────────────────────────────────────────┘

Load Cell Wiring (to NAU7802 terminal block):
  Red    → E+
  Black  → E-
  White  → A-
  Green  → A+
```

### I2C Addresses
| Device | Address |
|--------|---------|
| NAU7802 | 0x2A |
| LIS3DH | 0x18 (default) or 0x19 |

---

## Component Specifications

### ESP32 Feather V2
- Dual-core ESP32 @ 240MHz
- 8MB Flash, 2MB PSRAM
- WiFi 802.11 b/g/n + Bluetooth Classic/LE
- Built-in LiPo charging via USB-C
- STEMMA QT / Qwiic I2C connector
- Deep sleep: ~5μA

### NAU7802 24-Bit ADC
- 24-bit resolution
- Up to 320 samples/second
- On-chip PGA (1x to 128x gain)
- I2C interface
- Optimized for load cells / strain gauges

### LIS3DH Accelerometer
- ±2g/±4g/±8g/±16g selectable range
- 10-bit resolution
- I2C or SPI interface
- Interrupt pins for wake-on-motion
- Ultra-low power: 2μA in low-power mode

### 2.13" E-Paper Display
- 250×122 pixels, monochrome
- SSD1680 driver
- Partial refresh: ~0.3s
- Full refresh: ~2s
- Near-zero power when static

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

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2026-01-07 | 1.0 | Initial BOM - UK split order (Pimoroni + The Pi Hut) |
