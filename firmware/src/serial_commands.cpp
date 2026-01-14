// serial_commands.cpp
// Serial command parser for USB time setting and configuration
// Uses ESP32 internal RTC (gettimeofday/settimeofday)

#include "serial_commands.h"
#include "storage.h"
#include "drinks.h"
#include "storage_drinks.h"
#include "config.h"
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>

// Command buffer for serial input
#define CMD_BUFFER_SIZE 128
static char cmdBuffer[CMD_BUFFER_SIZE];
static uint8_t cmdBufferPos = 0;

// Callback for time set events
static OnTimeSetCallback g_onTimeSetCallback = nullptr;

// External variables (managed in main.cpp)
extern int8_t g_timezone_offset;

// Runtime debug control variables (managed in main.cpp)
extern bool g_debug_enabled;
extern bool g_debug_water_level;
extern bool g_debug_accelerometer;
extern bool g_debug_display;
extern bool g_debug_drink_tracking;
extern bool g_debug_calibration;
extern bool g_debug_ble;

// Initialize serial command handler
void serialCommandsInit() {
    cmdBufferPos = 0;
    cmdBuffer[0] = '\0';
}

// Register callback for time set events
void serialCommandsSetTimeCallback(OnTimeSetCallback callback) {
    g_onTimeSetCallback = callback;
}

// Parse integer from string
// Returns true if successful, false otherwise
static bool parseInt(const char* str, int& value) {
    char* endptr;
    long result = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0') {
        return false;
    }
    value = (int)result;
    return true;
}

// Validate date components
static bool validateDate(int year, int month, int day) {
    if (year < 2026 || year > 2099) {
        Serial.println("ERROR: Year must be 2026-2099");
        return false;
    }
    if (month < 1 || month > 12) {
        Serial.println("ERROR: Month must be 1-12");
        return false;
    }

    // Days in month (non-leap year, good enough for validation)
    const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maxDay = daysInMonth[month - 1];

    // Leap year check
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        maxDay = 29;
    }

    if (day < 1 || day > maxDay) {
        Serial.printf("ERROR: Day must be 1-%d for month %d\n", maxDay, month);
        return false;
    }

    return true;
}

// Validate time components
static bool validateTime(int hour, int minute, int second) {
    if (hour < 0 || hour > 23) {
        Serial.println("ERROR: Hour must be 0-23");
        return false;
    }
    if (minute < 0 || minute > 59) {
        Serial.println("ERROR: Minute must be 0-59");
        return false;
    }
    if (second < 0 || second > 59) {
        Serial.println("ERROR: Second must be 0-59");
        return false;
    }
    return true;
}

// Validate timezone offset
static bool validateTimezone(int offset) {
    if (offset < -12 || offset > 14) {
        Serial.println("ERROR: Timezone must be -12 to +14");
        return false;
    }
    return true;
}

// Get timezone name for common offsets
static const char* getTimezoneName(int offset) {
    switch (offset) {
        case -8: return "PST";
        case -7: return "MST";
        case -6: return "CST";
        case -5: return "EST";
        case 0: return "UTC";
        case 1: return "CET";
        default: return "";
    }
}

// Handle SET_TIME command
// Format: SET_TIME 2026-01-13 14:30:00 -5
static void handleSetTime(char* args) {
    // Parse date and time
    int year, month, day, hour, minute, second;
    int timezone_offset = g_timezone_offset; // Default to current offset

    // Try to parse date, time, and optional timezone
    int parsed = sscanf(args, "%d-%d-%d %d:%d:%d %d",
                       &year, &month, &day, &hour, &minute, &second, &timezone_offset);

    if (parsed < 6) {
        Serial.println("ERROR: Invalid format");
        Serial.println("Usage: SET_TIME YYYY-MM-DD HH:MM:SS [timezone_offset]");
        Serial.println("Example: SET_TIME 2026-01-13 14:30:00 -5");
        return;
    }

    // Validate components
    if (!validateDate(year, month, day)) return;
    if (!validateTime(hour, minute, second)) return;
    if (!validateTimezone(timezone_offset)) return;

    // Create tm structure with the user's LOCAL time
    struct tm timeinfo = {};
    timeinfo.tm_year = year - 1900;  // tm_year is years since 1900
    timeinfo.tm_mon = month - 1;      // tm_mon is 0-11
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    timeinfo.tm_isdst = -1;  // Let mktime determine DST

    // Convert to Unix timestamp
    // mktime() uses system timezone, but we'll adjust for our custom offset
    time_t timestamp = mktime(&timeinfo);
    if (timestamp == -1) {
        Serial.println("ERROR: Failed to convert time");
        return;
    }

    // Adjust for timezone offset (mktime assumes system timezone)
    timestamp -= timezone_offset * 3600;

    // Set ESP32 internal RTC
    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0;

    if (settimeofday(&tv, NULL) != 0) {
        Serial.println("ERROR: Failed to set RTC");
        return;
    }

    // Save timezone offset to NVS
    if (!storageSaveTimezone(timezone_offset)) {
        Serial.println("WARNING: Failed to save timezone to NVS");
    }

    // Save time_valid flag to NVS
    if (!storageSaveTimeValid(true)) {
        Serial.println("WARNING: Failed to save time_valid flag to NVS");
    }

    // Update global timezone offset
    g_timezone_offset = timezone_offset;

    // Format success message with local time
    char timeStr[64];
    const char* tzName = getTimezoneName(timezone_offset);
    if (strlen(tzName) > 0) {
        snprintf(timeStr, sizeof(timeStr), "Time set: %04d-%02d-%02d %02d:%02d:%02d %s (%+d)",
                 year, month, day,
                 hour + timezone_offset, minute, second,
                 tzName, timezone_offset);
    } else {
        snprintf(timeStr, sizeof(timeStr), "Time set: %04d-%02d-%02d %02d:%02d:%02d (UTC%+d)",
                 year, month, day,
                 hour + timezone_offset, minute, second,
                 timezone_offset);
    }
    Serial.println(timeStr);
    Serial.println("Timezone and time_valid flag saved to NVS");

    // FIX Bug #5: Initialize drink tracking when time becomes valid
    extern bool g_time_valid;  // Forward declaration
    g_time_valid = true;
    drinksInit();

    // Call callback if registered
    if (g_onTimeSetCallback != nullptr) {
        g_onTimeSetCallback();
    }
}

