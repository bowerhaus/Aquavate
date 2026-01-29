/**
 * Aquavate - Display State Tracking Module
 * Centralized display logic with smart update detection
 */

#include "display.h"
#include "config.h"
#include "drinks.h"
#include "calibration.h"
#include "aquavate.h"
#include <sys/time.h>
#include <time.h>

// External dependencies from main.cpp
extern Adafruit_NAU7802 nau;
extern bool nauReady;
extern CalibrationData g_calibration;
extern bool g_calibrated;
extern int8_t g_timezone_offset;
extern bool g_time_valid;
extern uint8_t g_daily_intake_display_mode;

// External battery functions (defined in main.cpp for Adafruit Feather)
#if defined(BOARD_ADAFRUIT_FEATHER)
extern float getBatteryVoltage();
extern int getBatteryPercent(float voltage);
#endif

// Display state (internal to this module)
static DisplayState g_display_state;
static ThinkInk_213_Mono_GDEY0213B74* g_display_ptr = nullptr;

// Daily goal for hydration graphic (synced from BLE config, persisted to NVS)
static uint16_t g_daily_goal_ml = DRINK_DAILY_GOAL_DEFAULT_ML;
static bool g_daily_goal_changed = false;  // Flag to trigger display update when goal changes

// RTC memory for display state persistence across deep sleep
// RTC_DATA_ATTR keeps data in RTC slow memory (survives deep sleep, lost on power cycle)
#define RTC_MAGIC_DISPLAY 0x41515541  // "AQUA" in hex
RTC_DATA_ATTR uint32_t rtc_display_magic = 0;
RTC_DATA_ATTR float rtc_display_water_ml = 0.0f;
RTC_DATA_ATTR uint16_t rtc_display_daily_ml = 0;
RTC_DATA_ATTR uint8_t rtc_display_hour = 0;
RTC_DATA_ATTR uint8_t rtc_display_minute = 0;
RTC_DATA_ATTR uint8_t rtc_display_battery = 0;
RTC_DATA_ATTR uint32_t rtc_wake_count = 0;
RTC_DATA_ATTR uint16_t rtc_display_daily_goal = 0;

// Bitmap data (moved from main.cpp)
// Water drop icon bitmap (60x60 pixels)
const unsigned char water_drop_bitmap[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0xFF, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0xFF, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0xC0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0xC0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFF, 0xE0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFF, 0xE0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1F, 0xFF, 0xF0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1F, 0xFF, 0xF0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0xFF, 0xF8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xFC, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xFC, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xFC, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0xFF, 0xF8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1F, 0xFF, 0xF0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFF, 0xE0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0xC0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00
};

#define WATER_DROP_WIDTH 60
#define WATER_DROP_HEIGHT 60

