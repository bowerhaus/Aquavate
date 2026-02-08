// drinks.cpp - Daily water intake tracking implementation
// Part of the Aquavate smart water bottle firmware
//
// Daily totals are computed dynamically from drink records using the 4am boundary,
// matching the iOS app's calculation. This ensures consistency between devices.

#include "drinks.h"
#include "storage_drinks.h"
#include "config.h"
#include "calibration.h"
#include "display.h"
#include <sys/time.h>
#include <time.h>

// External variables from main.cpp
extern int8_t g_timezone_offset;  // Timezone offset in hours
extern bool g_time_valid;         // True if time has been set
extern bool g_rtc_ds3231_present; // DS3231 RTC detected
extern bool g_debug_drink_tracking; // Debug flag for drink tracking

// Static state for drink detection
static DailyState g_daily_state;
static bool g_drinks_initialized = false;

// Cached computed values (recalculated on init and after changes)
static uint16_t g_cached_daily_total_ml = 0;
static uint16_t g_cached_drink_count = 0;

// RTC memory for drink detection baseline across deep sleep
// Persists last stable ADC reading to prevent false drink detection on wake
#define RTC_MAGIC_DRINKS 0x44524E4B  // "DRNK" in hex
RTC_DATA_ATTR uint32_t rtc_drinks_magic = 0;
RTC_DATA_ATTR int32_t rtc_last_stable_adc = 0;
RTC_DATA_ATTR float rtc_last_stable_water_ml = 0.0f;

// Helper: Get current Unix timestamp with timezone offset
uint32_t getCurrentUnixTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // Apply timezone offset (convert hours to seconds)
    return tv.tv_sec + (g_timezone_offset * 3600);
}

// Helper: Save timestamp to NVS on drink/refill events (for time persistence)
// Only saves if DS3231 RTC is not present
static void saveTimestampOnEvent(const char* event_type) {
    if (!g_rtc_ds3231_present) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        storageSaveLastBootTime(tv.tv_sec);
        DEBUG_PRINTF(g_debug_drink_tracking, "Time: Timestamp saved to NVS on %s\n", event_type);
    }
}

// Helper: Calculate today's 4am boundary timestamp
static uint32_t getTodayResetTimestamp() {
    uint32_t current_time = getCurrentUnixTime();
    time_t current_t = current_time;
    struct tm current_tm;
    gmtime_r(&current_t, &current_tm);

    // Calculate timestamp for 4am today
    struct tm reset_tm = current_tm;
    reset_tm.tm_hour = DRINK_DAILY_RESET_HOUR;
    reset_tm.tm_min = 0;
    reset_tm.tm_sec = 0;
    uint32_t today_reset_timestamp = (uint32_t)mktime(&reset_tm);

    // If we're before 4am today, use yesterday's 4am as the reset boundary
    if (current_tm.tm_hour < DRINK_DAILY_RESET_HOUR) {
        today_reset_timestamp -= 24 * 3600;  // Subtract 24 hours
    }

    return today_reset_timestamp;
}

// Calculate seconds until next 4am rollover
// Used for timer wake scheduling in deep sleep
// Returns 0 if time is not valid
uint32_t getSecondsUntilRollover() {
    if (!g_time_valid) {
        return 0;  // No timer if time not set
    }

    uint32_t current_time = getCurrentUnixTime();
    time_t current_t = current_time;
    struct tm current_tm;
    gmtime_r(&current_t, &current_tm);

    // Calculate timestamp for next 4am
    struct tm next_reset_tm = current_tm;
    next_reset_tm.tm_hour = DRINK_DAILY_RESET_HOUR;
    next_reset_tm.tm_min = 0;
    next_reset_tm.tm_sec = 0;
    uint32_t next_reset_timestamp = (uint32_t)mktime(&next_reset_tm);

    // If we're at or past 4am today, target tomorrow's 4am
    if (current_tm.tm_hour >= DRINK_DAILY_RESET_HOUR) {
        next_reset_timestamp += 24 * 3600;  // Add 24 hours
    }

    // Calculate seconds until rollover
    if (next_reset_timestamp > current_time) {
        return next_reset_timestamp - current_time;
    }

    return 0;  // Safety fallback
}

