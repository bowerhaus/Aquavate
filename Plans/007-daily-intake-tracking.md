# Phase 2 Implementation Plan: Daily Water Intake Tracking

**Target:** Aquavate firmware - Track daily water consumption with human figure visualization

---

## Overview

Implement daily water intake tracking for the Aquavate smart water bottle. The system will:
- Detect drink events (≥30ml decrease) with 5-minute aggregation windows
- Reset daily totals on first drink after 4:00am local time
- Store 7 days of drink history in NVS circular buffer
- Display daily intake with human figure visualization (fills bottom-to-top)
- Hardcode 2500ml daily goal for MVP

**User Choices:**
- ✓ Human figure visualization (fills up as you drink)
- ✓ Aggregate drinks within 5-minute windows
- ✓ Daily reset: first drink after 4am
- ✓ Hardcoded 2500ml goal for MVP

**Implementation Strategy:**
- **Day 1:** Test bitmap rendering FIRST to validate approach
- If bitmap works: proceed with drink tracking (Days 2-4)
- If bitmap fails: pivot to alternative visualization (progress bar or primitives)

---

## Architecture

### New Files to Create

1. **[firmware/include/drinks.h](firmware/include/drinks.h)**
   - DrinkRecord structure (9 bytes: timestamp, amount_ml, bottle_level_ml, flags)
   - DailyState structure (26 bytes: tracks daily total, last drink, reset time, aggregation window)
   - Public API: `drinksInit()`, `drinksUpdate()`, `getCurrentUnixTime()`

2. **[firmware/src/drinks.cpp](firmware/src/drinks.cpp)**
   - Core drink detection algorithm
   - 5-minute aggregation window logic
   - Daily reset detection (`shouldResetDailyCounter()`)
   - Integration with weight/time/gesture systems

3. **[firmware/include/storage_drinks.h](firmware/include/storage_drinks.h)**
   - CircularBufferMetadata structure (10 bytes: write_index, record_count, total_writes)
   - Storage API declarations

4. **[firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp)**
   - NVS circular buffer management (200 record capacity)
   - Functions: `storageSaveDrinkRecord()`, `storageLoadLastDrinkRecord()`, `storageUpdateLastDrinkRecord()`, `storageLoadDailyState()`, `storageSaveDailyState()`

### Files to Modify

5. **[firmware/src/main.cpp](firmware/src/main.cpp)**
   - Add `drinksUpdate()` call in loop (line ~710, after calibration)
   - Add `drinksInit()` call in setup (line ~555, after storage init)
   - Modify `drawMainScreen()` (line 338) to add daily intake visualization
   - Add `drawHumanFigure()` function for human silhouette visualization
   - Add `drawGlassGrid()` function for tumbler grid visualization (5×2 grid)

6. **[firmware/include/config.h](firmware/include/config.h)**
   - Add drink detection constants:
     - `DRINK_MIN_THRESHOLD_ML = 30`
     - `DRINK_REFILL_THRESHOLD_ML = 100`
     - `DRINK_AGGREGATION_WINDOW_SEC = 300` (5 minutes)
     - `DRINK_DAILY_RESET_HOUR = 4` (4am)
     - `DRINK_DISPLAY_UPDATE_THRESHOLD_ML = 50`
     - `DRINK_MAX_RECORDS = 200`
   - Add display visualization option:
     - `DAILY_INTAKE_DISPLAY_MODE = 0` (0 = human figure, 1 = tumbler grid)

---

## Data Structures

### DrinkRecord (9 bytes)
```cpp
struct DrinkRecord {
    uint32_t timestamp;         // Unix timestamp
    int16_t  amount_ml;         // Consumed (+) or refilled (-)
    uint16_t bottle_level_ml;   // Level after event (0-830ml)
    uint8_t  flags;             // 0x01=synced, 0x02=day_boundary
};
```

### DailyState (26 bytes)
```cpp
struct DailyState {
    uint32_t last_reset_timestamp;      // Last 4am reset
    uint32_t last_drink_timestamp;      // Last drink event
    int32_t  last_recorded_adc;         // Baseline for drink detection
    uint16_t daily_total_ml;            // Today's cumulative intake
    uint16_t drink_count_today;         // Number of drinks today
    uint16_t last_displayed_total_ml;   // Display hysteresis
    uint8_t  aggregation_window_active; // 0 or 1
    uint32_t aggregation_window_start;  // Window start timestamp
};
```

