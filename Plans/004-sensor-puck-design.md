# Aquavate Sensor Puck - Mechanical Design Document

**Version:** 3.0
**Created:** 2026-01-08
**Updated:** 2026-01-11

---

## 1. Design Summary

| Parameter | Value |
|-----------|-------|
| **Puck diameter** | 85mm (flush with bottle) |
| **Puck height** | 30mm (main body) |
| **Bottle diameter** | 85mm (transparent) |
| **Attachment** | Friction-fit silicone sleeve |
| **Display** | 2.13" e-paper, landscape, side-mounted |
| **Side bump** | 65mm wide x 44mm tall x 26mm deep |
| **Configuration** | Option A - electronics in base, cantilever load cell |

---

## 2. Overview & Concept

The sensor puck is a 3D-printed enclosure that attaches to the bottom of a transparent water bottle. It measures weight changes to track water consumption across refills.

### Exploded View

```
                    ┌─────────────────┐
                    │  Water Bottle   │
                    │ (85mm, transparent)
                    └────────┬────────┘
                             │
              ┌──────────────▼──────────────┐
              │      Silicone Sleeve        │  ← Friction grip (82mm ID)
              └──────────────┬──────────────┘
                             │
    ┌────────────────────────▼────────────────────────┐
    │                  TOP PLATE                       │  ← 80mm dia, 3mm thick
    │            (bottle contact surface)              │     Attached to FREE end
    └────────────────────────┬────────────────────────┘
                             │
                      ┌──────┴──────┐
                      │  LOAD CELL  │  ← Cantilever mounted
                      │  (bar type) │     ~80 x 12.7 x 12.7mm
                      └──────┬──────┘
                             │
    ┌────────────────────────▼────────────────────────┐
    │           BASE SHELL                           │
    │  ┌───────┐  ┌───────────────┐                  │
    │  │NAU7802│  │   Battery     │                  │
    │  │LIS3DH │  │   1200mAh     │                  │
    │  │DS3231 │  │               │                  │
    │  └───────┘  └───────────────┘                  │
    └──────────────────────┬─────────────────────────┘
           85mm main body  │
                          ┌┴───────────────────────────────┐
                          │  E-Paper + ESP32 Feather       │ ← Landscape
                          │  ┌─────────────────────────┐   │   display in
                          │  │       Display           │   │   side bump
                          │  └─────────────────────────┘   │
                          └────────────────────────────────┘
                                    65mm wide
```

### Side Profile

```
         Bottle (85mm)
              │
    ┌─────────▼─────────┐
    │                   │
    │   ════════════    │ ← Load cell (cantilever)
    │                   │
    │   [NAU][LIS][RTC] │
    │   [Battery]       │
    │                   │
    └─────────┬─────────┘
              │
           85mm
              │
    ┌─────────┴───────────────────────────────────────────┐
    │                                                     │
    │  ┌───────────────────────────────────────────────┐  │  44mm
    │  │  Display (landscape 48mm x 24mm active)       │  │  tall
    │  └───────────────────────────────────────────────┘  │
    │           ESP32 Feather behind display              │
    └─────────────────────────────────────────────────────┘
                    Side bump (65mm wide x 26mm deep)
```

---

## 3. Display Design

### Landscape E-Paper (2.13" FeatherWing)

The Feather + FeatherWing stack mounts horizontally in a side bump, giving landscape orientation.

**Display specs:**
- Resolution: 250 x 122 pixels (landscape)
- Physical active area: 48.55mm x 23.7mm
- Refresh: ~2s full, ~0.3s partial

**Stacking configuration:**
- FeatherWing mounts on **bottom** of ESP32 Feather via standard headers
- Display faces outward (away from Feather)
- Combined stack depth: ~22-24mm (Feather 7.2mm + headers 8.5mm + FeatherWing 6.7mm)

### Display Content

Shows information you **can't** see by looking at the transparent bottle:

```
┌─────────────────────────────────────────────────┐
│                                                 │
│   Daily Total: 1,850 ml          74%  🔋 83%   │
│                                                 │
│   ████████████████████░░░░░░░░                 │
│                                                 │
└─────────────────────────────────────────────────┘
        48mm wide x 24mm tall (active area)
```

**Not shown:** Current bottle level (visible through transparent bottle)

---

## 4. Mechanical Dimensions

### Main Body (85mm diameter)

```
Top View - Base Shell:

            USB-C cutout
                 ↓
        ╭───────────────────────╮
       ╱    ═══════════════      ╲
      │     LOAD CELL (80mm)      │
      │                           │
      │  ┌─────┐    ┌──────────┐  │
      │  │ NAU │    │          │  │
      │  │ LIS │    │ Battery  │  │
      │  │ RTC │    │ 1200mAh  │  │
      │  └─────┘    └──────────┘  │
       ╲                         ╱
        ╰──────────────┬────────╯
                       │
                  Side bump
                  (ESP32 + EPD)
```