// Helper: Recalculate daily totals from drink records
// This is the authoritative calculation using the 4am boundary
static void recalculateDailyTotals() {
    uint32_t today_reset_timestamp = getTodayResetTimestamp();

    // Load metadata
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        g_cached_daily_total_ml = 0;
        g_cached_drink_count = 0;
        DEBUG_PRINTLN(g_debug_drink_tracking, "Drinks: Recalculated total = 0ml (no records)");
        return;
    }

    // Sum all today's non-deleted drink records
    uint16_t total_ml = 0;
    uint16_t drink_count = 0;

    for (uint16_t i = 0; i < meta.record_count; i++) {
        DrinkRecord record;
        if (storageGetDrinkRecord(i, record)) {
            // Check: from today (after 4am reset), not deleted, is a drink (positive amount)
            if (record.timestamp >= today_reset_timestamp &&
                (record.flags & 0x04) == 0 &&  // Not deleted
                record.amount_ml > 0) {         // Drink, not refill
                total_ml += (uint16_t)record.amount_ml;
                drink_count++;
            }
        }
    }

    g_cached_daily_total_ml = total_ml;
    g_cached_drink_count = drink_count;

    DEBUG_PRINTF(g_debug_drink_tracking, "Drinks: Recalculated total = %dml (%d drinks)\n", total_ml, drink_count);
}

// Initialize drink tracking system
void drinksInit() {
    if (!g_time_valid) {
        Serial.println("WARNING: Cannot initialize drinks - time not set");
        g_drinks_initialized = false;
        return;
    }

    // Load daily state from NVS (baseline ADC and display threshold)
    if (!storageLoadDailyState(g_daily_state)) {
        // First run or struct size changed - initialize state
        DEBUG_PRINTLN(g_debug_drink_tracking, "Initializing new daily state");
        memset(&g_daily_state, 0, sizeof(DailyState));
        storageSaveDailyState(g_daily_state);
    } else {
        Serial.printf("Drinks: Init loaded NVS baseline ADC=%d, last_displayed=%dml\n",
                       g_daily_state.last_recorded_adc, g_daily_state.last_displayed_total_ml);
    }

    // Note: baseline ADC is preserved from NVS (or RTC restore).
    // drinksUpdate() validates it against -100ml..1000ml range and
    // re-establishes from the first stable reading if baseline is 0 or invalid.

    // Calculate daily totals from records (authoritative source)
    recalculateDailyTotals();

    g_drinks_initialized = true;
}

// Check if drink tracking is already initialized
bool drinksIsInitialized() {
    return g_drinks_initialized;
}

// Get current daily total (computed from records)
uint16_t drinksGetDailyTotal() {
    return g_cached_daily_total_ml;
}

// Get current drink count (computed from records)
uint16_t drinksGetDrinkCount() {
    return g_cached_drink_count;
}

// Force recalculation of daily totals (public wrapper)
void drinksRecalculateTotals() {
    recalculateDailyTotals();
}

