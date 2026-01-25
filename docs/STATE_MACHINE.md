# Aquavate Bottle State Machine

This document describes the operational states of the Aquavate smart water bottle firmware.

---

## Overview

The bottle operates in one of three primary modes:
1. **Normal Operation** - Monitoring water level and tracking consumption
2. **Calibration Mode** - Setting up the load cell for accurate measurements
3. **Deep Sleep** - Low power mode between uses (currently disabled for development)

---

## Primary Operating Modes

### Normal Operation

**What the bottle does:**
- Monitors water level in real-time
- Detects and records drink events (≥30ml removed)
- Tracks daily water intake with 4am daily reset
- Updates e-paper display when water level changes ≥5ml
- Responds to serial commands (time setting)

**How to enter:**
- Default state after power-on or wake from sleep
- Returns here after calibration completes

**How to exit:**
- Hold bottle inverted for 5 seconds → Calibration Mode
- Awake timeout (currently disabled) → Deep Sleep

---

### Calibration Mode

**What the bottle does:**
- Guides user through two-point calibration process
- Measures empty bottle weight
- Measures full bottle (830ml) weight
- Calculates and stores scale factor for accurate measurements

**How to enter:**
- From Normal Operation: Hold bottle inverted (upside down) for 5 seconds

**How to exit:**
- Calibration completes successfully → Normal Operation
- Calibration error occurs → Normal Operation (old calibration retained if any)

**Calibration Steps:**

```
1. TRIGGERED
   ↓
2. Place empty bottle upright on stable surface
   ↓
3. Measuring empty weight...
   ↓
4. Fill bottle to 830ml line
   ↓
5. Place full bottle upright, keep stable for 5 seconds
   ↓
6. Measuring full weight...
   ↓
7. Calculating scale factor...
   ↓
8. COMPLETE (saves to memory)
```

**Requirements:**
- Bottle must be stable (not moving) during measurements
- Full bottle must be exactly 830ml
- Weight delta must be ≥300,000 ADC units (~830g)

---

### Deep Sleep (Currently Disabled)

**What the bottle does:**
- ESP32 enters deep sleep mode
- All peripherals powered down except ADXL343 accelerometer
- Waits for tilt interrupt

**How to enter:**
- After timeout with no activity (feature disabled for development)

**How to exit:**
- ADXL343 detects tilt (Z-axis < threshold) → Wake → Normal Operation

---

## Bottle Orientations (Gestures)

The firmware recognizes four bottle orientations using the ADXL343 accelerometer:

### INVERTED_HOLD
- **Detection:** Z-axis < -0.8g held for 5 seconds
- **Meaning:** Bottle held upside down
- **Triggers:** Calibration mode entry

### UPRIGHT
- **Detection:** Z ≥ 0.97g, minimal tilt, weight ≥ -50ml, low motion
- **Meaning:** Bottle sitting on table
- **Triggers:** Display updates when water level changes

### UPRIGHT_STABLE
- **Detection:** UPRIGHT + weight variance < 6ml for 2+ seconds
- **Meaning:** Bottle resting on table with stable weight
- **Triggers:** Drink detection and recording

### SIDEWAYS_TILT
- **Detection:** |X| or |Y| > 0.5g
- **Meaning:** Bottle tilted sideways
- **Usage:** Currently unused (reserved for future confirmation gestures)

---

## Drink Tracking States

Within Normal Operation, the drink tracking system operates in several states:

### Baseline Mode
- **When:** First run or after refill
- **Action:** Establishing reference weight for drink detection

### Monitoring Mode
- **When:** Normal operation with established baseline
- **Action:** Comparing current weight to baseline every 500ms when UPRIGHT_STABLE

### Aggregation Window (5 minutes)
- **When:** Within 5 minutes of last drink
- **Action:** Multiple drinks combined into single record
- **Purpose:** Handles sipping behavior (e.g., 3 × 50ml sips = 150ml drink)

### Daily Reset
- **When:** Time crosses 4am boundary, or 20+ hours since last reset
- **Action:** Daily total and drink count reset to zero
- **Purpose:** New day starts at 4am (post-midnight drinking counts toward previous day)

---

## Display Update Logic

The e-paper display refreshes in these scenarios:

### Main Screen Updates (Normal Operation)
- **Trigger:** Water level changed ≥5ml while bottle UPRIGHT
- **Rate Limit:** Checked every 500ms maximum
- **Shows:**
  - Current bottle level (0-830ml) with bottle graphic
  - Daily intake total
  - Time and day (if RTC set)
  - Battery percentage
  - Human figure or glass grid (fills as daily goal progress)

