// drinks.cpp - Daily water intake tracking implementation
// Part of the Aquavate smart water bottle firmware

#include "drinks.h"
#include "storage_drinks.h"
#include "config.h"
#include "calibration.h"
#include <sys/time.h>
#include <time.h>

// External variables from main.cpp
extern int8_t g_timezone_offset;  // Timezone offset in hours
extern bool g_time_valid;         // True if time has been set
extern bool g_rtc_ds3231_present; // DS3231 RTC detected

// Static state for drink detection
static DailyState g_daily_state;
static bool g_drinks_initialized = false;

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
        Serial.print("Time: Timestamp saved to NVS on ");
        Serial.println(event_type);
    }
}

// Helper: Check if daily counter should reset
// Returns true on first drink after 4am boundary
static bool shouldResetDailyCounter(uint32_t current_time) {
    // First run - always reset
    if (g_daily_state.last_reset_timestamp == 0) {
        Serial.println("First run - initializing daily counter");
        return true;
    }

    // Convert timestamps to struct tm for date comparison
    time_t last_reset_t = g_daily_state.last_reset_timestamp;
    time_t current_t = current_time;

    struct tm last_reset_tm;
    struct tm current_tm;

    gmtime_r(&last_reset_t, &last_reset_tm);
    gmtime_r(&current_t, &current_tm);

    // Check if 20+ hours have passed (handles night-only drinking)
    uint32_t hours_elapsed = (current_time - g_daily_state.last_reset_timestamp) / 3600;
    if (hours_elapsed >= 20) {
        Serial.println("20+ hours since last reset - triggering daily reset");
        return true;
    }

    // Check if we've crossed the 4am boundary
    // Day changed AND current time >= 4am
    if (last_reset_tm.tm_yday != current_tm.tm_yday ||
        last_reset_tm.tm_year != current_tm.tm_year) {
        // Different day - check if current time is past 4am
        if (current_tm.tm_hour >= DRINK_DAILY_RESET_HOUR) {
            Serial.println("Crossed 4am boundary - triggering daily reset");
            return true;
        }
    }

    return false;
}

// Helper: Perform atomic daily reset
static void performAtomicDailyReset(uint32_t current_time) {
    Serial.println("\n=== ATOMIC DAILY RESET ===");
    Serial.printf("Previous total: %dml (%d drinks)\n",
                 g_daily_state.daily_total_ml,
                 g_daily_state.drink_count_today);

    // Reset all daily counters atomically
    g_daily_state.daily_total_ml = 0;
    g_daily_state.drink_count_today = 0;
    g_daily_state.last_displayed_total_ml = 0;
    g_daily_state.last_reset_timestamp = current_time;
    g_daily_state.last_recorded_adc = 0;  // Reset baseline ADC

    storageSaveDailyState(g_daily_state);
    Serial.println("Daily counter reset complete");
}

// Initialize drink tracking system
void drinksInit() {
    if (!g_time_valid) {
        Serial.println("WARNING: Cannot initialize drinks - time not set");
        g_drinks_initialized = false;
        return;
    }

    // Load daily state from NVS
    if (!storageLoadDailyState(g_daily_state)) {
        // First run - initialize state
        Serial.println("Initializing new daily state");
        memset(&g_daily_state, 0, sizeof(DailyState));
        g_daily_state.last_reset_timestamp = getCurrentUnixTime();
        storageSaveDailyState(g_daily_state);
    } else {
        Serial.println("Loaded daily state from NVS:");
        Serial.printf("  Daily total: %dml\n", g_daily_state.daily_total_ml);
        Serial.printf("  Drink count: %d\n", g_daily_state.drink_count_today);
        Serial.printf("  Last drink: %u\n", g_daily_state.last_drink_timestamp);
    }

    g_drinks_initialized = true;
}