// Main drink detection and tracking update
// Returns true if a drink was recorded, false otherwise
bool drinksUpdate(int32_t current_adc, const CalibrationData& cal) {
    if (!g_drinks_initialized || !g_time_valid) {
        return false;
    }

    uint32_t current_time = getCurrentUnixTime();

    // Convert current ADC to ml using calibration
    float current_ml = calibrationGetWaterWeight(current_adc, cal);

    // Initialize baseline on first call (or after reset)
    // Also re-establish baseline if stored ADC produces an unreasonable water level
    // This handles cold boot with stale NVS data or corrupted values
    bool needs_baseline = (g_daily_state.last_recorded_adc == 0);

    if (!needs_baseline) {
        // Validate that stored baseline produces a reasonable water level
        float stored_baseline_ml = calibrationGetWaterWeight(g_daily_state.last_recorded_adc, cal);
        // Valid range: -100ml to 1000ml (allows for some drift/tare error)
        if (stored_baseline_ml < -100.0f || stored_baseline_ml > 1000.0f) {
            DEBUG_PRINTF(g_debug_drink_tracking, "Drinks: Invalid baseline detected (%.1fml) - re-establishing\n", stored_baseline_ml);
            needs_baseline = true;
        }
    }

    if (needs_baseline) {
        Serial.printf("Drinks: Establishing baseline (ADC=%d, %.1fml)\n",
                       current_adc, current_ml);
        g_daily_state.last_recorded_adc = current_adc;
        storageSaveDailyState(g_daily_state);
        return false;  // Wait for next reading before detecting drinks
    }

    // Calculate delta from last recorded baseline
    float baseline_ml = calibrationGetWaterWeight(g_daily_state.last_recorded_adc, cal);
    float delta_ml = baseline_ml - current_ml;  // Positive = water removed (drink)

    // Debug output for drink tracking
    if (g_debug_drink_tracking) {
        Serial.printf("Drinks: baseline=%.1fml, current=%.1fml, delta=%.1fml\n",
                      baseline_ml, current_ml, delta_ml);
    }

    // Detect drink event (≥30ml decrease)
    if (delta_ml >= DRINK_MIN_THRESHOLD_ML) {
        // Classify drink type based on volume
        uint8_t drink_type = (delta_ml >= DRINK_GULP_THRESHOLD_ML)
                             ? DRINK_TYPE_POUR
                             : DRINK_TYPE_GULP;
        const char* type_str = (drink_type == DRINK_TYPE_POUR) ? "POUR" : "GULP";

        Serial.printf("\n=== DRINK DETECTED: %.1fml (%s) ===\n", delta_ml, type_str);

        // Create new drink record
        DrinkRecord record;
        record.timestamp = current_time;
        record.amount_ml = (int16_t)delta_ml;
        record.bottle_level_ml = (uint16_t)current_ml;
        record.flags = 0;
        record.type = drink_type;

        // Try to save drink record to NVS
        bool record_saved = storageSaveDrinkRecord(record);

        // Update baseline
        g_daily_state.last_recorded_adc = current_adc;
        bool state_saved = storageSaveDailyState(g_daily_state);

        if (record_saved) {
            // Record saved successfully - recalculate from NVS (authoritative)
            recalculateDailyTotals();
        } else {
            // NVS write failed - update in-memory totals directly to preserve the drink
            Serial.println("WARNING: NVS write failed, updating in-memory totals");
            g_cached_daily_total_ml += (uint16_t)delta_ml;
            g_cached_drink_count++;
            displayNVSWarning();  // Show warning to user
        }

        if (!state_saved) {
            Serial.println("WARNING: Daily state save failed");
        }

        // Save timestamp to NVS on drink event (for time persistence)
        saveTimestampOnEvent("drink");

        Serial.printf("Daily total: %dml (%d drinks)\n",
                     g_cached_daily_total_ml, g_cached_drink_count);

        // Check if display should update (≥50ml change)
        uint16_t display_delta = abs((int)g_cached_daily_total_ml -
                                     (int)g_daily_state.last_displayed_total_ml);
        if (display_delta >= DRINK_DISPLAY_UPDATE_THRESHOLD_ML) {
            DEBUG_PRINTLN(g_debug_drink_tracking, "Display update threshold reached - should refresh");
            g_daily_state.last_displayed_total_ml = g_cached_daily_total_ml;
            storageSaveDailyState(g_daily_state);
        }
        return true;  // Drink was recorded
    }
    // Detect refill event (≥100ml increase)
    else if (delta_ml <= -DRINK_REFILL_THRESHOLD_ML) {
        Serial.printf("\n=== REFILL DETECTED: %.1fml ===\n", -delta_ml);

        // Refills are not saved as drink records
        // Only update the baseline ADC
        g_daily_state.last_recorded_adc = current_adc;
        storageSaveDailyState(g_daily_state);

        // Save timestamp to NVS on refill event (for time persistence)
        saveTimestampOnEvent("refill");

        DEBUG_PRINTLN(g_debug_drink_tracking, "Daily total unchanged (refill)");
    }
    // No significant change - update baseline for drift compensation
    // Only update if change is very small to avoid contaminating baseline
    // with intermediate readings when a second drink is in progress
    else {
        float abs_delta = (delta_ml > 0) ? delta_ml : -delta_ml;
        if (abs_delta < DRINK_DRIFT_THRESHOLD_ML) {
            g_daily_state.last_recorded_adc = current_adc;
        }
        // If delta is between drift threshold and drink threshold,
        // don't update baseline - a drink may be in progress
    }
    return false;  // No drink recorded
}

// Debug function: Get current daily state
void drinksGetState(DailyState& state) {
    state = g_daily_state;
}

// Reset daily intake (marks today's records as deleted)
void drinksResetDaily() {
    Serial.println("=== MANUAL DAILY RESET ===");

    uint32_t today_reset_timestamp = getTodayResetTimestamp();

    // Mark all today's drink records as deleted
    CircularBufferMetadata meta;
    if (storageLoadBufferMetadata(meta) && meta.record_count > 0) {
        for (uint16_t i = 0; i < meta.record_count; i++) {
            DrinkRecord record;
            if (storageGetDrinkRecord(i, record)) {
                if (record.timestamp >= today_reset_timestamp &&
                    (record.flags & 0x04) == 0 &&  // Not already deleted
                    record.amount_ml > 0) {         // Drink, not refill
                    // Mark as deleted
                    storageMarkDeleted(record.record_id);
                }
            }
        }
    }

    // Recalculate (should be 0 now)
    recalculateDailyTotals();

    // Reset display threshold
    g_daily_state.last_displayed_total_ml = 0;
    storageSaveDailyState(g_daily_state);

    Serial.println("Daily intake reset to 0ml");
}