### NVS Storage Schema

**Namespace:** `"aquavate"` (existing)

| Key | Type | Size | Description |
|-----|------|------|-------------|
| `drink_meta` | Blob | 10 bytes | CircularBufferMetadata |
| `daily_state` | Blob | 26 bytes | DailyState |
| `drink_000` to `drink_199` | Blob | 9 bytes each | DrinkRecord entries |

**Total storage:** ~1.3 KB (200 records × 9 bytes + 36 bytes metadata)

---

## Drink Detection Algorithm

### Core Logic (in `drinksUpdate()`)

Called every 2 seconds when:
- Calibrated (`g_calibrated == true`)
- Time valid (`g_time_valid == true`)
- Not in calibration mode (`cal_state == CAL_IDLE`)
- Bottle upright and stable (`gesture == GESTURE_UPRIGHT_STABLE`)

**Steps:**
1. Load `DailyState` from NVS
2. Check if daily reset needed (first drink after 4am boundary)
3. Convert current ADC to ml using `calibrationGetWaterWeight()`
4. Calculate delta from last recorded baseline
5. If delta ≥ 30ml (drink detected):
   - Check if within 5-minute aggregation window
   - If yes: update last drink record (aggregate)
   - If no: create new drink record, start new window
   - Update daily total and drink count
   - Trigger display update if total changed ≥50ml
6. If delta ≤ -100ml (refill detected):
   - Create refill record (negative amount)
   - Close aggregation window
   - Update display
7. Save updated `DailyState` to NVS

### Daily Reset Logic (in `shouldResetDailyCounter()`)

Reset occurs when:
- **First run:** `last_reset_timestamp == 0`
- **Day changed AND current time ≥ 4am**
- **OR 20+ hours since last reset** (handles night-only drinking)

Algorithm:
- Convert timestamps to local time using `g_timezone_offset`
- Extract year, day-of-year, hour from `struct tm`
- Compare calendar days and time-of-day
- Return true if crossed 4am boundary

---

## Display Design

### Display Mode Configuration

The firmware supports two visualization modes for daily intake (configurable in config.h):

**Mode 0: Human Figure (50×83px bitmap)**
- Human silhouette fills from bottom to top
- Requires two bitmaps: outline + filled versions
- More intuitive "you're filling up" metaphor

**Mode 1: Tumbler Grid (44×86px, 5 rows × 2 columns)**
- 10 tapered tumbler icons in a grid
- Each glass represents 10% of daily goal (250ml per glass @ 2500ml goal)
- Fills from bottom-left to top-right with fractional fill support
- More granular visual feedback (10 steps vs continuous)

### Screen Layout (250×122px landscape)

```
┌──────────────────────────────────────────────────┐ 250px wide
│   [Time]          1250ml/2500ml          [Batt]  │ Row 1 (5-15px)
│                                                   │
│ [Bottle     [Daily Viz]                          │ Row 2-4 (20-110px)
│  40×90]     Human: 50×83 @ (185,20)              │
│  @(10,20)   OR Tumblers: 44×86 @ (195,20)        │
│  Current    Daily intake                          │
│  level                                            │
│                                                   │
└──────────────────────────────────────────────────┘ 122px tall
```

### Tumbler Grid Graphic (Primitive-based)

**Implementation Status: ✅ COMPLETE**

**Specifications:**
- Grid: 5 rows × 2 columns = 10 tumblers
- Each tumbler: 18×16 pixels with 4px horizontal spacing, 2px vertical spacing
- Total dimensions: 44px wide × 86px tall
- Position: (195, 20) on right side of screen

**Tumbler Shape:**
- Tapered drinking glass (wide rim at top, narrow base at bottom)
- Top edge: Full 18px width (rim)
- Bottom edge: 12px width (inset 3px on each side)
- Sloped sides for realistic tumbler appearance

**Fill Logic:**
- Each tumbler represents 10% of daily goal (250ml @ 2500ml goal)
- Fills from bottom-left to top-right (left column fills first in each row)
- Supports fractional fills (e.g., 55% = 5 full + 1 half-filled tumbler)
- Fill rendered from bottom-up within each glass
- Fill respects tapered shape (wider at top, narrower at bottom)

**Function:** `drawGlassGrid(int x, int y, float fill_percent)`