### Calibration Screen Updates
- **Trigger:** State changes during calibration
- **Shows:** Instructions for current calibration step

### Welcome Screen
- **Trigger:** First boot only
- **Shows:** "Aquavate" branding with water drop icon

---

## State Persistence (NVS Storage)

The following state is saved to non-volatile storage and survives power cycles:

### Calibration Data
- Scale factor (ADC counts per gram)
- Empty bottle ADC reading
- Full bottle ADC reading
- Calibration timestamp

### Time Configuration
- Timezone offset (hours from UTC)
- Time valid flag (whether RTC has been set)

### Daily Tracking State
- Daily total (ml)
- Drink count today
- Last drink timestamp
- Last recorded weight baseline
- Aggregation window state
- Last reset timestamp

### Drink History
- Circular buffer of last 100 drink records
- Each record: timestamp, amount (ml), bottle level, flags

---

## Error Handling

### Calibration Errors
- **Empty measurement failed** → Error screen → Return to Normal Operation
- **Full measurement failed** → Error screen → Return to Normal Operation
- **Invalid scale factor** → Error screen → Return to Normal Operation
- **Storage save failed** → Error screen → Return to Normal Operation

All errors display for 5 seconds then return to Normal Operation with previous calibration intact (if any).

### Time Not Set Warning
- Drink tracking disabled until time is set via USB serial command
- Display shows "--- --" instead of time
- Serial prompt: `"Use SET_TIME command to set time"`

---

## Complete State Transition Diagram