// Main drink detection and tracking update
void drinksUpdate(int32_t current_adc, const CalibrationData& cal) {
    if (!g_drinks_initialized || !g_time_valid) {
        return;
    }

    uint32_t current_time = getCurrentUnixTime();

    // Check for daily reset FIRST
    if (shouldResetDailyCounter(current_time)) {
        performAtomicDailyReset(current_time);
        // Early return to establish fresh baseline on next call
        return;
    }

    // Convert current ADC to ml using calibration
    float current_ml = calibrationGetWaterWeight(current_adc, cal);

    // Initialize baseline on first call (or after reset)
    if (g_daily_state.last_recorded_adc == 0) {
        Serial.println("Establishing baseline after reset");
        g_daily_state.last_recorded_adc = current_adc;
        storageSaveDailyState(g_daily_state);
        return;  // Wait for next reading before detecting drinks
    }

    // Calculate delta from last recorded baseline
    float baseline_ml = calibrationGetWaterWeight(g_daily_state.last_recorded_adc, cal);
    float delta_ml = baseline_ml - current_ml;  // Positive = water removed (drink)

    // Detect drink event (≥30ml decrease)
    if (delta_ml >= DRINK_MIN_THRESHOLD_ML) {
        // Classify drink type based on volume
        uint8_t drink_type = (delta_ml >= DRINK_GULP_THRESHOLD_ML)
                             ? DRINK_TYPE_POUR
                             : DRINK_TYPE_GULP;
        const char* type_str = (drink_type == DRINK_TYPE_POUR) ? "POUR" : "GULP";

        Serial.printf("\n=== DRINK DETECTED: %.1fml (%s) ===\n", delta_ml, type_str);

        // Create new drink record (no aggregation)
        DrinkRecord record;
        record.timestamp = current_time;
        record.amount_ml = (int16_t)delta_ml;
        record.bottle_level_ml = (uint16_t)current_ml;
        record.flags = 0;
        record.type = drink_type;

        storageSaveDrinkRecord(record);

        // Update daily total
        g_daily_state.daily_total_ml += (uint16_t)delta_ml;
        g_daily_state.drink_count_today++;
        g_daily_state.last_drink_timestamp = current_time;
        g_daily_state.last_recorded_adc = current_adc;

        storageSaveDailyState(g_daily_state);

        // Save timestamp to NVS on drink event (for time persistence)
        saveTimestampOnEvent("drink");

        Serial.printf("Daily total: %dml (%d drinks)\n",
                     g_daily_state.daily_total_ml,
                     g_daily_state.drink_count_today);

        // Check if display should update (≥50ml change)
        uint16_t display_delta = abs((int)g_daily_state.daily_total_ml -
                                     (int)g_daily_state.last_displayed_total_ml);
        if (display_delta >= DRINK_DISPLAY_UPDATE_THRESHOLD_ML) {
            Serial.println("Display update threshold reached - should refresh");
            g_daily_state.last_displayed_total_ml = g_daily_state.daily_total_ml;
            storageSaveDailyState(g_daily_state);
            // TODO: Trigger display refresh
        }
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

        Serial.println("Daily total unchanged (refill)");
    }
    // No significant change - update baseline for drift compensation
    else {
        g_daily_state.last_recorded_adc = current_adc;
    }
}

// Debug function: Get current daily state
void drinksGetState(DailyState& state) {
    state = g_daily_state;
}

// Debug function: Reset daily drinks (for testing)
void drinksResetDaily() {
    Serial.println("=== MANUAL DAILY RESET ===");
    performAtomicDailyReset(getCurrentUnixTime());
    Serial.println("Daily drinks reset to 0ml");
}

// Debug function: Clear all drink records (for testing)
void drinksClearAll() {
    Serial.println("=== CLEARING ALL DRINK RECORDS ===");

    // Reset daily state
    memset(&g_daily_state, 0, sizeof(DailyState));
    g_daily_state.last_reset_timestamp = getCurrentUnixTime();
    storageSaveDailyState(g_daily_state);

    // Reset circular buffer metadata
    CircularBufferMetadata meta;
    meta.write_index = 0;
    meta.record_count = 0;
    meta.total_writes = 0;
    meta._reserved = 0;
    storageSaveBufferMetadata(meta);

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

    Serial.printf("Drinks: Saved to RTC - baseline ADC=%d (%.0fml)\n",
                  rtc_last_stable_adc, rtc_last_stable_water_ml);
}

// Restore drink detection baseline from RTC memory after waking from deep sleep
// Returns true if valid state was restored, false if power cycle (magic invalid)
bool drinksRestoreFromRTC() {
    if (rtc_drinks_magic != RTC_MAGIC_DRINKS) {
        Serial.println("Drinks: No valid RTC state (power cycle)");
        return false;
    }

    // Restore baseline - this prevents false drink detection on wake
    g_daily_state.last_recorded_adc = rtc_last_stable_adc;

    Serial.printf("Drinks: Restored from RTC - baseline ADC=%d (%.0fml)\n",
                  rtc_last_stable_adc, rtc_last_stable_water_ml);

    return true;
}
