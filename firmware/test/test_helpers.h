#pragma once
// Shared declarations for native unit tests.
// Provides access to globals (normally in main.cpp) and helpers defined in stubs.cpp.
#include <stdint.h>

// Globals from stubs.cpp (normally defined in main.cpp)
extern int8_t  g_timezone_offset;
extern bool    g_time_valid;
extern bool    g_rtc_ds3231_present;
extern bool    g_debug_drink_tracking;

// In-memory storage helpers (stubs.cpp)
void testClearStorage();
void testAddDrinkRecord(uint32_t timestamp, int16_t amount_ml, uint8_t flags = 0);