```
                                    POWER ON / RESET
                                           │
                                           ↓
                                    ┌─────────────┐
                                    │   STARTUP   │
                                    │ Initialize  │
                                    │  Hardware   │
                                    └──────┬──────┘
                                           │
                                           ↓
    ╔══════════════════════════════════════════════════════════════════════════╗
    ║                          NORMAL OPERATION MODE                           ║
    ╠══════════════════════════════════════════════════════════════════════════╣
    ║                                                                          ║
    ║  ┌─────────────────────────────────────────────────────────────────┐    ║
    ║  │ DRINK TRACKING SUBSYSTEM                                        │    ║
    ║  │                                                                 │    ║
    ║  │  ┌──────────────┐                                               │    ║
    ║  │  │   BASELINE   │  First run / after refill                     │    ║
    ║  │  │     MODE     │  Establishing reference weight                │    ║
    ║  │  └──────┬───────┘                                               │    ║
    ║  │         │                                                        │    ║
    ║  │         ↓                                                        │    ║
    ║  │  ┌──────────────┐     Drink ≥30ml      ┌──────────────────┐    │    ║
    ║  │  │  MONITORING  │────────────────────→ │   AGGREGATION    │    │    ║
    ║  │  │     MODE     │                      │  WINDOW (5 min)  │    │    ║
    ║  │  └──────┬───────┘ ←────────────────────└────────┬─────────┘    │    ║
    ║  │         │           Window expires               │              │    ║
    ║  │         │                                        │              │    ║
    ║  │         └────────────────┬───────────────────────┘              │    ║
    ║  │                          │                                      │    ║
    ║  │                          │ 4am boundary or 20+ hours            │    ║
    ║  │                          ↓                                      │    ║
    ║  │                    ┌─────────────┐                              │    ║
    ║  │                    │ DAILY RESET │ Reset counters to 0          │    ║
    ║  │                    └─────────────┘                              │    ║
    ║  └─────────────────────────────────────────────────────────────────┘    ║
    ║                                                                          ║
    ║  ┌─────────────────────────────────────────────────────────────────┐    ║
    ║  │ DISPLAY UPDATE LOGIC                                            │    ║
    ║  │                                                                 │    ║
    ║  │  Check every 500ms when UPRIGHT:                                │    ║
    ║  │    If water level changed ≥5ml → Refresh e-paper display        │    ║
    ║  │    If daily total changed ≥50ml → Refresh e-paper display       │    ║
    ║  └─────────────────────────────────────────────────────────────────┘    ║
    ║                                                                          ║
    ╚══════════════════════════════════════════════════════════════════════════╝
                                           │
                                           │
                           Hold bottle INVERTED for 5 seconds
                                           │
                                           ↓
    ╔══════════════════════════════════════════════════════════════════════════╗
    ║                          CALIBRATION MODE                                ║
    ╠══════════════════════════════════════════════════════════════════════════╣
    ║                                                                          ║
    ║   ┌──────────────┐                                                       ║
    ║   │  TRIGGERED   │  Entry point                                          ║
    ║   └──────┬───────┘                                                       ║
    ║          │                                                               ║
    ║          ↓                                                               ║
    ║   ┌──────────────┐                                                       ║
    ║   │  WAIT_EMPTY  │  Display: "Place empty bottle"                        ║
    ║   └──────┬───────┘                                                       ║
    ║          │                                                               ║
    ║          │ Bottle UPRIGHT_STABLE                                         ║
    ║          ↓                                                               ║
    ║   ┌───────────────┐                                                      ║
    ║   │ MEASURE_EMPTY │  Taking stable ADC reading                           ║
    ║   └──────┬────────┘                                                      ║
    ║          │                                                               ║
    ║          │ Success                                                       ║
    ║          ↓                                                               ║
    ║   ┌──────────────┐                                                       ║
    ║   │  WAIT_FULL   │  Display: "Fill to 830ml"                             ║
    ║   └──────┬───────┘  Wait for weight ≥300k ADC & stable 5s                ║
    ║          │                                                               ║
    ║          │ Bottle UPRIGHT_STABLE + Heavy + Stable 5s                     ║
    ║          ↓                                                               ║
    ║   ┌──────────────┐                                                       ║
    ║   │ MEASURE_FULL │  Taking stable ADC reading                            ║
    ║   └──────┬───────┘  Calculate scale factor                               ║
    ║          │          Save to NVS                                          ║
    ║          │                                                               ║
    ║          │ Success                      Measurement error                ║
    ║          ↓                                      │                        ║
    ║   ┌──────────────┐                             ↓                         ║
    ║   │   COMPLETE   │                      ┌─────────────┐                  ║
    ║   │ Display: OK! │                      │    ERROR    │                  ║
    ║   └──────┬───────┘                      │ Display msg │                  ║
    ║          │                              └──────┬──────┘                  ║
    ║          │                                     │                         ║
    ║          │ (5 second delay)                    │ (5 second delay)        ║
    ║          └─────────────┬─────────────────────┘                          ║
    ║                        │                                                 ║
    ╚════════════════════════┼═════════════════════════════════════════════════╝
                             │
                             │ Return to Normal Operation
                             ↓
                      ┌──────────────┐
                      │    NORMAL    │
                      │  OPERATION   │
                      └──────┬───────┘
                             │
                             │ Awake timeout (CURRENTLY DISABLED)
                             ↓
                      ┌──────────────┐
                      │  DEEP SLEEP  │
                      │ Wait for tilt│
                      └──────┬───────┘
                             │
                             │ ADXL343 tilt interrupt
                             ↓
                      ┌──────────────┐
                      │  WAKE / BOOT │
                      └──────────────┘
                             │
                             └──────→ (Return to NORMAL OPERATION)


GESTURE STATES (detected throughout Normal Operation and Calibration):
═══════════════════════════════════════════════════════════════════

    INVERTED_HOLD       Z < -0.8g for 5s          → Trigger Calibration
    UPRIGHT             Z ≥ 0.97g, stable         → Enable display updates
    UPRIGHT_STABLE      UPRIGHT + weight stable   → Enable drink detection
    SIDEWAYS_TILT       |X| or |Y| > 0.5g         → (Reserved for future)
```

---

## Key Thresholds & Timings

| Parameter | Value | Purpose |
|-----------|-------|---------|
| Inverted hold duration | 5 seconds | Calibration trigger |
| Upright stable duration | 2 seconds | Drink detection readiness |
| Minimum drink | 30ml | Noise filtering |
| Minimum refill | 100ml | Distinguish from drinks |
| Aggregation window | 5 minutes | Combine sips |
| Daily reset hour | 4am | New day boundary |
| Display update threshold | 5ml | Minimize e-paper flashing |
| Display check interval | 500ms | Balance responsiveness/power |
| Accelerometer variance threshold | 0.01g² | Stability detection |

---

## Serial Commands (USB)

Available in all states except Deep Sleep:

- `SET_TIME <YYYY-MM-DD> <HH:MM:SS> <offset>` - Set RTC time and timezone
- `GET_TIME` - Show current time
- `RESET_DAILY` - Reset daily drink counter (testing only)
- `CLEAR_DRINKS` - Clear all drink records (testing only)

---

## Files Reference

- Main loop: [firmware/src/main.cpp](../firmware/src/main.cpp)
- Calibration: [firmware/src/calibration.cpp](../firmware/src/calibration.cpp)
- Gestures: [firmware/src/gestures.cpp](../firmware/src/gestures.cpp)
- Drinks: [firmware/src/drinks.cpp](../firmware/src/drinks.cpp)
- Configuration: [firmware/src/config.h](../firmware/src/config.h)