**Advantages:**
- No bitmap assets required (drawn with primitives)
- Granular 10-step visual feedback
- Fractional fill provides precise progress indication
- Compact layout fits alongside bottle graphic

### Human Figure Graphic (Bitmap-based)

**Implementation Status: ✅ COMPLETE**

**Asset:** [firmware/assets/EmptyMan.png](firmware/assets/EmptyMan.png)
- Original dimensions: 259×432px (RGBA PNG)
- Scaled to: 50×83px (maintains aspect ratio ~1:1.67)
- Final dimensions tested on hardware

**Conversion Requirements:**
1. Scale PNG to target size (use imagemagick or online tool)
2. Convert to 1-bit monochrome (black/white only)
3. Generate C byte array using tool like `image2cpp` or Python PIL script
4. Store as `PROGMEM` array in main.cpp (following water_drop_bitmap pattern)

**Recommended dimensions:** 50×83px (better detail at larger size)

**Fill Logic - Mask Rendering:**
The human figure will be drawn twice:
1. **Outline (full figure):** Draw complete bitmap in black
2. **Water fill (partial):** Draw filled rect clipped to figure bounds

```cpp
void drawHumanFigure(int x, int y, float fill_percent) {
    // Draw full outline
    display.drawBitmap(x, y, human_figure_bitmap,
                      HUMAN_FIGURE_WIDTH, HUMAN_FIGURE_HEIGHT, EPD_BLACK);

    // Calculate fill height from bottom
    int fill_height = (int)(HUMAN_FIGURE_HEIGHT * fill_percent);
    int fill_y = y + HUMAN_FIGURE_HEIGHT - fill_height;

    // Draw filled rectangle (clipped to figure bounds using setClipRect)
    // Or use inverted bitmap approach: draw "filled" version partially
    display.fillRect(x + 8, fill_y, HUMAN_FIGURE_WIDTH - 16, fill_height, EPD_BLACK);
}
```

**Alternative: Two Bitmaps (Cleaner)**
- `human_figure_outline.h` - Just the outline (hollow)
- `human_figure_filled.h` - Completely filled version
- Render filled version clipped from bottom based on fill_percent

### Display Update Strategy

**Update triggers:**
- Daily intake changed ≥50ml (not every drink)
- Bottle level changed ≥5ml (existing logic)
- Wake from sleep (always refresh)

**Hysteresis:** Track `last_displayed_total_ml` separately to avoid unnecessary refreshes

---

## Integration Points

### main.cpp - setup() (around line 555)
```cpp
// After time/storage initialization
if (g_time_valid) {
    drinksInit();
    DailyState state;
    if (storageLoadDailyState(state)) {
        Serial.print("Today's intake: ");
        Serial.print(state.daily_total_ml);
        Serial.println("ml");
    }
} else {
    Serial.println("WARNING: Drink tracking disabled - time not set");
}
```

### main.cpp - loop() (around line 710)
```cpp
// After calibration update, before display updates
if (cal_state == CAL_IDLE && g_calibrated && g_time_valid && nauReady) {
    static unsigned long last_drink_check = 0;
    if (millis() - last_drink_check >= 2000) {
        last_drink_check = millis();
        if (gesture == GESTURE_UPRIGHT_STABLE) {
            drinksUpdate(current_adc, g_calibration);
        }
    }
}
```

### main.cpp - drawMainScreen() (line 338)

**Add after existing bottle graphic (around line 395):**
```cpp
// Draw daily intake (only if time is set)
if (g_time_valid) {
    DailyState daily_state;
    if (storageLoadDailyState(daily_state)) {
        float daily_fill = (float)daily_state.daily_total_ml / 2500.0f;
        if (daily_fill > 1.0f) daily_fill = 1.0f;

        drawHumanFigure(60, 25, daily_fill);

        display.setTextSize(2);
        display.setCursor(120, 40);
        display.print("Daily:");
        display.setCursor(120, 60);
        char txt[16];
        snprintf(txt, sizeof(txt), "%dml", daily_state.daily_total_ml);
        display.print(txt);

        display.setTextSize(1);
        display.setCursor(120, 80);
        display.print("Goal: 2500ml");
    } else {
        // Not initialized yet
        display.setTextSize(1);
        display.setCursor(70, 55);
        display.print("Drink to start");
    }
} else {
    // Time not set
    display.setTextSize(1);
    display.setCursor(65, 55);
    display.print("Set time via USB");
}
```