// Handle GET_TIME command
static void handleGetTime() {
    // Load time_valid flag
    bool time_valid = storageLoadTimeValid();

    if (!time_valid) {
        Serial.println("WARNING: Time not set!");
        Serial.println("Current RTC: 1970-01-01 00:00:00 (epoch)");
        Serial.println("Use SET_TIME command to set time");
        Serial.println("Example: SET_TIME 2026-01-13 14:30:00 -5");
        return;
    }

    // Get current time from RTC
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec;

    // Convert to local time using timezone offset
    now += g_timezone_offset * 3600;  // Add timezone offset in seconds
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    // Format time string
    char timeStr[128];
    const char* tzName = getTimezoneName(g_timezone_offset);
    if (strlen(tzName) > 0) {
        snprintf(timeStr, sizeof(timeStr), "Current time: %04d-%02d-%02d %02d:%02d:%02d %s (%+d)",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                 tzName, g_timezone_offset);
    } else {
        snprintf(timeStr, sizeof(timeStr), "Current time: %04d-%02d-%02d %02d:%02d:%02d (UTC%+d)",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                 g_timezone_offset);
    }
    Serial.println(timeStr);
    Serial.println("Time valid: Yes");
    Serial.println("RTC drift: ~3-5 minutes/day (resync recommended weekly)");
}

// Handle SET_TIMEZONE command
static void handleSetTimezone(char* args) {
    int offset;
    if (!parseInt(args, offset)) {
        Serial.println("ERROR: Invalid timezone offset");
        Serial.println("Usage: SET_TIMEZONE offset");
        Serial.println("Example: SET_TIMEZONE -8");
        return;
    }

    if (!validateTimezone(offset)) return;

    // Save to NVS
    if (!storageSaveTimezone(offset)) {
        Serial.println("ERROR: Failed to save timezone to NVS");
        return;
    }

    // Update global offset
    g_timezone_offset = offset;

    const char* tzName = getTimezoneName(offset);
    if (strlen(tzName) > 0) {
        Serial.printf("Timezone set: %+d hours (%s)\n", offset, tzName);
    } else {
        Serial.printf("Timezone set: UTC%+d\n", offset);
    }
    Serial.println("Saved to NVS");
}

// Handle GET_DAILY_STATE command - display current daily state
static void handleGetDailyState() {
    DailyState state;
    drinksGetState(state);

    Serial.println("\n=== DAILY STATE ===");
    Serial.printf("Daily total: %dml\n", state.daily_total_ml);
    Serial.printf("Drink count: %d\n", state.drink_count_today);
    Serial.printf("Last reset: %u\n", state.last_reset_timestamp);
    Serial.printf("Last drink: %u\n", state.last_drink_timestamp);
    Serial.printf("Last baseline ADC: %d\n", state.last_recorded_adc);
    Serial.printf("Last displayed: %dml\n", state.last_displayed_total_ml);
    Serial.printf("Aggregation window: %s\n",
                 state.aggregation_window_active ? "ACTIVE" : "closed");
    if (state.aggregation_window_active) {
        Serial.printf("Window started: %u\n", state.aggregation_window_start);
        uint32_t elapsed = getCurrentUnixTime() - state.aggregation_window_start;
        Serial.printf("Window elapsed: %us\n", elapsed);
    }
    Serial.println("==================\n");
}