### Dimensions Table

| Component | Dimension | Notes |
|-----------|-----------|-------|
| Main body OD | 85mm | Flush with bottle |
| Main body ID | 80mm | 2.5mm wall thickness |
| Main body height | 30mm | Floor to top |
| Floor thickness | 3mm | Structural |
| Top plate diameter | 80mm | Clears walls by 2.5mm |
| Top plate thickness | 3mm | Rigid |
| Gap (plate to base) | 2.5mm | Load cell deflection |
| Side bump width | 65mm | Houses Feather stack (landscape) |
| Side bump height | 44mm | FeatherWing 40.2mm + clearance |
| Side bump depth | 26mm | Stack 22-24mm + wall |
| Display window | 50mm x 25mm | Active area + margin |

### Side Bump Detail (Landscape)

```
Front view of bump (looking at display):

┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                                                     │   │
│   │              Display Active Area                    │   │  44mm
│   │              48.55mm x 23.7mm                       │   │  tall
│   │                                                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
                          65mm wide


Side view of bump (cross-section):

    ┌─────────────────┐
    │  ESP32 Feather  │  ← Inner (7.2mm)
    │                 │
    └────────┬────────┘
             │ Headers (~8.5mm)
    ┌────────┴────────┐
    │  E-Paper Wing   │  ← Outer (6.7mm)
    │  [Display]      │     Display faces outward
    └─────────────────┘
        ~22-24mm stack depth
        ~26mm total with wall
```

---

## 5. Load Cell Mounting

### Cantilever Configuration

```
Side View - Load Cell Detail:

                              ┌── Top Plate (80mm)
                              │   ┌───────┐
                              │   │       │
    ══════════════════════════╪═══╪═══════╪
    │  ▓▓▓▓▓▓▓  │             │   │       │
    │  STRAIN   │             │   └───────┘
    │  GAUGE    │             │
    │  ▓▓▓▓▓▓▓  │             │
    ══════════════════════════╪═══════════
    ▲           ▲             ▲
    │           │             │
    M4 bolt     M4 bolt    M4 bolts (x2)
    (fixed)     (fixed)    (to top plate)

    ├── FIXED END ──┤├── FREE END ──┤
         ~25mm           ~55mm
```

### Load Cell Specifications

| Parameter | Value |
|-----------|-------|
| Type | Bar strain gauge, 5kg capacity |
| Dimensions | ~80 x 12.7 x 12.7mm |
| Mounting holes | M4 or M5, 2 per end |
| Output | ~1mV/V |
| Wires | Red (E+), Black (E-), White (A-), Green (A+) |

### Mounting Rules

1. **Fixed end**: Both holes bolted firmly to base
2. **Free end**: Both holes bolted to top plate only
3. **Strain gauge**: Must be in floating section (not clamped)
4. **Clearance**: 2mm minimum around load cell body
5. **No pre-stress**: Mount flat, no bending

---

## 6. Component Layout

### Internal Arrangement

```
Base Shell Interior (80mm ID):

    ┌─────────────────────────────────────────────┐
    │                                             │
    │    ═══════════════════════════════════      │
    │         LOAD CELL (80mm diagonal)           │
    │                                             │
    │    ┌───────────┐       ┌───────────────┐    │
    │    │  NAU7802  │       │               │    │
    │    │  (25x18)  │       │    BATTERY    │    │
    │    ├───────────┤       │    1200mAh    │    │
    │    │  LIS3DH   │       │   (50x34x6)   │    │
    │    │  (18x18)  │       │               │    │
    │    ├───────────┤       └───────────────┘    │
    │    │  DS3231   │                            │──► To side bump
    │    │  (25x25)  │                            │    (ESP32+EPD)
    │    └───────────┘                            │
    │                                             │
    └─────────────────────────────────────────────┘
```

### Side Bump (ESP32 + Display) - Landscape

```
Side bump interior (front view - looking at display):

┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                                                     │   │
│   │              E-Paper Display                        │   │
│   │              (50 x 25mm window)                     │   │
│   │                                                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
│                    ◯ USB-C (top edge)                       │
└─────────────────────────────────────────────────────────────┘
                          65mm wide

Side bump interior (top view - looking down):

    ┌─────────────────────────────────────────────────────────┐
    │                                                         │
    │   ESP32 Feather (52.3 x 22.8mm)                        │
    │   ┌─────────────────────────────────────────────────┐   │
    │   │ [USB-C]                            [STEMMA QT]  │   │
    │   └─────────────────────────────────────────────────┘   │
    │                                                         │
    │   E-Paper FeatherWing below (61.3 x 40.2mm)            │
    │                                                         │
    └─────────────────────────────────────────────────────────┘
              26mm deep (stack + wall)
```

### Cable Routing

