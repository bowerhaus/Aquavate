# Plan: Two Drink Types (Gulp vs Pour) with No Aggregation

## User Requirements

- **Two drink types**:
  - **Gulp**: Small volumes (<100ml)
  - **Pour**: Larger volumes (≥100ml)
- **No aggregation**: Every detected drink event is saved as a separate record
- **Volume threshold detection**: Classification based purely on amount consumed
- **Preserve all existing functionality**: Display, daily totals, refill detection, time management

## Pros and Cons Analysis

### ✅ Pros

1. **Granular behavior insights**
   - Can see drinking patterns (frequent small sips vs infrequent large drinks)
   - Useful for hydration coaching or health analysis in future iOS app
   - Better data for identifying drinking habits (e.g., "user takes 20 gulps/day averaging 40ml")

2. **Simpler logic**
   - Removes 5-minute aggregation window state machine
   - No need to track `aggregation_window_active` or `aggregation_window_start`
   - One code path per drink detection (no update-last-record vs create-new-record branching)
   - Easier to debug and test

3. **Accurate timestamps**
   - Each gulp/pour has its exact timestamp (not just the first sip in a 5-minute window)
   - Better for time-series analysis in app

4. **No loss of information**
   - Current aggregation loses timing data (can't see 3 separate gulps, only the merged 120ml)
   - New system preserves every drinking event

5. **Type distinction enables future features**
   - Could filter/sort by drink type in app
   - Different visualizations (e.g., "15 gulps + 5 pours today")
   - Track trends (e.g., "user drinks more gulps in afternoon")

### ⚠️ Cons

1. **Faster circular buffer wraparound** (MITIGATED by increasing to 600 records)
   - Current system: 200 records ÷ ~6 aggregated drinks/day = **33+ days** of history
   - New system at 200 records: 200 ÷ 20 drinks/day = **10 days** of history
   - **Updated to 600 records**: 600 ÷ 20 drinks/day = **30 days** of history ✅
   - Provides full month coverage while using only ~6 KB of available NVS space

2. **More NVS flash writes**
   - Every drink = 1 NVS write (vs aggregated drinks updating same record)
   - ESP32 NVS wear: ~100,000 write cycles per sector
   - At 20 drinks/day: 100,000 ÷ 20 = **~13 years** before wear-out (acceptable)

3. **Potential noise from small sips**
   - 30ml minimum threshold still applies, so tiny sips (<30ml) won't create records
   - But user taking many 30-35ml micro-sips could create "noisy" data
   - Mitigation: Could increase `DRINK_MIN_THRESHOLD_ML` from 30ml to 40-50ml if needed

4. **More BLE sync overhead (future)**
   - Phase 3 BLE sync will transfer more records per session
   - 20 records/day vs 6 aggregated records = 3.3x more data
   - Impact: Minimal (each record is 9 bytes, 20 records = 180 bytes vs 54 bytes)

5. **Display behavior unchanged**
   - Daily total still shown the same way (e.g., "1350ml")
   - Drink type (gulp vs pour) not visible on e-paper display
   - This is fine for MVP, but type distinction only visible in future app

### 🎯 Recommendation

**Proceed with this approach.** The pros significantly outweigh the cons:
- 30 days of history (600 records) provides full month coverage
- Flash wear is negligible (13-year lifespan even with increased record count)
- Simpler code is easier to maintain and debug
- Granular data enables richer future app features
- Storage footprint (~6 KB) is only half of available NVS space

---

## Implementation Plan

### Phase 1: Data Structure Changes

**Files to modify:**
- [firmware/include/drinks.h](firmware/include/drinks.h)
- [firmware/src/config.h](firmware/src/config.h)

**Changes:**

1. **Add drink type to `DrinkRecord`** (increase from 9 to 10 bytes):
   ```cpp
   struct DrinkRecord {
       uint32_t timestamp;         // Unix timestamp (seconds since epoch)
       int16_t  amount_ml;         // Consumed (+) or refilled (-) amount in ml
       uint16_t bottle_level_ml;   // Bottle water level after event (0-830ml)
       uint8_t  flags;             // Bit flags: 0x01=synced to app, 0x02=day_boundary
       uint8_t  type;              // NEW: 0=gulp (<100ml), 1=pour (≥100ml)
   };
   ```

2. **Remove aggregation fields from `DailyState`**:
   ```cpp
   struct DailyState {
       uint32_t last_reset_timestamp;
       uint32_t last_drink_timestamp;
       int32_t  last_recorded_adc;
       uint16_t daily_total_ml;
       uint16_t drink_count_today;
       uint16_t last_displayed_total_ml;
       // REMOVE: aggregation_window_active
       // REMOVE: aggregation_window_start
   };
   ```
   (Reduces from 26 bytes to 18 bytes)

3. **Update storage capacity and add drink type constants to `config.h`**:
   ```cpp
   #define DRINK_MAX_RECORDS               600     // Circular buffer capacity (30 days at 20 drinks/day)

   // Drink type classification
   #define DRINK_TYPE_GULP             0   // Small drink (<100ml)
   #define DRINK_TYPE_POUR             1   // Large drink (≥100ml)
   #define DRINK_GULP_THRESHOLD_ML     100 // Threshold for gulp vs pour
   ```

4. **Optional: Adjust minimum threshold** (if micro-sip noise is concern):
   ```cpp
   #define DRINK_MIN_THRESHOLD_ML      40  // Increase from 30ml to reduce noise
   ```

---

### Phase 2: Core Logic Changes

**File to modify:**
- [firmware/src/drinks.cpp](firmware/src/drinks.cpp)

**Changes:**

1. **Simplify `drinksUpdate()` function**:
   - **Remove** 5-minute aggregation window logic
   - **Remove** `aggregation_window_active` checks
   - **Remove** `storageUpdateLastDrinkRecord()` path (updating last record)
   - **Keep** refill detection (closes aggregation → no longer needed, but keep refill logic)
   - **Add** drink type classification based on volume

2. **New drink detection flow**:
   ```cpp
   void drinksUpdate(int32_t current_adc, const CalibrationData& cal) {
       // ... existing weight calculation ...

       // Drink detection (weight decreased by ≥40ml)
       if (delta_ml >= DRINK_MIN_THRESHOLD_ML) {
           // Classify drink type
           uint8_t drink_type = (delta_ml >= DRINK_GULP_THRESHOLD_ML)
                                ? DRINK_TYPE_POUR
                                : DRINK_TYPE_GULP;

           // Create new DrinkRecord (ALWAYS create new, no aggregation)
           DrinkRecord record;
           record.timestamp = now();
           record.amount_ml = delta_ml;
           record.bottle_level_ml = current_water_level_ml;
           record.flags = 0x00;  // Not synced, not day boundary
           record.type = drink_type;

           // Save to NVS
           storageSaveDrinkRecord(record);

           // Update daily state
           g_daily_state.daily_total_ml += delta_ml;
           g_daily_state.drink_count_today++;
           g_daily_state.last_drink_timestamp = record.timestamp;
           storageSaveDailyState(g_daily_state);

           // Update baseline for next drink detection
           g_daily_state.last_recorded_adc = current_adc;
       }

       // Refill detection (weight increased by ≥100ml)
       if (refill_ml >= DRINK_REFILL_THRESHOLD_ML) {
           // Update baseline ADC (don't create drink record)
           g_daily_state.last_recorded_adc = current_adc;
           // Note: No longer need to "close aggregation window"
       }
   }
   ```

3. **Daily reset function** (`drinksCheckDailyReset`):
   - Remove aggregation window clearing (no longer exists)
   - Keep daily_total_ml, drink_count_today reset

---

### Phase 3: Storage Changes

**File to modify:**
- [firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp)

**Changes:**

1. **Remove `storageUpdateLastDrinkRecord()` function** (no longer needed)
2. **Keep all other functions unchanged**:
   - `storageSaveDrinkRecord()` - Works as-is (circular buffer handles new 10-byte struct)
   - `storageLoadLastDrinkRecord()` - Works as-is
   - `storageLoadDailyState()` / `storageSaveDailyState()` - Works as-is (struct size change handled by NVS)
   - Circular buffer logic unchanged

---

### Phase 4: Debug/Testing Support

**File to modify:**
- [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp)

**Changes:**

1. **Update `DUMP_DRINKS` command** to show drink type:
   ```
   [000] 2026-01-14 08:32:15 | +120ml (POUR) | Level: 650ml | Flags: 0x00
   [001] 2026-01-14 08:45:22 | +35ml  (GULP) | Level: 615ml | Flags: 0x00
   [002] 2026-01-14 09:12:08 | +45ml  (GULP) | Level: 570ml | Flags: 0x00
   ```

2. **Update `GET_DAILY_STATE` command** to remove aggregation window status:
   ```
   Daily Total: 1350ml / 2500ml (54%)
   Drink Count: 18 drinks today
   Last Drink: 2026-01-14 14:32:15 (45ml GULP)
   // REMOVE: Aggregation window status
   ```

3. **Update `GET_LAST_DRINK` command** to show type:
   ```
   Last Drink: 2026-01-14 14:32:15
   Amount: 45ml (GULP)
   Bottle Level: 570ml
   Flags: 0x00 (not synced)
   ```

---

### Phase 5: Display Changes (Optional)

**File to modify:**
- [firmware/src/main.cpp](firmware/src/main.cpp) (lines 675-729, render_ui_normal_mode)

**Changes (Optional - for future):**

Currently, the e-paper display shows:
- Daily total (e.g., "1350ml")
- Fill percentage (progress bar or human silhouette)
- Drink count is tracked but not displayed

**Possible enhancements** (not required for MVP):
1. Show drink count with type breakdown: "18 drinks (12 gulps, 6 pours)"
2. Show last drink type icon (small cup vs large cup)

**Recommendation:** Skip display changes for now. Keep MVP simple (just show daily total). Add type visualization in Phase 4 iOS app where there's more screen space.

---

## Critical Files to Modify

| File | Lines of Code | Changes |
|------|---------------|---------|
| [firmware/include/drinks.h](firmware/include/drinks.h) | ~60 | Add `type` field to `DrinkRecord`, remove aggregation fields from `DailyState` |
| [firmware/src/drinks.cpp](firmware/src/drinks.cpp) | ~295 | Remove aggregation logic, add type classification (net reduction ~50 lines) |
| [firmware/src/storage_drinks.cpp](firmware/src/storage_drinks.cpp) | ~200 | Remove `storageUpdateLastDrinkRecord()` function (~30 lines) |
| [firmware/src/serial_commands.cpp](firmware/src/serial_commands.cpp) | ~150 | Update debug output to show drink type |
| [firmware/src/config.h](firmware/src/config.h) | ~150 | Update `DRINK_MAX_RECORDS` 200→600, add drink type constants, optionally adjust thresholds |

**Estimated total changes:** ~150 lines modified, ~80 lines removed (net simplification)

---

## Testing Strategy

### 1. **Unit Testing (Manual Serial Commands)**

After flashing firmware, use serial monitor to verify:

```bash
# Test gulp detection (<100ml)
1. Place bottle on scale (e.g., 500ml level)
2. Remove ~50ml water
3. Place back (trigger UPRIGHT_STABLE)
4. Run: GET_LAST_DRINK
   Expected: "Amount: ~50ml (GULP)"

# Test pour detection (≥100ml)
1. Remove ~150ml water
2. Place back
3. Run: GET_LAST_DRINK
   Expected: "Amount: ~150ml (POUR)"

# Test no aggregation (multiple drinks)
1. Remove ~40ml (gulp)
2. Wait 10 seconds
3. Remove ~40ml (another gulp)
4. Run: DUMP_DRINKS
   Expected: Two separate records, NOT aggregated into 80ml

# Test daily state
1. Run: GET_DAILY_STATE
   Expected: drink_count increments with each drink, no aggregation window status

# Test refill behavior
1. Add ~200ml water (refill)
2. Run: GET_LAST_DRINK
   Expected: No new drink record created (refills still ignored)
```

### 2. **Storage Verification**

```bash
# Test circular buffer wraparound
1. Create 605 drink records (mix of gulps/pours)
2. Run: DUMP_DRINKS
   Expected: Only last 600 records visible, oldest 5 overwritten

# Test NVS persistence
1. Create several drinks
2. Run: DUMP_DRINKS (note last 5 records)
3. Power cycle ESP32
4. Run: DUMP_DRINKS
   Expected: Same records present after reboot
```

### 3. **Display Integration**

```bash
# Test display updates
1. Create drink (any type)
2. Verify e-paper shows updated daily total
3. Verify fill percentage increases

# Test display hysteresis (50ml threshold)
1. Starting at 1000ml total
2. Create 40ml gulp
3. Expected: Display does NOT update (1040ml < 1050ml threshold)
4. Create 60ml pour
5. Expected: Display updates to 1100ml
```

### 4. **Daily Reset**

```bash
# Test 4am reset
1. Set time to 3:58am: SET_TIME 2026-01-14T03:58:00
2. Create drink at 3:59am
3. Set time to 4:01am: SET_TIME 2026-01-14T04:01:00
4. Create drink at 4:01am
5. Run: GET_DAILY_STATE
   Expected: daily_total_ml only includes 4:01am drink (reset occurred)
```

### 5. **Threshold Boundary Tests**

```bash
# Test gulp/pour boundary (100ml)
1. Remove exactly 99ml
   Expected: Type = GULP
2. Remove exactly 100ml
   Expected: Type = POUR
3. Remove exactly 101ml
   Expected: Type = POUR

# Test minimum threshold (30ml or 40ml)
1. Remove 29ml (if threshold=30) or 39ml (if threshold=40)
   Expected: No drink detected
2. Remove 30ml (if threshold=30) or 40ml (if threshold=40)
   Expected: Drink detected (GULP)
```

---

## Migration Notes

### Backwards Compatibility

**NVS Storage:** Existing drink records in NVS use 9-byte `DrinkRecord` struct. After firmware update:
- **Old records:** Will have garbage/zero in new `type` field (10th byte)
- **Impact:** Historical data type classification will be incorrect
- **Solution options:**
  1. **Ignore old records** - Clear NVS with `CLEAR_DRINKS` after update (simplest)
  2. **Infer type from amount_ml** - Add migration logic in `drinksInit()` to scan existing records and populate `type` field based on volume
  3. **Mark as unknown** - Add `DRINK_TYPE_UNKNOWN = 2` for pre-existing records

**Recommendation:** Use option 1 (clear drinks) for development. Add option 2 (infer type) if preserving historical data is important for production.

### Config Changes

If increasing `DRINK_MIN_THRESHOLD_ML` from 30ml → 40ml:
- Previous 30-39ml drinks will no longer be detected
- User should be aware of behavior change (if any)

---

## Future Enhancements (Out of Scope)

These are NOT part of this implementation but enabled by the new structure:

1. **iOS app drink type filtering**
   - Show "Gulps only" or "Pours only" in history view
   - Analyze drinking patterns (e.g., "You take mostly gulps in the morning")

2. **Adaptive thresholds**
   - Learn user's typical gulp size and adjust threshold
   - Detect abnormal drinking patterns (e.g., many tiny sips = possible dehydration)

3. **Drink type icons on e-paper display**
   - Show small cup icon for last gulp, large cup for last pour
   - Requires custom bitmap assets

4. **BLE sync optimization** (Phase 3)
   - Only sync unsynced records (flags & 0x01 == 0)
   - Clear synced flag after successful app transfer

---

## Summary

This change:
- ✅ **Simplifies firmware** (removes aggregation state machine)
- ✅ **Preserves granular data** (every drink event recorded)
- ✅ **Adds drink type classification** (gulp vs pour based on volume)
- ✅ **Maintains all existing features** (display, daily totals, refills, time management)
- ✅ **Reduces code complexity** (~80 lines removed, ~150 lines modified)
- ✅ **Increases storage capacity** (200 → 600 records = 30 days of history)
- ✅ **Efficient storage usage** (~6 KB NVS, only 50% of available space)
- ⚠️ **Increases NVS writes** (negligible impact, 13-year flash lifespan at 20 drinks/day)

**Storage Impact:**
- Previous: 200 records × 9 bytes = 1.8 KB (33 days with aggregation)
- New: 600 records × 10 bytes = 6.0 KB (30 days without aggregation)
- NVS available: ~12-16 KB (50% utilization)

**Estimated implementation time:** 2-3 hours (code changes + testing)

**Risk level:** Low (mostly removal of existing logic, simple threshold-based classification)
