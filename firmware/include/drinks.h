// drinks.h - Daily water intake tracking data structures and API
// Part of the Aquavate smart water bottle firmware

#ifndef DRINKS_H
#define DRINKS_H

#include <Arduino.h>
#include "storage.h"

// DrinkRecord structure: Stores individual drink/refill events (16 bytes)
// Flag bits: 0x01=synced to app, 0x02=day_boundary, 0x04=deleted (soft delete)
struct DrinkRecord {
    uint32_t record_id;         // Unique incrementing ID for bidirectional sync
    uint32_t timestamp;         // Unix timestamp (seconds since epoch)
    int16_t  amount_ml;         // Consumed (+) or refilled (-) amount in ml
    uint16_t bottle_level_ml;   // Bottle water level after event (0-830ml)
    uint8_t  flags;             // Bit flags: 0x01=synced, 0x02=day_boundary, 0x04=deleted
    uint8_t  type;              // Drink type: 0=gulp (<100ml), 1=pour (≥100ml)
    uint8_t  _reserved[2];      // Reserved for future fields (e.g., temperature, confidence)
};

// DailyState structure: Tracks drink detection state (8 bytes)
// Note: daily_total_ml and drink_count are computed dynamically from records
struct DailyState {
    int32_t  last_recorded_adc;         // Baseline ADC value for drink detection
    uint16_t last_displayed_total_ml;   // Last displayed total (for hysteresis)
    uint16_t _reserved;                 // Padding for alignment
};

// Public API functions

/**
 * Initialize drink tracking system
 * Loads DailyState from NVS or creates new state if not found
 * Must be called after time is set (g_time_valid == true)
 */
void drinksInit();

/**
 * Update drink tracking (call every 2 seconds when bottle is stable)
 * Detects drink events, manages aggregation, triggers display updates
 *
 * @param current_adc Current ADC reading from load cell
 * @param cal Calibration data (for ADC to ml conversion)
 * @return true if a drink was recorded, false otherwise
 */
bool drinksUpdate(int32_t current_adc, const CalibrationData& cal);

/**
 * Get current Unix timestamp (RTC + timezone offset)
 *
 * @return Unix timestamp in seconds (local time)
 */
uint32_t getCurrentUnixTime();

/**
 * Calculate seconds until next daily rollover (4am)
 * Used for timer wake scheduling in deep sleep
 *
 * @return Seconds until next 4am rollover, 0 if time invalid
 */
uint32_t getSecondsUntilRollover();

/**
 * Get current daily state (for debugging/display)
 *
 * @param state Output parameter for current state
 */
void drinksGetState(DailyState& state);

/**
 * Reset daily intake (marks today's drink records as deleted)
 * Used for manual reset or testing
 */
void drinksResetDaily();

/**
 * Cancel the most recent drink record (marks it as deleted)
 * Used when bottle is emptied (shake to empty gesture)
 *
 * @return true if a drink was cancelled, false if no drinks to cancel
 */
bool drinksCancelLast();

/**
 * Clear all drink records and reset state (for testing)
 * WARNING: This erases all stored drink data
 */
void drinksClearAll();

/**
 * Get current daily total (computed from drink records using 4am boundary)
 * This is the authoritative source for daily intake
 *
 * @return Today's cumulative water intake in ml
 */
uint16_t drinksGetDailyTotal();

/**
 * Get current drink count (computed from drink records using 4am boundary)
 *
 * @return Number of drink events today
 */
uint16_t drinksGetDrinkCount();

/**
 * Force recalculation of daily totals from drink records
 * Call this after external changes to drink records (e.g., BLE delete)
 */
void drinksRecalculateTotals();

/**
 * RTC memory persistence for deep sleep
 * Save/restore drink detection baseline to prevent false triggers on wake
 */
void drinksSaveToRTC();
bool drinksRestoreFromRTC();

/**
 * Get current baseline water level in ml
 * Used by shake-to-empty to check if bottle was empty when shake triggered
 *
 * @param cal Calibration data (for ADC to ml conversion)
 * @return Current baseline water level in ml
 */
float drinksGetBaselineWaterLevel(const CalibrationData& cal);

/**
 * Reset baseline ADC to current value
 * Called after drink cancellation to prevent re-detection
 *
 * @param adc Current ADC reading to use as new baseline
 */
void drinksResetBaseline(int32_t adc);

#endif // DRINKS_H