// Human figure OUTLINE bitmap (54x90 pixels) - generated from human_figure_outline_54x90.png
const unsigned char PROGMEM human_figure_bitmap[] = {
    0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0x03, 0xF0, 0x00, 0x00,
    0x00, 0x00, 0x7C, 0x00, 0xF0, 0x00, 0x00,
    0x00, 0x00, 0x78, 0x00, 0x78, 0x00, 0x00,
    0x00, 0x00, 0xF0, 0x00, 0x3C, 0x00, 0x00,
    0x00, 0x01, 0xE0, 0x00, 0x1C, 0x00, 0x00,
    0x00, 0x01, 0xC0, 0x00, 0x0E, 0x00, 0x00,
    0x00, 0x01, 0xC0, 0x00, 0x0E, 0x00, 0x00,
    0x00, 0x03, 0x80, 0x00, 0x0E, 0x00, 0x00,
    0x00, 0x03, 0x80, 0x00, 0x07, 0x00, 0x00,
    0x00, 0x03, 0x80, 0x00, 0x07, 0x00, 0x00,
    0x00, 0x03, 0x80, 0x00, 0x07, 0x00, 0x00,
    0x00, 0x03, 0x80, 0x00, 0x07, 0x00, 0x00,
    0x00, 0x03, 0xC0, 0x00, 0x0E, 0x00, 0x00,
    0x00, 0x01, 0xC0, 0x00, 0x0E, 0x00, 0x00,
    0x00, 0x01, 0xC0, 0x00, 0x0E, 0x00, 0x00,
    0x00, 0x01, 0xE0, 0x00, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0xF0, 0x00, 0x3C, 0x00, 0x00,
    0x00, 0x00, 0x78, 0x00, 0x78, 0x00, 0x00,
    0x00, 0x00, 0x7C, 0x00, 0xF0, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0x07, 0xE0, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00,
    0x00, 0x3F, 0xFC, 0x00, 0x7F, 0xF0, 0x00,
    0x00, 0x7C, 0x00, 0x00, 0x00, 0xF8, 0x00,
    0x00, 0xF0, 0x00, 0x00, 0x00, 0x3C, 0x00,
    0x01, 0xE0, 0x00, 0x00, 0x00, 0x1E, 0x00,
    0x01, 0xC0, 0x00, 0x00, 0x00, 0x0E, 0x00,
    0x03, 0xC0, 0x00, 0x00, 0x00, 0x0F, 0x00,
    0x03, 0x80, 0x00, 0x00, 0x00, 0x07, 0x00,
    0x03, 0x80, 0x00, 0x00, 0x00, 0x07, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x00, 0x07, 0x80,
    0x07, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80,
    0x07, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80,
    0x0F, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x01, 0xC0,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x01, 0xC0,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x01, 0xC0,
    0x1C, 0x00, 0x00, 0x00, 0x00, 0x01, 0xC0,
    0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0,
    0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0,
    0x1C, 0x02, 0x00, 0x00, 0x00, 0x00, 0xE0,
    0x38, 0x07, 0x00, 0x00, 0x03, 0x80, 0x70,
    0x38, 0x07, 0x00, 0x00, 0x03, 0x80, 0x70,
    0x38, 0x0F, 0x00, 0x00, 0x03, 0xC0, 0x70,
    0x70, 0x0F, 0x00, 0x00, 0x03, 0xC0, 0x70,
    0x70, 0x1F, 0x00, 0x00, 0x03, 0xE0, 0x38,
    0x70, 0x1F, 0x00, 0x00, 0x03, 0xE0, 0x38,
    0x70, 0x3F, 0x00, 0x00, 0x03, 0xE0, 0x38,
    0xE0, 0x3F, 0x00, 0x00, 0x03, 0xF0, 0x38,
    0xE0, 0x3F, 0x00, 0x00, 0x03, 0xF0, 0x1C,
    0xE0, 0x7F, 0x00, 0x00, 0x03, 0xF8, 0x1C,
    0xF0, 0x77, 0x00, 0x00, 0x03, 0xB8, 0x38,
    0x78, 0xF7, 0x00, 0x00, 0x03, 0xBC, 0x78,
    0x7F, 0xE7, 0x00, 0x00, 0x03, 0x9F, 0xF0,
    0x3F, 0xC7, 0x00, 0x00, 0x03, 0x8F, 0xE0,
    0x0F, 0x07, 0x00, 0x00, 0x03, 0x87, 0xC0,
    0x00, 0x07, 0x00, 0x00, 0x03, 0x80, 0x00,
    0x00, 0x07, 0x00, 0x00, 0x03, 0x80, 0x00,
    0x00, 0x07, 0x00, 0x00, 0x03, 0x80, 0x00,
    0x00, 0x07, 0x00, 0x00, 0x03, 0x80, 0x00,
    0x00, 0x07, 0x00, 0x00, 0x03, 0x80, 0x00,
    0x00, 0x07, 0x00, 0x30, 0x03, 0x80, 0x00,
    0x00, 0x07, 0x00, 0x78, 0x03, 0x80, 0x00,
    0x00, 0x07, 0x00, 0x78, 0x03, 0x80, 0x00,
    0x00, 0x06, 0x00, 0x78, 0x03, 0x80, 0x00,
    0x00, 0x0E, 0x00, 0x78, 0x03, 0x80, 0x00,
    0x00, 0x0E, 0x00, 0xF8, 0x03, 0x80, 0x00,
    0x00, 0x0E, 0x00, 0xFC, 0x03, 0x80, 0x00,
    0x00, 0x0E, 0x00, 0xFC, 0x01, 0x80, 0x00,
    0x00, 0x0E, 0x00, 0xFC, 0x01, 0x80, 0x00,
    0x00, 0x0E, 0x00, 0xFC, 0x01, 0xC0, 0x00,
    0x00, 0x0E, 0x01, 0xCE, 0x01, 0xC0, 0x00,
    0x00, 0x0E, 0x01, 0xCE, 0x01, 0xC0, 0x00,
    0x00, 0x0E, 0x01, 0xCE, 0x01, 0xC0, 0x00,
    0x00, 0x0E, 0x01, 0xCE, 0x01, 0xC0, 0x00,
    0x00, 0x0E, 0x01, 0xCE, 0x01, 0xC0, 0x00,
    0x00, 0x0E, 0x03, 0x87, 0x01, 0xC0, 0x00,
    0x00, 0x0E, 0x03, 0x87, 0x03, 0x80, 0x00,
    0x00, 0x07, 0x07, 0x87, 0x03, 0x80, 0x00,
    0x00, 0x07, 0x8F, 0x03, 0xCF, 0x80, 0x00,
    0x00, 0x03, 0xFF, 0x03, 0xFF, 0x00, 0x00,
    0x00, 0x01, 0xFE, 0x01, 0xFE, 0x00, 0x00,
    0x00, 0x00, 0x70, 0x00, 0x78, 0x00, 0x00,
};

