#ifndef AQUAVATE_DISPLAY_H
#define AQUAVATE_DISPLAY_H

#include <Adafruit_ThinkInk.h>
#include <Arduino.h>

// Water drop bitmap for welcome screen (exported from display.cpp)
extern const unsigned char water_drop_bitmap[] PROGMEM;

struct DisplayState {
    float water_ml;                    // Current bottle level (0-830ml)
    uint16_t daily_total_ml;           // Today's intake total
    uint8_t hour;                      // Current hour (0-23)
    uint8_t minute;                    // Current minute (0-59)
    uint8_t battery_percent;           // Battery % quantized to 20% steps
    uint32_t last_update_ms;           // Last full display update
    uint32_t last_time_check_ms;       // Last time check
    uint32_t last_battery_check_ms;    // Last battery check
    bool initialized;                  // Has state been initialized?
    bool sleeping;                     // True when showing Zzzz indicator
};

// Public API
void displayInit(ThinkInk_213_Mono_GDEY0213B74& display_ref);
bool displayNeedsUpdate(float current_water_ml,
                       uint16_t current_daily_ml,
                       bool time_interval_elapsed,
                       bool battery_interval_elapsed);
void displayUpdate(float water_ml, uint16_t daily_total_ml,
                   uint8_t hour, uint8_t minute,
                   uint8_t battery_percent, bool sleeping);
void displayForceUpdate(float water_ml, uint16_t daily_total_ml,
                       uint8_t hour, uint8_t minute,
                       uint8_t battery_percent, bool sleeping);
DisplayState displayGetState();
void drawMainScreen();

// Mark display as initialized (used when waking from deep sleep - display image preserved)
void displayMarkInitialized();

// RTC memory persistence for deep sleep
void displaySaveToRTC();
bool displayRestoreFromRTC();

// Draw bottle graphic (exported for calibration UI)
// x, y: top-left position
// fill_percent: 0.0 to 1.0 (water level)
// show_question_mark: true to show "?" above bottle
void drawBottleGraphic(int16_t x, int16_t y, float fill_percent, bool show_question_mark);

#endif