**Add drawHumanFigure() function (before drawMainScreen):**
```cpp
void drawHumanFigure(int x, int y, float fill_percent) {
    // Draw full human figure outline
    display.drawBitmap(x, y, human_figure_bitmap,
                      HUMAN_FIGURE_WIDTH, HUMAN_FIGURE_HEIGHT, EPD_BLACK);

    // Calculate fill height from bottom
    int fill_height = (int)(HUMAN_FIGURE_HEIGHT * fill_percent);
    int fill_y = y + HUMAN_FIGURE_HEIGHT - fill_height;

    // Draw filled rectangle inside figure bounds
    // Adjust x/width to stay within figure outline (±8px padding)
    if (fill_height > 0) {
        display.fillRect(x + 10, fill_y, HUMAN_FIGURE_WIDTH - 20,
                        fill_height, EPD_BLACK);
    }
}
```

---

## Testing Plan

### Unit Tests

1. **Drink Detection Threshold**
   - Fill to 500ml, drink 50ml with measuring cup
   - Verify: Serial shows drink event, daily total = 50ml
   - Display shows human figure ~2% filled (50/2500)

2. **5-Minute Aggregation**
   - Drink 30ml, wait 2 minutes, drink 40ml
   - Verify: Single record with 70ml (not two records)
   - Wait 6 minutes, drink 30ml
   - Verify: New record created

3. **Daily Reset at 4am**
   - Set time to 3:30am via `SET_TIME`
   - Drink 100ml (daily total = 100ml)
   - Set time to 4:15am
   - Drink 50ml
   - Verify: Daily total resets to 50ml, serial shows "Daily reset triggered"

4. **Bottle Refill**
   - Start at 200ml level
   - Add 300ml water
   - Verify: Refill record created (negative amount), daily total unchanged

5. **Display Hysteresis**
   - Daily total = 500ml
   - Drink 20ml (total = 520ml)
   - Verify: Display NOT refreshed (below 50ml threshold)
   - Drink 35ml (total = 555ml)
   - Verify: Display refreshes (≥50ml change)

6. **Calibration Mode Isolation**
   - Daily total = 500ml
   - Enter calibration (hold inverted 5s)
   - Complete calibration (weight changes drastically)
   - Verify: Daily total still 500ml (no spurious drinks)

### Integration Test (Full Day Simulation)

| Time | Action | Expected Daily Total | Drink Count |
|------|--------|---------------------|-------------|
| 8:00am | Drink 250ml | 250ml | 1 |
| 8:03am | Drink 100ml | 350ml | 1 (aggregated) |
| 10:30am | Drink 200ml | 550ml | 2 |
| 12:00pm | Refill bottle | 550ml | 2 (unchanged) |
| 1:00pm | Drink 300ml | 850ml | 3 |
| 4:00pm | Drink 150ml | 1000ml | 4 |
| 8:00pm | Drink 250ml | 1250ml | 5 |
| 4:01am | Drink 200ml (next day) | 200ml | 1 (RESET) |

### Hardware Verification

- Test on actual Adafruit Feather + NAU7802 + LIS3DH hardware
- Verify NVS persistence across deep sleep cycles
- Measure power consumption during wake (should remain ~30s wake window)
- Test with real drinks (not just test water additions)

---

## Edge Cases Handled

1. **Midnight drinking:** Drinks before 4am count toward previous day
2. **Timezone changes:** Daily reset uses `g_timezone_offset` from NVS
3. **Rapid weight fluctuations:** Ignored (only process when `UPRIGHT_STABLE`)
4. **Battery dies mid-drink:** NVS atomic writes preserve completed records
5. **Time not set:** Drink tracking disabled, display shows "Set time via USB"
6. **Calibration mode active:** Drink detection skipped during calibration

---

## Before Starting Implementation

**Update PROGRESS.md:**
```markdown
## Active Work

**Phase 2: Daily Water Intake Tracking** (Started 2026-01-13)
- Plan: Plans/007-daily-intake-tracking.md
- Status: Day 1 - Bitmap rendering validation
- Branch: TBD (create after bitmap validation succeeds)

Key features:
- Drink event detection with 5-minute aggregation
- Daily reset at 4am boundary
- NVS circular buffer storage (200 records)
- Human figure visualization on e-paper
- Hardcoded 2500ml daily goal
```