#define HUMAN_FIGURE_WIDTH 54
#define HUMAN_FIGURE_HEIGHT 90

// Human figure FILLED bitmap (54x90 pixels) - generated from human_figure_filled_54x90.png
const unsigned char PROGMEM human_figure_filled_bitmap[] = {
    0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xF8, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFC, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFC, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFE, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFE, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFE, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFE, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFE, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFE, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFC, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFC, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xF8, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00,
    0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00,
    0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xF8, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x00,
    0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x00,
    0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x00,
    0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x80,
    0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x80,
    0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x80,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x80,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0,
    0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0,
    0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0,
    0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0,
    0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0,
    0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
    0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
    0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8,
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8,
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xF7, 0xFF, 0xFF, 0xFF, 0xBF, 0xF8,
    0x7F, 0xF7, 0xFF, 0xFF, 0xFF, 0xBF, 0xF8,
    0x7F, 0xE7, 0xFF, 0xFF, 0xFF, 0x9F, 0xF0,
    0x3F, 0xC7, 0xFF, 0xFF, 0xFF, 0x8F, 0xE0,
    0x0F, 0x07, 0xFF, 0xFF, 0xFF, 0x87, 0xC0,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xC0, 0x00,
    0x00, 0x0F, 0xFF, 0xCF, 0xFF, 0xC0, 0x00,
    0x00, 0x0F, 0xFF, 0xCF, 0xFF, 0xC0, 0x00,
    0x00, 0x0F, 0xFF, 0xCF, 0xFF, 0xC0, 0x00,
    0x00, 0x0F, 0xFF, 0xCF, 0xFF, 0xC0, 0x00,
    0x00, 0x0F, 0xFF, 0xCF, 0xFF, 0xC0, 0x00,
    0x00, 0x0F, 0xFF, 0x87, 0xFF, 0xC0, 0x00,
    0x00, 0x0F, 0xFF, 0x87, 0xFF, 0x80, 0x00,
    0x00, 0x07, 0xFF, 0x87, 0xFF, 0x80, 0x00,
    0x00, 0x07, 0xFF, 0x03, 0xFF, 0x80, 0x00,
    0x00, 0x03, 0xFF, 0x03, 0xFF, 0x00, 0x00,
    0x00, 0x01, 0xFE, 0x01, 0xFE, 0x00, 0x00,
    0x00, 0x00, 0x70, 0x00, 0x78, 0x00, 0x00,
};

// Helper: Quantize battery percentage to 20% steps
static uint8_t quantizeBatteryPercent(int raw_percent) {
    if (raw_percent >= 90) return 100;
    if (raw_percent >= 70) return 80;
    if (raw_percent >= 50) return 60;
    if (raw_percent >= 30) return 40;
    if (raw_percent >= 10) return 20;
    return 0;
}

// Helper: Get day name from weekday number (0=Sunday, 1=Monday, etc.)
static const char* getDayName(int weekday) {
    switch (weekday) {
        case 0: return "Sun";
        case 1: return "Mon";
        case 2: return "Tue";
        case 3: return "Wed";
        case 4: return "Thu";
        case 5: return "Fri";
        case 6: return "Sat";
        default: return "---";
    }
}