// Cancel the most recent drink record (marks it as deleted)
bool drinksCancelLast() {
    uint32_t today_reset_timestamp = getTodayResetTimestamp();

    // Find the most recent non-deleted drink from today
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta) || meta.record_count == 0) {
        Serial.println("Drinks: No records to cancel");
        return false;
    }

    // Search backwards for the most recent drink
    DrinkRecord last_drink;
    bool found = false;

    // Iterate from newest to oldest
    for (int i = meta.record_count - 1; i >= 0; i--) {
        DrinkRecord record;
        if (storageGetDrinkRecord(i, record)) {
            if (record.timestamp >= today_reset_timestamp &&
                (record.flags & 0x04) == 0 &&  // Not deleted
                record.amount_ml > 0) {         // Drink, not refill
                last_drink = record;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        Serial.println("Drinks: No drinks to cancel");
        return false;
    }

    // Mark as deleted
    storageMarkDeleted(last_drink.record_id);

    // Recalculate totals
    recalculateDailyTotals();

    Serial.printf("Drinks: Cancelled drink of %dml. New total: %dml (%d drinks)\n",
                  last_drink.amount_ml, g_cached_daily_total_ml, g_cached_drink_count);

    return true;
}

// Debug function: Clear all drink records (for testing)
void drinksClearAll() {
    Serial.println("=== CLEARING ALL DRINK RECORDS ===");

    // Reset daily state
    memset(&g_daily_state, 0, sizeof(DailyState));
    storageSaveDailyState(g_daily_state);

    // Reset circular buffer metadata
    CircularBufferMetadata meta;
    meta.write_index = 0;
    meta.record_count = 0;
    meta.total_writes = 0;
    meta.next_record_id = 1;  // Start IDs at 1 (0 = invalid/unassigned)
    meta._reserved = 0;
    storageSaveBufferMetadata(meta);

    // Reset cached values
    g_cached_daily_total_ml = 0;
    g_cached_drink_count = 0;

    Serial.println("All drink records cleared");
}

// Save drink detection baseline to RTC memory before deep sleep
void drinksSaveToRTC() {
    rtc_last_stable_adc = g_daily_state.last_recorded_adc;

    // Load calibration to convert ADC to ml for logging
    CalibrationData cal;
    if (storageLoadCalibration(cal)) {
        rtc_last_stable_water_ml = calibrationGetWaterWeight(g_daily_state.last_recorded_adc, cal);
    } else {
        rtc_last_stable_water_ml = 0.0f;
    }

    rtc_drinks_magic = RTC_MAGIC_DRINKS;  // Mark as valid

    // Also save to NVS so baseline survives power cycles (better fallback)
    storageSaveDailyState(g_daily_state);

    DEBUG_PRINTF(g_debug_drink_tracking, "Drinks: Saved to RTC + NVS - baseline ADC=%d (%.0fml)\n",
                  rtc_last_stable_adc, rtc_last_stable_water_ml);
}

// Restore drink detection baseline from RTC memory after waking from deep sleep
// Returns true if valid state was restored, false if power cycle (magic invalid)
bool drinksRestoreFromRTC() {
    if (rtc_drinks_magic != RTC_MAGIC_DRINKS) {
        DEBUG_PRINTLN(g_debug_drink_tracking, "Drinks: No valid RTC state (power cycle)");
        return false;
    }

    // Restore baseline - this prevents false drink detection on wake
    g_daily_state.last_recorded_adc = rtc_last_stable_adc;

    DEBUG_PRINTF(g_debug_drink_tracking, "Drinks: Restored from RTC - baseline ADC=%d (%.0fml)\n",
                  rtc_last_stable_adc, rtc_last_stable_water_ml);

    return true;
}

// Get current baseline water level (for shake-to-empty detection)
float drinksGetBaselineWaterLevel(const CalibrationData& cal) {
    return calibrationGetWaterWeight(g_daily_state.last_recorded_adc, cal);
}

// Reset baseline ADC to current value (prevents re-detection after cancel)
void drinksResetBaseline(int32_t adc) {
    g_daily_state.last_recorded_adc = adc;
    storageSaveDailyState(g_daily_state);
    DEBUG_PRINTF(g_debug_drink_tracking, "Drinks: Baseline reset to ADC=%d\n", adc);
}