// Handle GET_LAST_DRINK command - display most recent drink record
static void handleGetLastDrink() {
    DrinkRecord record;
    if (!storageLoadLastDrinkRecord(record)) {
        Serial.println("No drink records found");
        return;
    }

    Serial.println("\n=== LAST DRINK RECORD ===");
    Serial.printf("Timestamp: %u\n", record.timestamp);
    Serial.printf("Amount: %dml\n", record.amount_ml);
    Serial.printf("Bottle level: %dml\n", record.bottle_level_ml);
    Serial.printf("Flags: 0x%02X\n", record.flags);

    // Format timestamp
    time_t t = record.timestamp;
    struct tm tm;
    gmtime_r(&t, &tm);
    Serial.printf("Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);
    Serial.println("=========================\n");
}

// Handle DUMP_DRINKS command - display all drink records
static void handleDumpDrinks() {
    CircularBufferMetadata meta;
    if (!storageLoadBufferMetadata(meta)) {
        Serial.println("No drink records in buffer");
        return;
    }

    Serial.println("\n=== DRINK BUFFER METADATA ===");
    Serial.printf("Write index: %d\n", meta.write_index);
    Serial.printf("Record count: %d\n", meta.record_count);
    Serial.printf("Total writes: %u\n", meta.total_writes);
    Serial.println("=============================\n");

    if (meta.record_count == 0) {
        Serial.println("No drink records stored");
        return;
    }

    Serial.printf("Showing %d most recent drinks:\n\n", meta.record_count);

    // Calculate starting index (oldest record in buffer)
    uint16_t start_index = (meta.record_count < DRINK_MAX_RECORDS)
        ? 0
        : meta.write_index;

    // Display records in chronological order
    for (uint16_t i = 0; i < meta.record_count; i++) {
        uint16_t index = (start_index + i) % DRINK_MAX_RECORDS;

        // Load record directly from NVS
        char key[16];
        snprintf(key, sizeof(key), "drink_%03d", index);

        DrinkRecord record;
        Preferences prefs;
        if (prefs.begin(NVS_NAMESPACE, true)) {
            size_t read_size = prefs.getBytes(key, &record, sizeof(DrinkRecord));
            prefs.end();

            if (read_size == sizeof(DrinkRecord)) {
                time_t t = record.timestamp;
                struct tm tm;
                gmtime_r(&t, &tm);

                Serial.printf("%3d: %04d-%02d-%02d %02d:%02d:%02d | %+5dml | level: %4dml | flags: 0x%02X\n",
                             i + 1,
                             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                             tm.tm_hour, tm.tm_min, tm.tm_sec,
                             record.amount_ml,
                             record.bottle_level_ml,
                             record.flags);
            }
        }
    }
    Serial.println();
}

// Handle RESET_DAILY_DRINKS command - reset daily counter
static void handleResetDailyDrinks() {
    drinksResetDaily();
    Serial.println("OK: Daily drinks reset");
}

// Handle CLEAR_DRINKS command - clear all drink records
static void handleClearDrinks() {
    drinksClearAll();
    Serial.println("OK: All drink records cleared");
}

// Handle debug level change (single character '0'-'4')
static void handleDebugLevel(char level) {
    switch (level) {
        case '0':  // Level 0: All OFF
            g_debug_enabled = false;
            g_debug_water_level = false;
            g_debug_accelerometer = false;
            g_debug_display = false;
            g_debug_drink_tracking = false;
            g_debug_calibration = false;
            g_debug_ble = false;
            Serial.println("Debug Level 0: All debug output OFF");
            break;

        case '1':  // Level 1: Events only (no gesture details)
            g_debug_enabled = true;
            g_debug_water_level = false;
            g_debug_accelerometer = false;
            g_debug_display = true;
            g_debug_drink_tracking = true;
            g_debug_calibration = false;  // Gestures are level 2
            g_debug_ble = false;
            Serial.println("Debug Level 1: Events (drinks, refills, display)");
            break;

        case '2':  // Level 2: + Gestures (uses g_debug_calibration for gesture output)
            g_debug_enabled = true;
            g_debug_water_level = false;
            g_debug_accelerometer = false;
            g_debug_display = true;
            g_debug_drink_tracking = true;
            g_debug_calibration = true;  // Used for gesture detection output
            g_debug_ble = false;
            Serial.println("Debug Level 2: + Gestures (gesture detection, state changes)");
            break;

        case '3':  // Level 3: + Weight readings
            g_debug_enabled = true;
            g_debug_water_level = true;
            g_debug_accelerometer = false;
            g_debug_display = true;
            g_debug_drink_tracking = true;
            g_debug_calibration = true;
            g_debug_ble = false;
            Serial.println("Debug Level 3: + Weight (load cell ADC, water levels)");
            break;

        case '4':  // Level 4: + Accelerometer
            g_debug_enabled = true;
            g_debug_water_level = true;
            g_debug_accelerometer = true;
            g_debug_display = true;
            g_debug_drink_tracking = true;
            g_debug_calibration = true;
            g_debug_ble = false;
            Serial.println("Debug Level 4: + Accelerometer (raw readings)");
            break;

        case '9':  // Level 9: All ON (future-proof)
            g_debug_enabled = true;
            g_debug_water_level = true;
            g_debug_accelerometer = true;
            g_debug_display = true;
            g_debug_drink_tracking = true;
            g_debug_calibration = true;
            g_debug_ble = true;
            Serial.println("Debug Level 9: All debug ON (all categories)");
            break;

        default:
            Serial.println("ERROR: Invalid debug level (use 0-4 or 9)");
            Serial.println("  0 = All OFF");
            Serial.println("  1 = Events (drinks, refills, display)");
            Serial.println("  2 = + Gestures (gesture detection)");
            Serial.println("  3 = + Weight readings (load cell)");
            Serial.println("  4 = + Accelerometer (raw data)");
            Serial.println("  9 = All ON");
            break;
    }
}

// Process a complete command
static void processCommand(char* cmd) {
    // Trim leading whitespace
    while (*cmd == ' ' || *cmd == '\t') cmd++;

    // Check for empty command
    if (*cmd == '\0') return;

    // Check for single-character debug level commands ('0'-'4', '9')
    if (strlen(cmd) == 1 && ((cmd[0] >= '0' && cmd[0] <= '4') || cmd[0] == '9')) {
        handleDebugLevel(cmd[0]);
        return;
    }

    // Find command and arguments
    char* args = strchr(cmd, ' ');
    if (args != nullptr) {
        *args = '\0';  // Null-terminate command
        args++;        // Point to arguments
        // Trim leading whitespace from args
        while (*args == ' ' || *args == '\t') args++;
    } else {
        args = cmd + strlen(cmd);  // Point to empty string
    }

    // Process commands
    if (strcmp(cmd, "SET_TIME") == 0) {
        handleSetTime(args);
    } else if (strcmp(cmd, "GET_TIME") == 0) {
        handleGetTime();
    } else if (strcmp(cmd, "SET_TIMEZONE") == 0) {
        handleSetTimezone(args);
    } else if (strcmp(cmd, "GET_DAILY_STATE") == 0) {
        handleGetDailyState();
    } else if (strcmp(cmd, "GET_LAST_DRINK") == 0) {
        handleGetLastDrink();
    } else if (strcmp(cmd, "DUMP_DRINKS") == 0) {
        handleDumpDrinks();
    } else if (strcmp(cmd, "RESET_DAILY_DRINKS") == 0) {
        handleResetDailyDrinks();
    } else if (strcmp(cmd, "CLEAR_DRINKS") == 0) {
        handleClearDrinks();
    } else {
        Serial.print("ERROR: Unknown command: ");
        Serial.println(cmd);
        Serial.println("\nAvailable commands:");
        Serial.println("Debug Control:");
        Serial.println("  0-4, 9                - Set debug level (single character)");
        Serial.println("                          0=OFF, 1=Events, 2=+Gestures,");
        Serial.println("                          3=+Weight, 4=+Accel, 9=All ON");
        Serial.println("\nTime/Timezone:");
        Serial.println("  SET_TIME YYYY-MM-DD HH:MM:SS [timezone]");
        Serial.println("  GET_TIME");
        Serial.println("  SET_TIMEZONE offset");
        Serial.println("\nDrink Tracking:");
        Serial.println("  GET_DAILY_STATE       - Show current daily state");
        Serial.println("  GET_LAST_DRINK        - Show most recent drink record");
        Serial.println("  DUMP_DRINKS           - Display all drink records");
        Serial.println("  RESET_DAILY_DRINKS    - Reset daily counter to 0");
        Serial.println("  CLEAR_DRINKS          - Clear all drink records (WARNING: erases data)");
    }
}

// Update serial command handler (call in loop())
void serialCommandsUpdate() {
    while (Serial.available() > 0) {
        char c = Serial.read();

        // Handle newline (command complete)
        if (c == '\n' || c == '\r') {
            if (cmdBufferPos > 0) {
                cmdBuffer[cmdBufferPos] = '\0';
                processCommand(cmdBuffer);
                cmdBufferPos = 0;
            }
        }
        // Add character to buffer
        else if (cmdBufferPos < CMD_BUFFER_SIZE - 1) {
            cmdBuffer[cmdBufferPos++] = c;
        }
        // Buffer overflow - reset
        else {
            Serial.println("ERROR: Command too long");
            cmdBufferPos = 0;
        }
    }
}