// Helper: Format time for display as "Wed 2pm" (day + 12-hour format)
static void formatTimeForDisplay(char* buffer, size_t buffer_size) {
    if (!g_time_valid) {
        snprintf(buffer, buffer_size, "--- --");
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec + (g_timezone_offset * 3600);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    const char* day = getDayName(timeinfo.tm_wday);
    int hour_12 = timeinfo.tm_hour % 12;
    if (hour_12 == 0) hour_12 = 12;
    const char* am_pm = (timeinfo.tm_hour < 12) ? "am" : "pm";

    snprintf(buffer, buffer_size, "%s %d%s", day, hour_12, am_pm);
}

#if defined(BOARD_ADAFRUIT_FEATHER)
// Helper: Draw battery icon
static void drawBatteryIcon(int x, int y, int percent) {
    g_display_ptr->drawRect(x, y, 20, 12, EPD_BLACK);
    g_display_ptr->fillRect(x + 20, y + 3, 3, 6, EPD_BLACK);
    int fillWidth = (percent * 16) / 100;
    if (fillWidth > 0) {
        g_display_ptr->fillRect(x + 2, y + 2, fillWidth, 8, EPD_BLACK);
    }
}

// Note: Zzzz sleep indicator removed - backpack mode screen replaces it (Issue #38)

// Helper: Draw bottle graphic (exported for calibration UI)
void drawBottleGraphic(int16_t x, int16_t y, float fill_percent, bool show_question_mark) {
    int bottle_width = 40;
    int bottle_height = 90;
    int bottle_body_height = 70;
    int neck_height = 10;
    int cap_height = 10;

    int fill_height = (int)(bottle_body_height * fill_percent);

    g_display_ptr->fillRoundRect(x, y + cap_height + neck_height,
                                 bottle_width, bottle_body_height, 8, EPD_BLACK);
    g_display_ptr->fillRoundRect(x + 2, y + cap_height + neck_height + 2,
                                 bottle_width - 4, bottle_body_height - 4, 6, EPD_WHITE);

    int neck_width = bottle_width - 12;
    int neck_x = x + 6;
    g_display_ptr->fillRect(neck_x, y + cap_height, neck_width, neck_height, EPD_BLACK);
    g_display_ptr->fillRect(neck_x + 2, y + cap_height + 2, neck_width - 4, neck_height - 4, EPD_WHITE);

    int cap_width = neck_width - 4;
    int cap_x = neck_x + 2;
    g_display_ptr->fillRect(cap_x, y, cap_width, cap_height, EPD_BLACK);

    // Draw water fill if needed
    if (fill_height > 0) {
        int water_y = y + cap_height + neck_height + bottle_body_height - fill_height;
        g_display_ptr->fillRoundRect(x + 4, water_y,
                                     bottle_width - 8, fill_height - 2, 4, EPD_BLACK);
    }

    // Draw question mark on top if requested
    if (show_question_mark) {
        g_display_ptr->setTextSize(3);
        // Use white text for filled bottles (> 50%), black for empty
        g_display_ptr->setTextColor(fill_percent > 0.5f ? EPD_WHITE : EPD_BLACK);
        int question_x = x + (bottle_width / 2) - 6;  // Center the "?" (shifted right 3px)
        int question_y = y + cap_height + neck_height + (bottle_body_height / 2) - 12;
        g_display_ptr->setCursor(question_x, question_y);
        g_display_ptr->print("?");
    }
}

// Helper: Draw human figure with progressive fill
static void drawHumanFigure(int x, int y, float fill_percent, bool goal_reached) {
    (void)goal_reached;  // Reserved for future use (e.g., smiley face when goal reached)
    int fill_start_row = (int)(HUMAN_FIGURE_HEIGHT * (1.0f - fill_percent));
    int bytes_per_row = (HUMAN_FIGURE_WIDTH + 7) / 8;

    for (int row = 0; row < HUMAN_FIGURE_HEIGHT; row++) {
        int byte_offset = row * bytes_per_row;

        // Select bitmap: filled for rows at or below fill level, outline for rows above
        const unsigned char* bitmap_to_use = (row >= fill_start_row)
            ? human_figure_filled_bitmap
            : human_figure_bitmap;

        for (int col = 0; col < HUMAN_FIGURE_WIDTH; col++) {
            int byte_index = byte_offset + (col / 8);
            int bit_index = 7 - (col % 8);
            uint8_t byte_val = pgm_read_byte(&bitmap_to_use[byte_index]);
            bool pixel_set = (byte_val & (1 << bit_index)) != 0;

            if (pixel_set) {
                g_display_ptr->drawPixel(x + col, y + row, EPD_BLACK);
            }
        }
    }
}

// Helper: Draw tumbler grid
static void drawGlassGrid(int x, int y, float fill_percent) {
    const int GLASS_WIDTH = 18;
    const int GLASS_HEIGHT = 16;
    const int GLASS_SPACING_X = 4;
    const int GLASS_SPACING_Y = 2;
    const int COLS = 2;
    const int ROWS = 5;
    const int TOTAL_GLASSES = COLS * ROWS;

    float total_fill = fill_percent * TOTAL_GLASSES;
    if (total_fill > TOTAL_GLASSES) total_fill = TOTAL_GLASSES;

    int glass_index = 0;

    for (int row = ROWS - 1; row >= 0; row--) {
        for (int col = 0; col < COLS; col++) {
            int glass_x = x + col * (GLASS_WIDTH + GLASS_SPACING_X);
            int glass_y = y + row * (GLASS_HEIGHT + GLASS_SPACING_Y);

            float glass_fill = 0.0f;
            if (total_fill >= glass_index + 1) {
                glass_fill = 1.0f;
            } else if (total_fill > glass_index) {
                glass_fill = total_fill - glass_index;
            }

            g_display_ptr->drawLine(glass_x, glass_y, glass_x + 3, glass_y + GLASS_HEIGHT - 1, EPD_BLACK);
            g_display_ptr->drawLine(glass_x + GLASS_WIDTH - 1, glass_y, glass_x + GLASS_WIDTH - 4, glass_y + GLASS_HEIGHT - 1, EPD_BLACK);
            g_display_ptr->drawLine(glass_x, glass_y, glass_x + GLASS_WIDTH - 1, glass_y, EPD_BLACK);
            g_display_ptr->drawLine(glass_x + 3, glass_y + GLASS_HEIGHT - 1, glass_x + GLASS_WIDTH - 4, glass_y + GLASS_HEIGHT - 1, EPD_BLACK);

            if (glass_fill > 0.0f) {
                int fill_height = (int)((GLASS_HEIGHT - 2) * glass_fill);
                int fill_start_row = GLASS_HEIGHT - 1 - fill_height;

                for (int i = fill_start_row; i < GLASS_HEIGHT - 1; i++) {
                    float ratio = (float)i / (float)(GLASS_HEIGHT - 1);
                    int top_inset = 1;
                    int bottom_inset = 4;
                    int current_inset = top_inset + (int)(ratio * (bottom_inset - top_inset));

                    int line_start = glass_x + current_inset;
                    int line_end = glass_x + GLASS_WIDTH - 1 - current_inset;
                    g_display_ptr->drawLine(line_start, glass_y + i, line_end, glass_y + i, EPD_BLACK);
                }
            }

            glass_index++;
        }
    }
}
#endif

// Public API Implementation

void displayInit(ThinkInk_213_Mono_GDEY0213B74& display_ref) {
    g_display_ptr = &display_ref;
    g_display_state.initialized = false;
    g_display_state.water_ml = 0.0f;
    g_display_state.daily_total_ml = 0;
    g_display_state.hour = 0;
    g_display_state.minute = 0;
    g_display_state.battery_percent = 0;
    g_display_state.last_update_ms = 0;
    g_display_state.last_time_check_ms = 0;
    g_display_state.last_battery_check_ms = 0;

    Serial.println("Display: Initialized state tracking");
}

void displaySetDailyGoal(uint16_t goal_ml) {
    if (goal_ml != g_daily_goal_ml) {
        g_daily_goal_ml = goal_ml;
        g_daily_goal_changed = true;  // Trigger display update
        Serial.printf("Display: Daily goal changed to %dml\n", goal_ml);
    }
}

bool displayCheckGoalChanged() {
    if (g_daily_goal_changed) {
        g_daily_goal_changed = false;
        return true;
    }
    return false;
}

bool displayNeedsUpdate(float current_water_ml,
                       uint16_t current_daily_ml,
                       bool time_interval_elapsed,
                       bool battery_interval_elapsed) {
    bool needs_update = false;

    // Check if water level is valid (ADC stabilized after power-on)
    bool water_valid = (current_water_ml >= -100.0f && current_water_ml <= 1000.0f);

    // If display not initialized and water is invalid, skip ALL updates until ADC stabilizes
    if (!g_display_state.initialized && !water_valid) {
        DEBUG_PRINTF(1, "Display: Waiting for ADC to stabilize (%.1fml invalid)\n", current_water_ml);
        return false;
    }

    // 1. Water level check (5ml threshold)
    if (!g_display_state.initialized) {
        DEBUG_PRINTF(1, "Display: Not initialized - forcing update\n");
        needs_update = true;
    } else if (fabs(current_water_ml - g_display_state.water_ml) >= DISPLAY_UPDATE_THRESHOLD_ML) {
        DEBUG_PRINTF(1, "Display: Water level changed (%.1fml -> %.1fml)\n",
                     g_display_state.water_ml, current_water_ml);
        needs_update = true;
    }

    // 2. Daily intake check (50ml threshold)
    if (abs((int)current_daily_ml - (int)g_display_state.daily_total_ml) >=
        DRINK_DISPLAY_UPDATE_THRESHOLD_ML) {
        DEBUG_PRINTF(1, "Display: Daily intake changed (%dml -> %dml)\n",
                     g_display_state.daily_total_ml, current_daily_ml);
        needs_update = true;
    }

    // 3. Time check (always check if time is valid, update if hour changed or 15+ min elapsed)
    if (g_time_valid) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec + (g_timezone_offset * 3600);
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);

        // Update if: hour changed OR 15+ minutes elapsed since last check
        if (timeinfo.tm_hour != g_display_state.hour ||
            (time_interval_elapsed && abs(timeinfo.tm_min - (int)g_display_state.minute) >= DISPLAY_TIME_UPDATE_THRESHOLD_MIN)) {
            DEBUG_PRINTF(1, "Display: Time changed (%d:%02d -> %d:%02d)\n",
                         g_display_state.hour, g_display_state.minute,
                         timeinfo.tm_hour, timeinfo.tm_min);
            needs_update = true;
        }
    }

    // 4. Battery check (if interval elapsed)