---

## Implementation Checklist

### Day 1: Bitmap Testing & Display Prototype (VALIDATE APPROACH FIRST)

**Goal:** Validate that bitmap rendering looks good on e-paper before building drink tracking

- [ ] Convert [EmptyMan.png](firmware/assets/EmptyMan.png) to C bitmap array:
  - [ ] Scale to 50×83px using image2cpp or imagemagick
  - [ ] Convert to 1-bit monochrome
  - [ ] Generate byte array (see "Bitmap Conversion Guide" section)
  - [ ] Add to [main.cpp](firmware/src/main.cpp) as `human_figure_bitmap[]` PROGMEM
  - [ ] Add `#define HUMAN_FIGURE_WIDTH 50` and `HUMAN_FIGURE_HEIGHT 83`
- [ ] Add `drawHumanFigure()` function to [main.cpp](firmware/src/main.cpp)
- [ ] Create temporary test code in setup() or welcome screen to render fills
- [ ] **CRITICAL TEST:** Upload to hardware and photograph e-paper at different fill levels:
  - [ ] 0% fill (empty outline only) - does outline look clean?
  - [ ] 25% fill - does fill rectangle align with figure?
  - [ ] 50% fill - is fill visible and proportional?
  - [ ] 75% fill - does it look like it's "filling up"?
  - [ ] 100% fill - does entire figure appear filled?
- [ ] **DECISION POINT:** Does bitmap + fill rectangle approach work visually?
  - ✅ **If YES:** Continue with current plan (proceed to Day 2)
  - ❌ **If NO:** Pivot to alternative:
    - Option A: Two bitmaps (outline + filled, mask approach)
    - Option B: Draw primitives (circles/rectangles as originally planned)
    - Option C: Simplify to progress bar instead of human figure
    - Option D: Use different graphic (water bottle outline that fills)
- [ ] If bitmap works, test complete screen layout with placeholder daily total (e.g., "1250ml")
- [ ] Adjust fill rectangle padding/dimensions if needed (currently ±10px from edges)
- [ ] Document final rendering approach and continue to storage implementation

### Day 2: Data Structures & Storage
- [ ] Create [drinks.h](firmware/include/drinks.h) with structures
- [ ] Create [storage_drinks.h](firmware/include/storage_drinks.h)
- [ ] Implement [storage_drinks.cpp](firmware/src/storage_drinks.cpp):
  - [ ] `storageSaveDrinkRecord()` (circular buffer write)
  - [ ] `storageLoadLastDrinkRecord()` (find last index)
  - [ ] `storageUpdateLastDrinkRecord()` (aggregate drinks)
  - [ ] `storageLoadDailyState()` / `storageSaveDailyState()`
- [ ] Add constants to [config.h](firmware/include/config.h)
- [ ] Test NVS functions with serial debug commands

### Day 3: Drink Detection
- [ ] Implement [drinks.cpp](firmware/src/drinks.cpp):
  - [ ] `drinksInit()` (load or initialize state)
  - [ ] `drinksUpdate()` (main detection loop)
  - [ ] `shouldResetDailyCounter()` (4am boundary check)
  - [ ] `getCurrentUnixTime()` (helper for RTC + timezone)
- [ ] Test drink detection via serial output (no display yet)
- [ ] Test 5-minute aggregation logic
- [ ] Test daily reset at 4am boundary

### Day 4: System Integration
- [ ] Add `drinksInit()` call to setup()
- [ ] Add `drinksUpdate()` call to loop()
- [ ] Run full integration tests (unit + full day simulation)
- [ ] Test on hardware with real drinks
- [ ] Verify NVS persistence across reboots and deep sleep
- [ ] Check power consumption (should be unchanged)

### Day 5: Documentation & Merge
- [ ] Update [PROGRESS.md](PROGRESS.md) - mark Phase 2 complete
- [ ] Update [docs/PRD.md](docs/PRD.md) with implemented Phase 2 features:
  - [ ] Update Phase 1 section to mark drink detection as "IMPLEMENTED"
  - [ ] Document actual NVS storage schema used
  - [ ] Update display specifications with human figure dimensions
  - [ ] Note any deviations from original plan (if applicable)
  - [ ] Add verification results (accuracy, power consumption, etc.)
