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

// Static state for drink detection
static DailyState g_daily_state;
static bool g_drinks_initialized = false;

// Helper: Get current Unix timestamp with timezone offset
uint32_t getCurrentUnixTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // Apply timezone offset (convert hours to seconds)
    return tv.tv_sec + (g_timezone_offset * 3600);
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

// Helper: Get state name for debugging
const char* getDrinkTrackingStateName(DrinkTrackingState state) {
    switch (state) {
        case DRINK_UNINITIALIZED:        return "UNINITIALIZED";
        case DRINK_ESTABLISHING_BASELINE: return "ESTABLISHING_BASELINE";
        case DRINK_MONITORING:           return "MONITORING";
        case DRINK_AGGREGATING:          return "AGGREGATING";
        default:                         return "UNKNOWN";
    }
}

// Helper: Atomic daily reset handler (Fixes Bug 3.1, 3.3)
static void handleDailyReset(uint32_t current_time) {
    Serial.println("\n=== ATOMIC DAILY RESET ===");
    Serial.printf("Previous total: %dml (%d drinks)\n",
                 g_daily_state.daily_total_ml,
                 g_daily_state.drink_count_today);

    // Reset all daily counters atomically
    g_daily_state.daily_total_ml = 0;
    g_daily_state.drink_count_today = 0;
    g_daily_state.last_displayed_total_ml = 0;
    g_daily_state.last_reset_timestamp = current_time;

    // FIX Bug 3.2: Clear aggregation window state on daily reset
    g_daily_state.aggregation_window_active = 0;
    g_daily_state.aggregation_window_start = 0;

    // FIX Bug 3.3: Reset baseline ADC so first drink uses fresh baseline
    g_daily_state.last_recorded_adc = 0;

    // Return to ESTABLISHING_BASELINE state
    g_daily_state.state = DRINK_ESTABLISHING_BASELINE;

    storageSaveDailyState(g_daily_state);
    Serial.println("Daily counter reset complete");
    Serial.printf("State transition: -> %s\n", getDrinkTrackingStateName(g_daily_state.state));
}

// Initialize drink tracking system
void drinksInit() {
    if (!g_time_valid) {
        Serial.println("WARNING: Cannot initialize drinks - time not set");
        g_drinks_initialized = false;
        g_daily_state.state = DRINK_UNINITIALIZED;
        return;
    }

    // Load daily state from NVS
    if (!storageLoadDailyState(g_daily_state)) {
        // First run - initialize state
        Serial.println("Initializing new daily state");
        memset(&g_daily_state, 0, sizeof(DailyState));
        g_daily_state.state = DRINK_ESTABLISHING_BASELINE;
        g_daily_state.last_reset_timestamp = getCurrentUnixTime();
        storageSaveDailyState(g_daily_state);
    } else {
        Serial.println("Loaded daily state from NVS:");
        Serial.printf("  State: %s\n", getDrinkTrackingStateName(g_daily_state.state));
        Serial.printf("  Daily total: %dml\n", g_daily_state.daily_total_ml);
        Serial.printf("  Drink count: %d\n", g_daily_state.drink_count_today);
        Serial.printf("  Last drink: %u\n", g_daily_state.last_drink_timestamp);

        // If state is UNINITIALIZED, transition to ESTABLISHING_BASELINE
        if (g_daily_state.state == DRINK_UNINITIALIZED) {
            g_daily_state.state = DRINK_ESTABLISHING_BASELINE;
        }
    }

    g_drinks_initialized = true;
}

// FIX Bug 4.2: Handle time becoming valid
void drinksOnTimeSet() {
    Serial.println("[Drinks FSM] Time became valid - initializing drink tracking");
    drinksInit();
}

