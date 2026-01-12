# Component Dimensions Reference

**Version:** 1.0
**Created:** 2026-01-11
**Source:** Adafruit product pages, manufacturer datasheets

---

## 1. ESP32 Feather V2

**Product:** [Adafruit ESP32 Feather V2](https://www.adafruit.com/product/5400)

| Parameter | Value |
|-----------|-------|
| Length | 52.3 mm |
| Width | 22.8 mm |
| Height | 7.2 mm |
| Weight | 6.0 g |

### Key Features
- USB-C connector on short edge
- JST-PH 2-pin battery connector
- STEMMA QT connector
- Feather-standard header spacing: 2.0" (50.8mm) between rows

### Mounting
- Standard Feather mounting holes (4x)
- Hole spacing: ~45.7mm x 17.8mm (Feather standard)
- Hole diameter: 2.5mm (for M2.5 screws)

---

## 2. 2.13" Mono E-Paper FeatherWing

**Product:** [Adafruit 2.13" Mono E-Paper FeatherWing](https://www.adafruit.com/product/4195)

| Parameter | Value |
|-----------|-------|
| Board Length | 61.3 mm |
| Board Width | 40.2 mm |
| Board Height | 6.7 mm |
| Weight | 13.8 g |

### Display Panel Specifications
| Parameter | Value |
|-----------|-------|
| Display Size | 2.13 inch diagonal |
| Resolution | 250 x 122 pixels |
| Active Area | 48.55 x 23.7 mm |
| Pixel Pitch | 0.194 mm |
| DPI | 130 |

### Stacking Configuration
- Mounts on **bottom** of ESP32 Feather via standard headers
- Display faces outward (away from Feather)
- **Landscape orientation** in side bump
- Standard headers: ~8.5mm spacing between boards
- Combined stack height: ~22-24mm (Feather 7.2mm + headers 8.5mm + FeatherWing 6.7mm)

```
Front View (looking at side bump - LANDSCAPE):

    ┌──────────────────────────────────────────────────────────┐
    │                    E-Paper FeatherWing                   │
    │                       (61.3mm wide)                      │
    │    ┌────────────────────────────────────────────────┐    │
    │    │                                                │    │
    │    │           Display Active Area                  │    │
    │    │            48.55mm x 23.7mm                    │    │
    │    │              (250 x 122 px)                    │    │
    │    │                                                │    │
    │    └────────────────────────────────────────────────┘    │
    │                                                          │
    └──────────────────────────────────────────────────────────┘
                              40.2mm tall

Side View (cross-section through side bump):

    ┌─────────────────┐
    │  ESP32 Feather  │  ← Inner (USB-C accessible from top)
    │    (7.2mm)      │
    └────────┬────────┘
             │ Headers (~8.5mm)
    ┌────────┴────────┐
    │  E-Paper Wing   │  ← Outer
    │    (6.7mm)      │
    │  [Display]      │  ← Display faces outward
    └─────────────────┘
        ~22-24mm total depth
```

### Side Bump Dimensions (Landscape)
| Parameter | Value | Notes |
|-----------|-------|-------|
| Width | ~65mm | FeatherWing 61.3mm + 2mm clearance each side |
| Height | ~44mm | FeatherWing 40.2mm + 2mm clearance each side |
| Depth | ~26mm | Stack 22-24mm + 2mm wall |
| Display Window | 50mm x 25mm | Active area 48.55 x 23.7mm + margin |

---

## 3. NAU7802 24-Bit ADC STEMMA QT

**Product:** [Adafruit NAU7802 STEMMA QT](https://www.adafruit.com/product/4538)

| Parameter | Value |
|-----------|-------|
| Length | 25.3 mm |
| Width | 23.0 mm |
| Height | 4.5 mm |
| Weight | 2.3 g |

### Connectors
- 2x STEMMA QT/Qwiic connectors (I2C daisy-chain)
- 4-pin terminal block for load cell wires
- I2C Address: 0x2A (fixed)

### Mounting
- 2x mounting holes
- Hole spacing: ~19mm
- Hole diameter: ~2.5mm

---

## 4. LIS3DH Triple-Axis Accelerometer STEMMA QT

**Product:** [Adafruit LIS3DH STEMMA QT](https://www.adafruit.com/product/2809)

| Parameter | Value |
|-----------|-------|
| Length | 20.6 mm |
| Width | 20.3 mm |
| Height | 2.6 mm |
| Weight | 1.5 g |

### Connectors
- 2x STEMMA QT/Qwiic connectors (I2C daisy-chain)
- I2C Address: 0x18 (default), 0x19 (alt)

### Mounting
- 4x mounting holes
- Square layout: ~15mm x 15mm
- Hole diameter: ~2.5mm

---

## 5. 5kg Bar Load Cell

**Product:** [SparkFun TAL220B](https://www.sparkfun.com/load-cell-5kg-straight-bar-tal220b.html) / [Adafruit 4541](https://www.adafruit.com/product/4541)

### SparkFun TAL220B (Compact)
| Parameter | Value |
|-----------|-------|
| Length | 55 mm |
| Width | 12.7 mm |
| Height | 12.7 mm |
| Wire Length | 200 mm |
| Capacity | 5 kg |

### Adafruit 4541 (Longer)
| Parameter | Value |
|-----------|-------|
| Length | 75 mm |
| Width | 12.7 mm |
| Height | 12.7 mm |
| Wire Length | ~200 mm |
| Capacity | 5 kg |

### Mounting Holes (Both Models)
| End | Hole Type | Spacing |
|-----|-----------|---------|
| Fixed End | 2x M4 or M5 | ~15mm center-to-center |
| Free End | 2x M4 or M5 | ~15mm center-to-center |

### Wiring Color Code
| Wire Color | Connection |
|------------|------------|
| Red | E+ (Excitation+) |
| Black | E- (Excitation-) |
| White | A- (Signal-) |
| Green | A+ (Signal+) |

### Important Notes
- Mount FIXED end rigidly to base (both holes)
- Mount FREE end to weighing platform only
- Strain gauge section must float (not clamped)
- Arrow on side indicates force direction

---

## 6. 1200mAh LiPo Battery

**Product:** [Adafruit 258](https://www.adafruit.com/product/258)

| Parameter | Value |
|-----------|-------|
| Length | 62 mm |
| Width | 34 mm |
| Thickness | 5 mm |
| Capacity | 1200 mAh |
| Voltage | 3.7V nominal |
| Connector | JST-PH 2-pin |

### Notes
- Built-in protection circuit
- Compatible with Feather charging circuit
- Mount with foam tape (avoid near load cell)

---

## 7. STEMMA QT Cables

**Standard Lengths Available:**
- 50mm
- 100mm (recommended for this design)
- 200mm

**Connector:** JST SH 4-pin (1mm pitch)

**Cable Bend Radius:** Minimum 5mm recommended

---

## 8. Clearance Requirements

| Clearance Type | Minimum |
|----------------|---------|
| STEMMA QT cable routing | 4 mm |
| Load cell deflection gap | 2.5 mm |
| PCB to wall | 2 mm |
| USB-C port access | 10 mm depth |
| Battery expansion margin | 1 mm |

---

## 9. Summary Table

| Component | L x W x H (mm) | Notes |
|-----------|----------------|-------|
| ESP32 Feather V2 | 52.3 x 22.8 x 7.2 | MCU |
| E-Paper FeatherWing | 61.3 x 40.2 x 6.7 | Stacks under Feather |
| Feather Stack | 61.3 x 40.2 x 22-24 | Combined with headers |
| NAU7802 | 25.3 x 23.0 x 4.5 | Load cell ADC |
| LIS3DH | 20.6 x 20.3 x 2.6 | Accelerometer |
| Load Cell (short) | 55 x 12.7 x 12.7 | TAL220B |
| Load Cell (long) | 75 x 12.7 x 12.7 | Adafruit 4541 |
| Battery | 62 x 34 x 5 | 1200mAh LiPo |
| Display Active Area | 48.55 x 23.7 | Landscape orientation |
| Side Bump (ext.) | 65 x 44 x 26 | Houses Feather stack |

---

## 10. References

- [Adafruit ESP32 Feather V2](https://www.adafruit.com/product/5400)
- [Adafruit 2.13" E-Paper FeatherWing](https://www.adafruit.com/product/4195)
- [Adafruit NAU7802 STEMMA QT](https://www.adafruit.com/product/4538)
- [Adafruit LIS3DH STEMMA QT](https://www.adafruit.com/product/2809)
- [Adafruit 1200mAh LiPo Battery](https://www.adafruit.com/product/258)
- [SparkFun TAL220B Load Cell](https://www.sparkfun.com/load-cell-5kg-straight-bar-tal220b.html)
- [Adafruit 5kg Load Cell](https://www.adafruit.com/product/4541)