- [ ] Add code comments and function headers to all new files
- [ ] Update [CLAUDE.md](CLAUDE.md) with new modules:
  - [ ] Add drinks.cpp, storage_drinks.cpp to file structure
  - [ ] Update build/test instructions if needed
- [ ] Create Phase 2 summary document in Plans folder
- [ ] Merge `usb-time-setting` branch to master (if not already done)
- [ ] Create new branch for Phase 2 or commit directly to master

---

## Performance & Power

**NVS Write Wear:**
- 20 drinks/day × 365 days = 7,300 writes/year
- ESP32 NVS: ~100,000 write cycles
- Lifespan: **13+ years**

**Display Refresh Reduction:**
- Without threshold: 20 drinks/day × 2s = 40s refresh time
- With 50ml threshold: ~6 refreshes/day × 2s = 12s refresh time
- **70% reduction in display power**

**Wake Duration:** 30s (unchanged from current implementation)

---

## Bitmap Conversion Guide

### Converting EmptyMan.png to C Array

**Method 1: Online Tool (image2cpp)**
1. Visit: https://javl.github.io/image2cpp/
2. Upload `firmware/assets/EmptyMan.png`
3. Settings:
   - Canvas size: 50×83 (or 40×67)
   - Scaling: Scale to fit, keep aspect ratio
   - Background color: White
   - Invert colors: No
   - Brightness/Threshold: Adjust to get clean outline
   - Code output format: Arduino code, PROGMEM
4. Copy generated array
5. Paste into main.cpp below water_drop_bitmap

**Method 2: ImageMagick + Python**
```bash
# Scale and convert to monochrome
convert firmware/assets/EmptyMan.png -resize 50x83 -monochrome -threshold 50% human_50x83.png

# Use Python PIL to generate C array
python3 << 'EOF'
from PIL import Image
img = Image.open('human_50x83.png').convert('1')
width, height = img.size
pixels = list(img.getdata())

print(f"const unsigned char PROGMEM human_figure_bitmap[] = {{")
for y in range(height):
    for byte_x in range((width + 7) // 8):
        byte_val = 0
        for bit in range(8):
            x = byte_x * 8 + bit
            if x < width and pixels[y * width + x] == 0:  # 0 = black
                byte_val |= (0x80 >> bit)
        print(f"    0x{byte_val:02X},", end=" ")
    print()
print("};")
print(f"#define HUMAN_FIGURE_WIDTH {width}")
print(f"#define HUMAN_FIGURE_HEIGHT {height}")
EOF
```

**Expected Output Format:**
```cpp
const unsigned char PROGMEM human_figure_bitmap[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ... (50 × 83 = 4150 pixels ÷ 8 = ~519 bytes)
};

#define HUMAN_FIGURE_WIDTH 50
#define HUMAN_FIGURE_HEIGHT 83
```

---

## Future Extensions (Post-MVP)

1. **Configurable Daily Goal:** Add `SET_GOAL` serial command, store in DailyState
2. **BLE Sync:** Use `flags` field for sync status, retrieve unsynced records
3. **Weekly Summaries:** Add DailySummary structure, render bar chart on e-paper
4. **Multiple Bottles:** Add bottle_id to DrinkRecord
5. **Export Data:** Add serial command to dump drink records as JSON

---

## Critical Files Summary

**New Files (4):**
1. [firmware/include/drinks.h](firmware/include/drinks.h) - Data structures
2. [firmware/src/drinks.cpp](firmware/src/drinks.cpp) - Detection algorithm (~300 lines)
3. [firmware/include/storage_drinks.h](firmware/include/storage_drinks.h) - Storage API
4. [firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp) - NVS management (~200 lines)

**Modified Files (2):**
5. [firmware/src/main.cpp](firmware/src/main.cpp) - Integration + display (~50 lines added)
6. [firmware/include/config.h](firmware/include/config.h) - Constants (~15 lines added)

**Total new code:** ~550-600 lines across 6 files

---

## Verification Criteria (Success Definition)

✅ **Phase 2 Complete When:**
- Drink events (≥30ml) detected and stored in NVS
- 5-minute aggregation works (multiple sips → single record)
- Daily reset occurs on first drink after 4am
- Human figure visualization displays correctly (0-100% fill)
- Daily total shows accurate ml count
- Display updates only when intake changes ≥50ml
- System persists data across deep sleep cycles
- No spurious drink events during calibration mode
- Full day simulation test passes
- Hardware tested with real drinks