#if defined(BOARD_ADAFRUIT_FEATHER)
    if (battery_interval_elapsed) {
        float voltage = getBatteryVoltage();
        int raw_percent = getBatteryPercent(voltage);
        uint8_t quantized = quantizeBatteryPercent(raw_percent);

        if (abs((int)quantized - (int)g_display_state.battery_percent) >=
            DISPLAY_BATTERY_UPDATE_THRESHOLD) {
            DEBUG_PRINTF(1, "Display: Battery changed (%d%% -> %d%%)\n",
                         g_display_state.battery_percent, quantized);
            needs_update = true;
        }
    }
#endif

    return needs_update;
}

void displayUpdate(float water_ml, uint16_t daily_total_ml,
                   uint8_t hour, uint8_t minute,
                   uint8_t battery_percent, bool sleeping) {
    if (g_display_ptr == nullptr) {
        Serial.println("Display ERROR: Not initialized!");
        return;
    }

    DEBUG_PRINTF(1, "Display: Updating screen (water=%.1fml, daily=%dml, sleeping=%d)\n",
                 water_ml, daily_total_ml, sleeping);

    // Update display state BEFORE rendering
    g_display_state.water_ml = water_ml;
    g_display_state.daily_total_ml = daily_total_ml;
    g_display_state.hour = hour;
    g_display_state.minute = minute;
    g_display_state.battery_percent = battery_percent;
    g_display_state.sleeping = sleeping;
    g_display_state.last_update_ms = millis();
    g_display_state.initialized = true;

    // Render the main screen
    drawMainScreen();
}