```
STEMMA QT Daisy Chain:

Side bump                    Main body
┌────────┐                  ┌─────────────┐
│ ESP32  │──[100mm cable]──►│ NAU7802     │
└────────┘                  │     │       │
                            │     ▼       │
                            │ LIS3DH      │
                            │     │       │
                            │     ▼       │
                            │ DS3231      │
                            └─────────────┘

Load cell wires (~50mm) from NAU7802 to load cell terminals.
Battery JST cable routes from main body to side bump ESP32.
```

---

## 7. 3D Printing Specifications

### Parts List

| Part | Material | Supports | Notes |
|------|----------|----------|-------|
| Base shell + bump | PETG | Yes (internal) | Print as one piece |
| Top plate | PETG | No | Print flat |
| Display bezel (optional) | PETG | No | Snap-fit frame |

### Print Settings

| Parameter | Base Shell | Top Plate |
|-----------|------------|-----------|
| Layer height | 0.2mm | 0.2mm |
| Perimeters | 3 | 4 |
| Top/bottom layers | 5 | 6 |
| Infill | 40% gyroid | 60% gyroid |
| Supports | Yes | No |
| Orientation | Upright | Flat |

### Material Choice

| Material | Pros | Cons |
|----------|------|------|
| **PETG** (recommended) | Strong, slight flex, easy to print | Strings slightly |
| ABS | Very strong | Warps, needs enclosure |
| PLA | Easy to print | Creeps under load, heat sensitive |

---

## 8. Bill of Materials - Hardware

### Fasteners

| Item | Qty | Size | Purpose |
|------|-----|------|---------|
| Heat-set inserts | 4 | M4 x 6mm | Load cell mounting |
| Heat-set inserts | 6 | M2.5 x 4mm | PCB standoffs |
| Cap head screws | 4 | M4 x 10mm | Load cell |
| Cap head screws | 6 | M2.5 x 6mm | PCBs |
| Washers | 4 | M4 | Load distribution |

### Other Hardware

| Item | Qty | Notes |
|------|-----|-------|
| Silicone sleeve | 1 | 82-83mm ID, stretches to 85mm |
| Rubber feet | 4 | 8-10mm adhesive |
| Foam tape | 1 | Battery mounting |

### Estimated Cost

| Category | Cost |
|----------|------|
| Fasteners & inserts | ~£5 |
| Silicone sleeve | ~£5-8 |
| Rubber feet & misc | ~£3 |
| **Total hardware** | **~£13-16** |

---

## 9. Assembly Instructions

### Tools Required

- Soldering iron (for heat-set inserts)
- Hex drivers (2mm, 2.5mm)
- Wire strippers

### Step 1: Prepare Shell

```
1.1 Install M4 heat-set inserts in load cell mounts (x4)
1.2 Install M2.5 heat-set inserts for PCB standoffs (x6)
1.3 Test fit all screws
```

### Step 2: Mount Load Cell

```
2.1 Place load cell in base shell
2.2 Bolt FIXED END to base (2x M4 screws + washers)
2.3 Verify FREE END floats with clearance
```

### Step 3: Install Main Electronics

```
3.1 Mount NAU7802 on standoffs
3.2 Connect load cell wires:
    Red → E+, Black → E-, White → A-, Green → A+
3.3 Mount LIS3DH and DS3231
3.4 Connect STEMMA QT chain: NAU → LIS → RTC
3.5 Mount battery with foam tape
```

### Step 4: Install Side Bump Electronics

```
4.1 Mount ESP32 Feather in side bump (USB-C accessible from top)
4.2 Stack FeatherWing on bottom via headers (landscape orientation)
4.3 Display faces outward through window
4.4 Route STEMMA QT cable from ESP32 to NAU7802
4.5 Connect battery JST to ESP32
```

### Step 5: Attach Top Plate

```
5.1 Align top plate over load cell FREE END
5.2 Bolt with 2x M4 screws + washers
5.3 Verify plate moves freely (no rubbing)
```

### Step 6: Final Assembly

```
6.1 Stretch silicone sleeve over top plate
6.2 Apply rubber feet to base
6.3 Power on, run calibration
```

---

## 10. Calibration

### Procedure

1. **Tare**: Empty top plate, set zero
2. **Span**: Apply 500g known weight, calibrate
3. **Verify**: Test 250g, 750g, 1000g

### Accuracy Targets

| Parameter | Target |
|-----------|--------|
| Accuracy | ±15ml |
| Repeatability | ±5g |
| Resolution | ~0.1g |

---

## 11. Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2026-01-08 | 1.0 | Initial design (100mm, landscape display) |
| 2026-01-08 | 2.0 | Revised: 85mm diameter, portrait e-paper in side bump, simplified display content |
| 2026-01-11 | 3.0 | Changed to landscape display (65x44x26mm side bump), FeatherWing mounts under Feather |
