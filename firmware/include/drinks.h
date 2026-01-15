// drinks.h - Daily water intake tracking data structures and API
// Part of the Aquavate smart water bottle firmware

#ifndef DRINKS_H
#define DRINKS_H

#include <Arduino.h>
#include "storage.h"

// DrinkRecord structure: Stores individual drink/refill events (10 bytes)
struct DrinkRecord {
    uint32_t timestamp;         // Unix timestamp (seconds since epoch)
    int16_t  amount_ml;         // Consumed (+) or refilled (-) amount in ml
    uint16_t bottle_level_ml;   // Bottle water level after event (0-830ml)
    uint8_t  flags;             // Bit flags: 0x01=synced to app, 0x02=day_boundary
    uint8_t  type;              // Drink type: 0=gulp (<100ml), 1=pour (≥100ml)
};

// DailyState structure: Tracks current day's drinking state (18 bytes)
struct DailyState {
    uint32_t last_reset_timestamp;      // Last 4am daily reset (Unix time)
    uint32_t last_drink_timestamp;      // Most recent drink event (Unix time)
    int32_t  last_recorded_adc;         // Baseline ADC value for drink detection
    uint16_t daily_total_ml;            // Today's cumulative water intake (ml)
    uint16_t drink_count_today;         // Number of drink events today
    uint16_t last_displayed_total_ml;   // Last displayed total (for hysteresis)
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
 */
void drinksUpdate(int32_t current_adc, const CalibrationData& cal);

/**
 * Get current Unix timestamp (RTC + timezone offset)
 *
 * @return Unix timestamp in seconds (local time)
 */
uint32_t getCurrentUnixTime();

/**
 * Get current daily state (for debugging/display)
 *
 * @param state Output parameter for current state
 */
void drinksGetState(DailyState& state);

/**
 * Set daily intake to a specific value (for testing/manual adjustment)
 * Updates internal state and saves to NVS
 *
 * @param ml Daily intake value in milliliters (0-10000)
 * @return true if successful, false if value out of range
 */
bool drinksSetDailyIntake(uint16_t ml);

/**
 * Reset daily intake counter (for testing)
 * Clears daily total and drink count, keeps drink records
 */
void drinksResetDaily();

/**
 * Clear all drink records and reset state (for testing)
 * WARNING: This erases all stored drink data
 */
void drinksClearAll();

/**
 * RTC memory persistence for deep sleep
 * Save/restore drink detection baseline to prevent false triggers on wake
 */
void drinksSaveToRTC();
bool drinksRestoreFromRTC();

#endif // DRINKS_H