void displayForceUpdate(float water_ml, uint16_t daily_total_ml,
                       uint8_t hour, uint8_t minute,
                       uint8_t battery_percent, bool sleeping) {
    DEBUG_PRINTF(1, "Display: Force update triggered\n");
    g_display_state.initialized = false;
    displayUpdate(water_ml, daily_total_ml, hour, minute, battery_percent, sleeping);
}

// Get current display state (for sleep mode to reuse last valid values)
DisplayState displayGetState() {
    return g_display_state;
}

// Mark display as initialized without reading sensors
// Used when waking from deep sleep - e-paper retains image, so no update needed
// IMPORTANT: We don't read sensors here because bottle may be tilted/unstable during wake
// The display will update naturally once the bottle is placed upright and values actually change
void displayMarkInitialized() {
    // Mark as initialized - this prevents the "!initialized" check from triggering
    g_display_state.initialized = true;

    // Set timestamps to current time to prevent interval-based updates from triggering immediately
    g_display_state.last_update_ms = millis();
    g_display_state.last_time_check_ms = millis();
    g_display_state.last_battery_check_ms = millis();

    // Keep the existing state values (they're already initialized to defaults in displayInit)
    // This is safe because:
    // 1. The first UPRIGHT_STABLE reading will compare against these defaults
    // 2. If values truly changed (water consumed, time passed), the thresholds will detect it
    // 3. If nothing changed, the thresholds prevent unnecessary updates

    Serial.println("Display: Marked as initialized (wake from sleep - display image preserved)");
}