// Main drink detection and tracking update - NOW WITH EXPLICIT FSM
void drinksUpdate(int32_t current_adc, const CalibrationData& cal) {
    if (!g_drinks_initialized || !g_time_valid) {
        return;
    }

    uint32_t current_time = getCurrentUnixTime();
    float current_ml = calibrationGetWaterWeight(current_adc, cal);

    // Check for daily reset FIRST (atomically)
    if (shouldResetDailyCounter(current_time)) {
        handleDailyReset(current_time);
        // State is now DRINK_ESTABLISHING_BASELINE, fall through to handle it
    }

    // ========================================================================
    // DRINK TRACKING STATE MACHINE
    // ========================================================================
    switch (g_daily_state.state) {
        case DRINK_UNINITIALIZED:
            // Should not reach here if g_drinks_initialized is true
            Serial.println("[Drinks FSM] ERROR: In UNINITIALIZED state but initialized flag is true");
            break;

        case DRINK_ESTABLISHING_BASELINE:
            // Waiting for first stable reading to establish baseline
            Serial.println("[Drinks FSM] Establishing baseline");
            g_daily_state.last_recorded_adc = current_adc;
            g_daily_state.state = DRINK_MONITORING;
            storageSaveDailyState(g_daily_state);
            Serial.printf("[Drinks FSM] State transition: ESTABLISHING_BASELINE -> MONITORING\n");
            break;

        case DRINK_MONITORING: {
            // Normal monitoring mode - check for drink/refill events
            float baseline_ml = calibrationGetWaterWeight(g_daily_state.last_recorded_adc, cal);
            float delta_ml = baseline_ml - current_ml;  // Positive = water removed (drink)

            // Detect drink event (≥30ml decrease)
            if (delta_ml >= DRINK_MIN_THRESHOLD_ML) {
                Serial.printf("\n=== DRINK DETECTED: %.1fml ===\n", delta_ml);

                // Create new drink record
                DrinkRecord record;
                record.timestamp = current_time;
                record.amount_ml = (int16_t)delta_ml;
                record.bottle_level_ml = (uint16_t)current_ml;
                record.flags = 0;

                storageSaveDrinkRecord(record);

                // Update daily total
                g_daily_state.daily_total_ml += (uint16_t)delta_ml;
                g_daily_state.drink_count_today++;
                g_daily_state.last_drink_timestamp = current_time;
                g_daily_state.last_recorded_adc = current_adc;

                // Start aggregation window and transition to AGGREGATING state
                g_daily_state.aggregation_window_active = 1;
                g_daily_state.aggregation_window_start = current_time;
                g_daily_state.state = DRINK_AGGREGATING;

                storageSaveDailyState(g_daily_state);

                Serial.printf("Daily total: %dml (%d drinks)\n",
                             g_daily_state.daily_total_ml,
                             g_daily_state.drink_count_today);
                Serial.printf("[Drinks FSM] State transition: MONITORING -> AGGREGATING\n");

                // Check if display should update (≥50ml change)
                uint16_t display_delta = abs((int)g_daily_state.daily_total_ml -
                                             (int)g_daily_state.last_displayed_total_ml);
                if (display_delta >= DRINK_DISPLAY_UPDATE_THRESHOLD_ML) {
                    Serial.println("Display update threshold reached");
                    g_daily_state.last_displayed_total_ml = g_daily_state.daily_total_ml;
                    storageSaveDailyState(g_daily_state);
                }
            }
            // Detect refill event (≥100ml increase)
            else if (delta_ml <= -DRINK_REFILL_THRESHOLD_ML) {
                Serial.printf("\n=== REFILL DETECTED: %.1fml ===\n", -delta_ml);

                DrinkRecord record;
                record.timestamp = current_time;
                record.amount_ml = (int16_t)delta_ml;  // Negative for refill
                record.bottle_level_ml = (uint16_t)current_ml;
                record.flags = 0;

                storageSaveDrinkRecord(record);

                // Update baseline
                g_daily_state.last_recorded_adc = current_adc;
                storageSaveDailyState(g_daily_state);

                Serial.println("Daily total unchanged (refill)");
            }
            // No significant change - update baseline for drift compensation
            else {
                g_daily_state.last_recorded_adc = current_adc;
            }
            break;
        }

        case DRINK_AGGREGATING: {
            // In aggregation window - aggregate drinks or close window
            uint32_t window_elapsed = current_time - g_daily_state.aggregation_window_start;

            // Check if window expired
            if (window_elapsed >= DRINK_AGGREGATION_WINDOW_SEC) {
                Serial.println("[Drinks FSM] Aggregation window closed (5 min elapsed)");
                g_daily_state.aggregation_window_active = 0;
                g_daily_state.state = DRINK_MONITORING;
                storageSaveDailyState(g_daily_state);
                Serial.printf("[Drinks FSM] State transition: AGGREGATING -> MONITORING\n");
                break;
            }

            // Check for additional drinks within window
            float baseline_ml = calibrationGetWaterWeight(g_daily_state.last_recorded_adc, cal);
            float delta_ml = baseline_ml - current_ml;  // Positive = water removed (drink)

            if (delta_ml >= DRINK_MIN_THRESHOLD_ML) {
                Serial.printf("\n=== DRINK IN WINDOW: %.1fml ===\n", delta_ml);
                Serial.printf("Aggregation window: %us elapsed\n", window_elapsed);

                // Aggregate with last drink record
                DrinkRecord last_record;
                if (storageLoadLastDrinkRecord(last_record)) {
                    last_record.amount_ml += (int16_t)delta_ml;
                    last_record.bottle_level_ml = (uint16_t)current_ml;
                    last_record.timestamp = current_time;

                    if (storageUpdateLastDrinkRecord(last_record)) {
                        Serial.printf("Aggregated drink: %dml total\n", last_record.amount_ml);
                    }
                }

                // Update daily total
                g_daily_state.daily_total_ml += (uint16_t)delta_ml;
                g_daily_state.drink_count_today++;
                g_daily_state.last_drink_timestamp = current_time;
                g_daily_state.last_recorded_adc = current_adc;

                storageSaveDailyState(g_daily_state);

                Serial.printf("Daily total: %dml (%d drinks)\n",
                             g_daily_state.daily_total_ml,
                             g_daily_state.drink_count_today);

                // Check if display should update
                uint16_t display_delta = abs((int)g_daily_state.daily_total_ml -
                                             (int)g_daily_state.last_displayed_total_ml);
                if (display_delta >= DRINK_DISPLAY_UPDATE_THRESHOLD_ML) {
                    Serial.println("Display update threshold reached");
                    g_daily_state.last_displayed_total_ml = g_daily_state.daily_total_ml;
                    storageSaveDailyState(g_daily_state);
                }
            }
            // Refill closes aggregation window immediately
            else if (delta_ml <= -DRINK_REFILL_THRESHOLD_ML) {
                Serial.printf("\n=== REFILL DETECTED: %.1fml ===\n", -delta_ml);

                DrinkRecord record;
                record.timestamp = current_time;
                record.amount_ml = (int16_t)delta_ml;  // Negative for refill
                record.bottle_level_ml = (uint16_t)current_ml;
                record.flags = 0;

                storageSaveDrinkRecord(record);

                // Close aggregation window and return to MONITORING
                g_daily_state.aggregation_window_active = 0;
                g_daily_state.last_recorded_adc = current_adc;
                g_daily_state.state = DRINK_MONITORING;

                storageSaveDailyState(g_daily_state);

                Serial.println("Daily total unchanged (refill)");
                Serial.printf("[Drinks FSM] State transition: AGGREGATING -> MONITORING (refill)\n");
            }
            break;
        }
    }
}

// Debug function: Get current daily state
void drinksGetState(DailyState& state) {
    state = g_daily_state;
}

// Debug function: Reset daily drinks (for testing)
void drinksResetDaily() {
    Serial.println("=== MANUAL DAILY RESET ===");
    g_daily_state.daily_total_ml = 0;
    g_daily_state.drink_count_today = 0;
    g_daily_state.last_displayed_total_ml = 0;
    g_daily_state.last_reset_timestamp = getCurrentUnixTime();
    g_daily_state.aggregation_window_active = 0;
    storageSaveDailyState(g_daily_state);
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