// Save display state to RTC memory before entering deep sleep
void displaySaveToRTC() {
    rtc_display_water_ml = g_display_state.water_ml;
    rtc_display_daily_ml = g_display_state.daily_total_ml;
    rtc_display_hour = g_display_state.hour;
    rtc_display_minute = g_display_state.minute;
    rtc_display_battery = g_display_state.battery_percent;
    rtc_display_daily_goal = g_daily_goal_ml;
    rtc_display_magic = RTC_MAGIC_DISPLAY;  // Mark as valid
    rtc_wake_count++;  // Increment wake counter

    Serial.printf("Display: Saved to RTC (wake #%lu) - %.0fml, %uml daily, %02u:%02u, %u%% batt\n",
                  rtc_wake_count, rtc_display_water_ml, rtc_display_daily_ml,
                  rtc_display_hour, rtc_display_minute, rtc_display_battery);
}

// Restore display state from RTC memory after waking from deep sleep
// Returns true if valid state was restored, false if power cycle (magic invalid)
bool displayRestoreFromRTC() {
    if (rtc_display_magic != RTC_MAGIC_DISPLAY) {
        Serial.println("Display: No valid RTC state (power cycle) - wake count reset");
        rtc_wake_count = 0;
        return false;
    }

    // Restore display state
    g_display_state.water_ml = rtc_display_water_ml;
    g_display_state.daily_total_ml = rtc_display_daily_ml;
    g_display_state.hour = rtc_display_hour;
    g_display_state.minute = rtc_display_minute;
    g_display_state.battery_percent = rtc_display_battery;
    g_daily_goal_ml = rtc_display_daily_goal;

    Serial.printf("Display: Restored from RTC (wake #%lu) - %.0fml, %uml daily, %02u:%02u, %u%% batt\n",
                  rtc_wake_count, rtc_display_water_ml, rtc_display_daily_ml,
                  rtc_display_hour, rtc_display_minute, rtc_display_battery);

    return true;
}

// Display backpack mode screen with user instructions (Issue #38)
void displayBackpackMode() {
    if (g_display_ptr == nullptr) return;

    g_display_ptr->clearBuffer();
    g_display_ptr->setTextColor(EPD_BLACK);

    // Title: "backpack mode" centered, textSize=3 (larger for visibility)
    g_display_ptr->setTextSize(3);
    const char* title = "backpack mode";
    int title_width = strlen(title) * 18;  // 18px per char at textSize=3
    g_display_ptr->setCursor((250 - title_width) / 2, 15);
    g_display_ptr->print(title);

    // Instructions: multi-line, textSize=2 (larger for visibility), centered
    g_display_ptr->setTextSize(2);
    const char* line1 = "double-tap firmly";
    const char* line2 = "to wake up";

    int w1 = strlen(line1) * 12;  // 12px per char at textSize=2
    int w2 = strlen(line2) * 12;

    g_display_ptr->setCursor((250 - w1) / 2, 52);
    g_display_ptr->print(line1);
    g_display_ptr->setCursor((250 - w2) / 2, 75);
    g_display_ptr->print(line2);

    // Small note at bottom about wake time
    g_display_ptr->setTextSize(1);
    const char* note = "allow five seconds to wake";
    int note_width = strlen(note) * 6;  // 6px per char at textSize=1
    g_display_ptr->setCursor((250 - note_width) / 2, 105);
    g_display_ptr->print(note);

    g_display_ptr->display();
}

// Display immediate feedback when waking from tap (shows "waking" text)
void displayTapWakeFeedback() {
    if (g_display_ptr == nullptr) return;

    // Show "waking" text centered with "please wait" below
    g_display_ptr->clearBuffer();
    g_display_ptr->setTextColor(EPD_BLACK);

    // "waking" in large text
    g_display_ptr->setTextSize(3);
    const char* text = "waking";
    int text_width = strlen(text) * 18;  // 18px per char at textSize=3
    int text_x = (250 - text_width) / 2;
    g_display_ptr->setCursor(text_x, 40);
    g_display_ptr->print(text);

    // "please wait" in smaller text below
    g_display_ptr->setTextSize(1);
    const char* subtext = "please wait";
    int subtext_width = strlen(subtext) * 6;  // 6px per char at textSize=1
    int subtext_x = (250 - subtext_width) / 2;
    g_display_ptr->setCursor(subtext_x, 72);
    g_display_ptr->print(subtext);

    g_display_ptr->display();

    Serial.println("Display: Tap wake feedback shown (waking)");
}

// Display NVS storage warning screen (shown when drink save fails)
void displayNVSWarning() {
    if (g_display_ptr == nullptr) return;

    Serial.println("Display: Showing NVS warning");

    g_display_ptr->clearBuffer();
    g_display_ptr->setTextColor(EPD_BLACK);

    // Warning text centered
    g_display_ptr->setTextSize(3);
    const char* line1 = "storage";
    const char* line2 = "error";
    int w1 = strlen(line1) * 18;  // 18px per char at textSize=3
    int w2 = strlen(line2) * 18;
    g_display_ptr->setCursor((250 - w1) / 2, 35);
    g_display_ptr->print(line1);
    g_display_ptr->setCursor((250 - w2) / 2, 70);
    g_display_ptr->print(line2);

    g_display_ptr->display();  // Full refresh
    delay(3000);               // Show for 3 seconds

    // Redraw main screen
    drawMainScreen();
}

#if defined(BOARD_ADAFRUIT_FEATHER)
void drawMainScreen() {
    if (g_display_ptr == nullptr) return;

    Serial.println("Drawing main screen...");
    g_display_ptr->clearBuffer();
    g_display_ptr->setTextColor(EPD_BLACK);

    // Get current water level
    float water_ml = g_display_state.water_ml;

    // Check if weight is significantly negative (needs tare or cap is off)
    // Using same -50ml threshold as gestures.cpp for UPRIGHT_STABLE detection
    bool show_question_mark = (water_ml < -50.0f);

    // Clamp to valid range for display
    float display_ml = water_ml;
    if (display_ml < 0) display_ml = 0;  // Clamp negative values for fill calculation
    if (display_ml > 830) display_ml = 830;

    // Draw vertical bottle graphic on left side (shifted down 3px)
    int bottle_x = 10;
    int bottle_y = 23;
    float fill_percent = display_ml / 830.0f;
    drawBottleGraphic(bottle_x, bottle_y, fill_percent, show_question_mark);

    // Draw large text showing daily intake (center, shifted down 3px)
    g_display_ptr->setTextSize(3);
    char intake_text[16];
    snprintf(intake_text, sizeof(intake_text), "%dml", g_display_state.daily_total_ml);

    int intake_text_width = strlen(intake_text) * 18;
    int available_width = 185 - 60;
    int intake_x = 60 + (available_width - intake_text_width) / 2;

    g_display_ptr->setCursor(intake_x, 53);
    g_display_ptr->print(intake_text);

    // Draw "today" label below (shifted down 3px)
    g_display_ptr->setTextSize(2);
    int today_width = 5 * 12;
    int today_x = 60 + (available_width - today_width) / 2;
    g_display_ptr->setCursor(today_x, 78);
    g_display_ptr->print("today");

    // Draw battery status in top-right corner
    drawBatteryIcon(220, 5, g_display_state.battery_percent);

    // Draw time centered at top
    char time_text[16];
    formatTimeForDisplay(time_text, sizeof(time_text));
    g_display_ptr->setTextSize(1);

    int text_width = strlen(time_text) * 6;
    int center_x = (250 - text_width) / 2;
    g_display_ptr->setCursor(center_x, 5);
    g_display_ptr->print(time_text);

    // Draw daily intake visualization (if time is valid)
    float daily_fill = 0.0f;
    bool goal_reached = false;
    if (g_time_valid) {
        daily_fill = (float)g_display_state.daily_total_ml / (float)g_daily_goal_ml;
        if (daily_fill > 1.0f) daily_fill = 1.0f;
        goal_reached = (g_display_state.daily_total_ml >= g_daily_goal_ml);
    }

    // Use runtime display mode instead of compile-time constant (shifted down 3px)
    if (g_daily_intake_display_mode == 0) {
        int figure_x = 185;
        int figure_y = 26;  // Human figure shifted down additional 3px (total 6px from original)
        drawHumanFigure(figure_x, figure_y, daily_fill, goal_reached);
    } else {
        int grid_x = 195;
        int grid_y = 23;
        drawGlassGrid(grid_x, grid_y, daily_fill);
    }

    g_display_ptr->display();
}
#endif
