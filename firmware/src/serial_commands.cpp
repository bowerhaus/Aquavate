// serial_commands.cpp
// Serial command parser for USB time setting and configuration
// Uses ESP32 internal RTC (gettimeofday/settimeofday)

#include "config.h"

#if ENABLE_SERIAL_COMMANDS

#include "serial_commands.h"
#include "storage.h"
#include "drinks.h"
#include "storage_drinks.h"
#include "weight.h"
#include "config.h"
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>
#include <RTClib.h>  // Adafruit RTClib for DS3231

// Command buffer for serial input
#define CMD_BUFFER_SIZE 128
static char cmdBuffer[CMD_BUFFER_SIZE];
static uint8_t cmdBufferPos = 0;

// Callback for time set events
static OnTimeSetCallback g_onTimeSetCallback = nullptr;

// External variables (managed in main.cpp)
extern int8_t g_timezone_offset;
extern bool g_rtc_ds3231_present;
extern RTC_DS3231 rtc;

// Runtime debug control variables (managed in main.cpp)
extern bool g_debug_enabled;
extern bool g_debug_water_level;
extern bool g_debug_accelerometer;
extern bool g_debug_display;
extern bool g_debug_drink_tracking;
extern bool g_debug_calibration;
extern bool g_debug_ble;

// Runtime display mode variable (managed in main.cpp)
extern uint8_t g_daily_intake_display_mode;

// Runtime sleep timeout variable (managed in main.cpp)
extern uint32_t g_sleep_timeout_ms;

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

// Handle SET DATETIME command
// Format: SET DATETIME 2026-01-13 14:30:00 -5
static void handleSetDateTime(char* args) {
    // Parse date and time
    int year, month, day, hour, minute, second;
    int timezone_offset = g_timezone_offset; // Default to current offset

    // Try to parse date, time, and optional timezone
    int parsed = sscanf(args, "%d-%d-%d %d:%d:%d %d",
                       &year, &month, &day, &hour, &minute, &second, &timezone_offset);

    if (parsed < 6) {
        Serial.println("ERROR: Invalid format");
        Serial.println("Usage: SET DATETIME YYYY-MM-DD HH:MM:SS [timezone_offset]");
        Serial.println("Example: SET DATETIME 2026-01-13 14:30:00 -5");
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

    // Also update DS3231 if present
    if (g_rtc_ds3231_present) {
        rtc.adjust(DateTime(timestamp));
        Serial.println("DS3231 RTC updated");
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

    // Save current timestamp to NVS for time persistence
    storageSaveLastBootTime(timestamp);

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

// Handle GET TIME command
static void handleGetTime() {
    // Load time_valid flag
    bool time_valid = storageLoadTimeValid();

    if (!time_valid) {
        Serial.println("WARNING: Time not set!");
        Serial.println("Current RTC: 1970-01-01 00:00:00 (epoch)");
        Serial.println("Use SET DATETIME command to set time");
        Serial.println("Example: SET DATETIME 2026-01-13 14:30:00 -5");
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

    // Show RTC source and accuracy
    if (g_rtc_ds3231_present) {
        Serial.println("RTC source: DS3231 external RTC (\u00b12-3min/year)");
        Serial.println("Battery-backed: Yes (CR1220)");
    } else {
        Serial.println("RTC source: ESP32 internal RTC (\u00b12-10min/day)");
        Serial.println("Resync recommended: Weekly via USB");
    }
}

// Handle SET DATE command
// Format: SET DATE 2026-01-13
static void handleSetDate(char* args) {
    int year, month, day;

    // Parse date
    int parsed = sscanf(args, "%d-%d-%d", &year, &month, &day);

    if (parsed != 3) {
        Serial.println("ERROR: Invalid format");
        Serial.println("Usage: SET DATE YYYY-MM-DD");
        Serial.println("Example: SET DATE 2026-01-13");
        return;
    }

    // Validate date
    if (!validateDate(year, month, day)) return;

    // Get current time from RTC
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec;

    // Convert current time to struct tm with timezone offset
    now += g_timezone_offset * 3600;
    struct tm current_time;
    gmtime_r(&now, &current_time);

    // Update only the date fields, keep existing time
    current_time.tm_year = year - 1900;
    current_time.tm_mon = month - 1;
    current_time.tm_mday = day;

    // Convert back to Unix timestamp
    time_t new_timestamp = mktime(&current_time);
    if (new_timestamp == -1) {
        Serial.println("ERROR: Failed to convert time");
        return;
    }

    // Adjust for timezone offset
    new_timestamp -= g_timezone_offset * 3600;

    // Set ESP32 internal RTC
    tv.tv_sec = new_timestamp;
    tv.tv_usec = 0;

    if (settimeofday(&tv, NULL) != 0) {
        Serial.println("ERROR: Failed to set RTC");
        return;
    }

    // Also update DS3231 if present
    if (g_rtc_ds3231_present) {
        rtc.adjust(DateTime(new_timestamp));
        Serial.println("DS3231 RTC updated");
    }

    // Save current timestamp to NVS for time persistence
    storageSaveLastBootTime(new_timestamp);

    // Save time_valid flag if not already set
    if (!storageLoadTimeValid()) {
        if (!storageSaveTimeValid(true)) {
            Serial.println("WARNING: Failed to save time_valid flag to NVS");
        }

        // Initialize drink tracking if time just became valid
        extern bool g_time_valid;
        g_time_valid = true;
        drinksInit();

        // Call callback if registered
        if (g_onTimeSetCallback != nullptr) {
            g_onTimeSetCallback();
        }
    }

    Serial.printf("Date set: %04d-%02d-%02d (time preserved: %02d:%02d:%02d)\n",
                 year, month, day,
                 current_time.tm_hour, current_time.tm_min, current_time.tm_sec);
}

// Handle SET TIME command
// Format: SET TIME HH[:MM[:SS]]
static void handleSetTime(char* args) {
    int hour, minute = 0, second = 0;

    // Parse time - allow HH, HH:MM, or HH:MM:SS
    int parsed = sscanf(args, "%d:%d:%d", &hour, &minute, &second);

    if (parsed < 1) {
        Serial.println("ERROR: Invalid format");
        Serial.println("Usage: SET TIME HH[:MM[:SS]]");
        Serial.println("Examples:");
        Serial.println("  SET TIME 14          → 14:00:00");
        Serial.println("  SET TIME 14:30       → 14:30:00");
        Serial.println("  SET TIME 14:30:45    → 14:30:45");
        return;
    }

    // Validate time
    if (!validateTime(hour, minute, second)) return;

    // Get current time from RTC
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec;

    // Convert current time to struct tm with timezone offset
    now += g_timezone_offset * 3600;
    struct tm current_time;
    gmtime_r(&now, &current_time);

    // Update only the time fields, keep existing date
    current_time.tm_hour = hour;
    current_time.tm_min = minute;
    current_time.tm_sec = second;

    // Convert back to Unix timestamp
    time_t new_timestamp = mktime(&current_time);
    if (new_timestamp == -1) {
        Serial.println("ERROR: Failed to convert time");
        return;
    }

    // Adjust for timezone offset
    new_timestamp -= g_timezone_offset * 3600;

    // Set ESP32 internal RTC
    tv.tv_sec = new_timestamp;
    tv.tv_usec = 0;

    if (settimeofday(&tv, NULL) != 0) {
        Serial.println("ERROR: Failed to set RTC");
        return;
    }

    // Also update DS3231 if present
    if (g_rtc_ds3231_present) {
        rtc.adjust(DateTime(new_timestamp));
        Serial.println("DS3231 RTC updated");
    }

    // Save current timestamp to NVS for time persistence
    storageSaveLastBootTime(new_timestamp);

    // Save time_valid flag if not already set
    if (!storageLoadTimeValid()) {
        if (!storageSaveTimeValid(true)) {
            Serial.println("WARNING: Failed to save time_valid flag to NVS");
        }

        // Initialize drink tracking if time just became valid
        extern bool g_time_valid;
        g_time_valid = true;
        drinksInit();

        // Call callback if registered
        if (g_onTimeSetCallback != nullptr) {
            g_onTimeSetCallback();
        }
    }

    Serial.printf("Time set: %02d:%02d:%02d (date preserved: %04d-%02d-%02d)\n",
                 hour, minute, second,
                 current_time.tm_year + 1900, current_time.tm_mon + 1, current_time.tm_mday);
}

// Handle SET TIMEZONE command (and SET TZ alias)
static void handleSetTimezone(char* args) {
    int offset;
    if (!parseInt(args, offset)) {
        Serial.println("ERROR: Invalid timezone offset");
        Serial.println("Usage: SET TIMEZONE offset  (or SET TZ offset)");
        Serial.println("Example: SET TIMEZONE -8");
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

// Handle GET DAILY STATE command - display current daily state
static void handleGetDailyState() {
    DailyState state;
    drinksGetState(state);
    uint16_t daily_total = drinksGetDailyTotal();
    uint16_t drink_count = drinksGetDrinkCount();

    Serial.println("\n=== DAILY STATE ===");
    Serial.printf("Daily total: %dml / %dml (%d%%)\n",
                 daily_total,
                 DRINK_DAILY_GOAL_ML,
                 (daily_total * 100) / DRINK_DAILY_GOAL_ML);
    Serial.printf("Drink count: %d drinks today\n", drink_count);
    Serial.printf("Last baseline ADC: %d\n", state.last_recorded_adc);
    Serial.printf("Last displayed: %dml\n", state.last_displayed_total_ml);
    Serial.println("==================\n");
}

// Handle GET LAST DRINK command - display most recent drink record
static void handleGetLastDrink() {
    DrinkRecord record;
    if (!storageLoadLastDrinkRecord(record)) {
        Serial.println("No drink records found");
        return;
    }

    const char* drink_type = (record.type == DRINK_TYPE_POUR) ? "POUR" : "GULP";

    Serial.println("\n=== LAST DRINK RECORD ===");

    // Format timestamp
    time_t t = record.timestamp;
    struct tm tm;
    gmtime_r(&t, &tm);
    Serial.printf("Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);

    Serial.printf("Amount: %dml (%s)\n", record.amount_ml, drink_type);
    Serial.printf("Bottle level: %dml\n", record.bottle_level_ml);
    Serial.printf("Flags: 0x%02X (", record.flags);
    if (record.flags & 0x01) Serial.print("synced ");
    if (record.flags & 0x02) Serial.print("day_boundary");
    if (record.flags == 0) Serial.print("not synced");
    Serial.println(")");
    Serial.println("=========================\n");
}

// Handle DUMP DRINKS command - display all drink records
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

                const char* drink_type = (record.type == DRINK_TYPE_POUR) ? "POUR" : "GULP";

                Serial.printf("[%03d] %04d-%02d-%02d %02d:%02d:%02d | %+5dml (%s) | Level: %4dml | Flags: 0x%02X\n",
                             i,
                             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                             tm.tm_hour, tm.tm_min, tm.tm_sec,
                             record.amount_ml,
                             drink_type,
                             record.bottle_level_ml,
                             record.flags);
            }
        }
    }
    Serial.println();
}

// Handle RESET DAILY INTAKE command - reset daily counter
static void handleResetDailyIntake() {
    drinksResetDaily();
    Serial.println("OK: Daily intake reset");
}

// Note: SET DAILY INTAKE command removed - daily totals are now computed from records

// Handle CLEAR DRINKS command - clear all drink records
static void handleClearDrinks() {
    drinksClearAll();
    Serial.println("OK: All drink records cleared");
}

// Handle SET DISPLAY MODE command - switch between man and tumblers
static void handleSetDisplayMode(char* args) {
    int mode;
    if (!parseInt(args, mode)) {
        Serial.println("ERROR: Invalid display mode");
        Serial.println("Usage: SET DISPLAY MODE mode");
        Serial.println("  0 = Human figure (continuous fill)");
        Serial.println("  1 = Tumbler grid (10 glasses)");
        return;
    }

    if (mode < 0 || mode > 1) {
        Serial.println("ERROR: Display mode must be 0 or 1");
        Serial.println("  0 = Human figure (continuous fill)");
        Serial.println("  1 = Tumbler grid (10 glasses)");
        return;
    }

    // Save to NVS
    if (!storageSaveDisplayMode((uint8_t)mode)) {
        Serial.println("ERROR: Failed to save display mode to NVS");
        return;
    }

    // Update global variable
    g_daily_intake_display_mode = (uint8_t)mode;

    const char* mode_name = (mode == 0) ? "Human figure" : "Tumbler grid";
    Serial.printf("Display mode set: %d (%s)\n", mode, mode_name);
    Serial.println("Saved to NVS");

    Serial.println("Display updated");
}

// Handle SET SLEEP TIMEOUT command
// Format: SET SLEEP TIMEOUT seconds (0 = disable sleep)
static void handleSetSleepTimeout(char* args) {
    int seconds;
    if (!parseInt(args, seconds)) {
        Serial.println("ERROR: Invalid timeout");
        Serial.println("Usage: SET SLEEP TIMEOUT seconds");
        Serial.println("  0 = Disable sleep (debug mode)");
        Serial.println("  1-300 = Sleep after N seconds");
        Serial.println("Examples:");
        Serial.println("  SET SLEEP TIMEOUT 30  \xE2\x86\x92 Sleep after 30 seconds (default)");
        Serial.println("  SET SLEEP TIMEOUT 0   \xE2\x86\x92 Never sleep (for debugging)");
        return;
    }

    if (seconds < 0 || seconds > 300) {
        Serial.println("ERROR: Timeout must be 0-300 seconds");
        return;
    }

    g_sleep_timeout_ms = (uint32_t)seconds * 1000;

    // Save to NVS for persistence
    if (storageSaveSleepTimeout((uint32_t)seconds)) {
        if (seconds == 0) {
            Serial.println("Sleep DISABLED (debug mode)");
            Serial.println("Device will never enter deep sleep");
            Serial.println("Setting saved to NVS - persists across reboots");
        } else {
            Serial.printf("Sleep timeout set: %d seconds\n", seconds);
            Serial.println("Setting saved to NVS - persists across reboots");
        }
    } else {
        Serial.println("WARNING: Failed to save to NVS - will reset to default on reboot");
    }
}

// Handle SET EXTENDED SLEEP TIMER command
// Format: SET EXTENDED SLEEP TIMER seconds
static void handleSetExtendedSleepTimer(char* args) {
    int seconds;
    if (!parseInt(args, seconds)) {
        Serial.println("ERROR: Invalid timer duration");
        Serial.println("Usage: SET EXTENDED SLEEP TIMER seconds");
        Serial.println("  1-3600 = Timer wake interval in extended mode");
        Serial.println("Examples:");
        Serial.println("  SET EXTENDED SLEEP TIMER 60   - 1 minute timer wake (default)");
        Serial.println("  SET EXTENDED SLEEP TIMER 120  - 2 minute timer wake");
        return;
    }

    if (seconds < 1 || seconds > 3600) {
        Serial.println("ERROR: Timer duration must be 1-3600 seconds");
        return;
    }

    extern uint32_t g_extended_sleep_timer_sec;
    g_extended_sleep_timer_sec = (uint32_t)seconds;

    // Save to NVS for persistence
    if (storageSaveExtendedSleepTimer((uint32_t)seconds)) {
        Serial.printf("Extended sleep timer set: %d seconds\n", seconds);
        Serial.println("Setting saved to NVS - persists across reboots");
    } else {
        Serial.println("WARNING: Failed to save to NVS - will reset to default on reboot");
    }
}

// Handle SET EXTENDED SLEEP THRESHOLD command
// Format: SET EXTENDED SLEEP THRESHOLD seconds
static void handleSetExtendedSleepThreshold(char* args) {
    int seconds;
    if (!parseInt(args, seconds)) {
        Serial.println("ERROR: Invalid threshold");
        Serial.println("Usage: SET EXTENDED SLEEP THRESHOLD seconds");
        Serial.println("  30-600 = Continuous awake threshold before extended mode");
        Serial.println("Examples:");
        Serial.println("  SET EXTENDED SLEEP THRESHOLD 120  - 2 minutes (default)");
        Serial.println("  SET EXTENDED SLEEP THRESHOLD 60   - 1 minute");
        return;
    }

    if (seconds < 30 || seconds > 600) {
        Serial.println("ERROR: Threshold must be 30-600 seconds");
        return;
    }

    extern uint32_t g_extended_sleep_threshold_sec;
    g_extended_sleep_threshold_sec = (uint32_t)seconds;

    // Save to NVS for persistence
    if (storageSaveExtendedSleepThreshold((uint32_t)seconds)) {
        Serial.printf("Extended sleep threshold set: %d seconds\n", seconds);
        Serial.println("Setting saved to NVS - persists across reboots");
    } else {
        Serial.println("WARNING: Failed to save to NVS - will reset to default on reboot");
    }
}

// Handle TARE command - zero the scale at current weight
static void handleTare() {
    // Check if NAU7802 is ready
    if (!weightIsReady()) {
        Serial.println("ERROR: NAU7802 not ready");
        return;
    }

    Serial.println("Taking tare reading...");

    // Take a quick weight measurement (2 seconds)
    WeightConfig quick_config = weightGetDefaultConfig();
    quick_config.duration_seconds = 2;

    WeightMeasurement tare_measurement = weightMeasureStable(quick_config);

    if (!tare_measurement.valid) {
        Serial.println("ERROR: Failed to get stable tare reading");
        return;
    }

    // Save as tare offset
    CalibrationData cal;
    if (storageLoadCalibration(cal)) {
        // Update the empty bottle reading to current reading (tare)
        cal.empty_bottle_adc = tare_measurement.raw_adc;

        // Recalculate scale factor (if we have a valid full bottle reading)
        if (cal.calibration_valid && cal.full_bottle_adc != cal.empty_bottle_adc) {
            cal.scale_factor = (float)(cal.full_bottle_adc - cal.empty_bottle_adc) / 830.0f;
        }

        // Save updated calibration
        if (storageSaveCalibration(cal)) {
            Serial.println("OK: Tare set successfully");
            Serial.printf("New tare ADC: %d\n", cal.empty_bottle_adc);
            if (cal.calibration_valid) {
                Serial.printf("Updated scale factor: %.2f counts/g\n", cal.scale_factor);
            }

            // Force display refresh to show updated water level
            extern void forceDisplayRefresh();
            forceDisplayRefresh();
        } else {
            Serial.println("ERROR: Failed to save tare offset");
        }
    } else {
        // No existing calibration, create a new one with just tare
        cal = storageGetEmptyCalibration();
        cal.empty_bottle_adc = tare_measurement.raw_adc;
        cal.calibration_valid = 0; // Not fully calibrated yet

        if (storageSaveCalibration(cal)) {
            Serial.println("OK: Tare set successfully");
            Serial.printf("Tare ADC: %d\n", cal.empty_bottle_adc);
            Serial.println("Note: Full calibration still required (SET FULL BOTTLE)");

            // Force display refresh to show updated water level
            extern void forceDisplayRefresh();
            forceDisplayRefresh();
        } else {
            Serial.println("ERROR: Failed to save tare offset");
        }
    }
}

// Handle GET STATUS command - show all system status
static void handleGetStatus() {
    extern bool g_calibrated;
    extern int8_t g_timezone_offset;
    extern bool g_time_valid;
    extern uint8_t g_daily_intake_display_mode;
    extern uint32_t g_sleep_timeout_ms;
    extern uint32_t g_extended_sleep_timer_sec;
    extern uint32_t g_extended_sleep_threshold_sec;
    extern bool g_in_extended_sleep_mode;
    extern unsigned long g_continuous_awake_start;

    Serial.println("\n=== SYSTEM STATUS ===");

    // Calibration status
    Serial.print("Calibration: ");
    Serial.println(g_calibrated ? "VALID" : "NOT CALIBRATED");

    // Time configuration
    Serial.print("Time valid: ");
    Serial.println(g_time_valid ? "YES" : "NO");
    if (g_time_valid) {
        Serial.print("Timezone offset: ");
        Serial.println(g_timezone_offset);

        // Show current time
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec + (g_timezone_offset * 3600);
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        Serial.printf("Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    // Display mode
    Serial.print("Display mode: ");
    Serial.print(g_daily_intake_display_mode);
    Serial.print(" (");
    Serial.print((g_daily_intake_display_mode == 0) ? "Human figure" : "Tumbler grid");
    Serial.println(")");

    // Sleep settings
    Serial.print("Normal sleep timeout: ");
    if (g_sleep_timeout_ms == 0) {
        Serial.println("DISABLED");
    } else {
        Serial.print(g_sleep_timeout_ms / 1000);
        Serial.println(" seconds");
    }

    Serial.print("Extended sleep timer: ");
    Serial.print(g_extended_sleep_timer_sec);
    Serial.println(" seconds");

    Serial.print("Extended sleep threshold: ");
    Serial.print(g_extended_sleep_threshold_sec);
    Serial.println(" seconds");

    Serial.print("Extended sleep mode: ");
    Serial.println(g_in_extended_sleep_mode ? "ACTIVE" : "INACTIVE");

    if (g_continuous_awake_start > 0) {
        unsigned long awake_time = (millis() - g_continuous_awake_start) / 1000;
        Serial.print("Continuous awake time: ");
        Serial.print(awake_time);
        Serial.println(" seconds");
    }

    Serial.println("=====================\n");
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

// Helper: Parse command into words (space-separated)
// Modifies input string in-place, null-terminates words
// Returns number of words parsed, sets args pointer to remaining string
static int parseCommandWords(char* input, char* words[], int max_words, char** args) {
    // Trim leading whitespace
    while (*input == ' ' || *input == '\t') input++;
    
    if (*input == '\0') {
        *args = input;
        return 0;
    }

    // Find end of original string before we modify it
    char* original_end = input + strlen(input);

    int word_count = 0;
    char* start = input;
    bool in_word = false;

    // Parse words and convert to uppercase
    for (char* p = input; *p && word_count < max_words; p++) {
        if (*p == ' ' || *p == '\t') {
            if (in_word) {
                *p = '\0';  // Null-terminate word
                words[word_count++] = start;
                in_word = false;
            }
        } else {
            if (!in_word) {
                start = p;
                in_word = true;
            }
            *p = toupper((unsigned char)*p);  // Convert to uppercase
        }
    }

    // Handle last word
    if (in_word && word_count < max_words) {
        words[word_count++] = start;
    }

    // Find where arguments start (after last word)
    if (word_count == 0) {
        *args = input;
        return 0;
    }

    // Find end of last word
    char* last_word_end = words[word_count - 1] + strlen(words[word_count - 1]);
    
    // Skip null terminators we added and whitespace to find arguments
    char* p = last_word_end;
    while (p < original_end && (*p == '\0' || *p == ' ' || *p == '\t')) {
        p++;
    }
    
    // If we've reached the original end, there are no args
    if (p >= original_end || *p == '\0') {
        *args = original_end;  // Point to end of string (null terminator)
    } else {
        *args = p;  // Point to start of arguments
    }

    return word_count;
}

// Helper: Check if words match a command pattern
static bool matchWords(char* words[], int word_count, const char* pattern[], int pattern_count) {
    if (word_count != pattern_count) return false;
    for (int i = 0; i < word_count; i++) {
        if (strcmp(words[i], pattern[i]) != 0) return false;
    }
    return true;
}

// Helper: Check if first N words match a pattern (allows extra words for arguments)
static bool matchWordsPrefix(char* words[], int word_count, const char* pattern[], int pattern_count) {
    if (word_count < pattern_count) return false;  // Not enough words
    for (int i = 0; i < pattern_count; i++) {
        if (strcmp(words[i], pattern[i]) != 0) return false;
    }
    return true;
}

// Static empty string for commands with no arguments
static char empty_args[] = "";

// Helper: Reconstruct args string from remaining words (for multi-word arguments)
// Returns pointer to static buffer containing reconstructed args, or original args if no extra words
static char* reconstructArgs(char* words[], int word_count, int pattern_count, char* original_args) {
    if (word_count <= pattern_count) {
        return (original_args && *original_args != '\0') ? original_args : empty_args;
    }
    
    // Reconstruct args by joining remaining words with spaces
    static char args_buffer[128];  // Static buffer for reconstructed args
    int pos = 0;
    for (int i = pattern_count; i < word_count && pos < sizeof(args_buffer) - 1; i++) {
        if (i > pattern_count && pos < sizeof(args_buffer) - 1) {
            args_buffer[pos++] = ' ';  // Add space between words
        }
        int len = strlen(words[i]);
        if (pos + len < sizeof(args_buffer) - 1) {
            strncpy(args_buffer + pos, words[i], sizeof(args_buffer) - pos - 1);
            pos += len;
        }
    }
    args_buffer[pos] = '\0';
    return args_buffer;
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

    // Check for single-character test interrupt command ('T' or 't')
    if (strlen(cmd) == 1 && (cmd[0] == 'T' || cmd[0] == 't')) {
        extern void testInterruptState();
        testInterruptState();
        return;
    }

    // Parse command words
    char* words[8];  // Max 8 words should be enough
    char* args;
    int word_count = parseCommandWords(cmd, words, 8, &args);
    
    if (word_count == 0) return;

    // Trim leading whitespace from args
    while (*args == ' ' || *args == '\t') args++;
    if (*args == '\0') args = nullptr;

    // Match commands by word count and pattern
    // One-word commands
    if (word_count >= 1) {
        const char* pattern1[] = {"TARE"};
        if (matchWordsPrefix(words, word_count, pattern1, 1)) {
            handleTare();
            return;
        }
    }

    // Two-word commands (check if first 2 words match, even if more words present for arguments)
    if (word_count >= 2) {
        const char* pattern1[] = {"SET", "DATE"};
        if (matchWordsPrefix(words, word_count, pattern1, 2)) {
            handleSetDate(reconstructArgs(words, word_count, 2, args));
            return;
        }
        const char* pattern2[] = {"SET", "TIME"};
        if (matchWordsPrefix(words, word_count, pattern2, 2)) {
            handleSetTime(reconstructArgs(words, word_count, 2, args));
            return;
        }
        const char* pattern3[] = {"GET", "TIME"};
        if (matchWordsPrefix(words, word_count, pattern3, 2)) {
            handleGetTime();
            return;
        }
        const char* pattern4[] = {"SET", "TZ"};
        if (matchWordsPrefix(words, word_count, pattern4, 2)) {
            handleSetTimezone(reconstructArgs(words, word_count, 2, args));
            return;
        }
        const char* pattern5[] = {"GET", "STATUS"};
        if (matchWordsPrefix(words, word_count, pattern5, 2)) {
            handleGetStatus();
            return;
        }
        const char* pattern6[] = {"DUMP", "DRINKS"};
        if (matchWordsPrefix(words, word_count, pattern6, 2)) {
            handleDumpDrinks();
            return;
        }
        const char* pattern7[] = {"CLEAR", "DRINKS"};
        if (matchWordsPrefix(words, word_count, pattern7, 2)) {
            handleClearDrinks();
            return;
        }
        const char* pattern8[] = {"SET", "DATETIME"};
        if (matchWordsPrefix(words, word_count, pattern8, 2)) {
            handleSetDateTime(reconstructArgs(words, word_count, 2, args));
            return;
        }
        const char* pattern9[] = {"SET", "TIMEZONE"};
        if (matchWordsPrefix(words, word_count, pattern9, 2)) {
            handleSetTimezone(reconstructArgs(words, word_count, 2, args));
            return;
        }
    }
    
    // Three-word commands (check if first 3 words match, even if more words present for arguments)
    if (word_count >= 3) {
        const char* pattern3[] = {"GET", "DAILY", "STATE"};
        if (matchWordsPrefix(words, word_count, pattern3, 3)) {
            handleGetDailyState();
            return;
        }
        const char* pattern4[] = {"GET", "LAST", "DRINK"};
        if (matchWordsPrefix(words, word_count, pattern4, 3)) {
            handleGetLastDrink();
            return;
        }
        const char* pattern5[] = {"SET", "DISPLAY", "MODE"};
        if (matchWordsPrefix(words, word_count, pattern5, 3)) {
            handleSetDisplayMode(reconstructArgs(words, word_count, 3, args));
            return;
        }
        const char* pattern6[] = {"SET", "SLEEP", "TIMEOUT"};
        if (matchWordsPrefix(words, word_count, pattern6, 3)) {
            handleSetSleepTimeout(reconstructArgs(words, word_count, 3, args));
            return;
        }
        // Note: SET DAILY INTAKE removed - daily totals computed from records
        const char* pattern8[] = {"RESET", "DAILY", "INTAKE"};
        if (matchWordsPrefix(words, word_count, pattern8, 3)) {
            handleResetDailyIntake();
            return;
        }
    }
    
    // Four-word commands (check if first 4 words match, even if more words present for arguments)
    if (word_count >= 4) {
        const char* pattern1[] = {"SET", "NORMAL", "SLEEP", "TIMEOUT"};
        if (matchWordsPrefix(words, word_count, pattern1, 4)) {
            handleSetSleepTimeout(reconstructArgs(words, word_count, 4, args));
            return;
        }
        const char* pattern2[] = {"SET", "EXTENDED", "SLEEP", "TIMER"};
        if (matchWordsPrefix(words, word_count, pattern2, 4)) {
            handleSetExtendedSleepTimer(reconstructArgs(words, word_count, 4, args));
            return;
        }
        const char* pattern3[] = {"SET", "EXTENDED", "SLEEP", "THRESHOLD"};
        if (matchWordsPrefix(words, word_count, pattern3, 4)) {
            handleSetExtendedSleepThreshold(reconstructArgs(words, word_count, 4, args));
            return;
        }
    }

    // Command not found
    Serial.print("ERROR: Unknown command: ");
    for (int i = 0; i < word_count; i++) {
        if (i > 0) Serial.print(" ");
        Serial.print(words[i]);
    }
    Serial.println();
    Serial.println("\nAvailable commands:");
    Serial.println("Debug Control:");
    Serial.println("  0-4, 9                - Set debug level (single character)");
    Serial.println("                          0=OFF, 1=Events, 2=+Gestures,");
    Serial.println("                          3=+Weight, 4=+Accel, 9=All ON");
    Serial.println("  T                     - Test interrupt state (shows INT1_SRC)");
    Serial.println("\nCalibration:");
    Serial.println("  TARE                  - Zero the scale at current weight");
    Serial.println("\nTime/Timezone:");
    Serial.println("  SET DATETIME YYYY-MM-DD HH:MM:SS [tz]    - Set date, time, and timezone");
    Serial.println("  SET DATE YYYY-MM-DD                       - Set date only");
    Serial.println("  SET TIME HH[:MM[:SS]]                     - Set time (defaults: MM=00, SS=00)");
    Serial.println("  SET TZ offset                             - Set timezone (alias: SET TIMEZONE)");
    Serial.println("  GET TIME                                  - Show current time");
    Serial.println("\nDrink Tracking:");
    Serial.println("  GET DAILY STATE       - Show current daily state");
    Serial.println("  GET LAST DRINK        - Show most recent drink record");
    Serial.println("  DUMP DRINKS           - Display all drink records");
    Serial.println("  RESET DAILY INTAKE    - Reset daily intake (marks today's records as deleted)");
    Serial.println("  CLEAR DRINKS          - Clear all drink records (WARNING: erases data)");
    Serial.println("\nDisplay Settings:");
    Serial.println("  SET DISPLAY MODE mode - Switch intake visualization (0=human, 1=tumblers)");
    Serial.println("\nPower Management:");
    Serial.println("  SET SLEEP TIMEOUT sec         - Normal sleep timeout (0=disable, default=30)");
    Serial.println("  SET EXTENDED SLEEP TIMER sec  - Extended sleep timer wake (default=60)");
    Serial.println("  SET EXTENDED SLEEP THRESHOLD sec - Awake threshold for extended mode (default=120)");
    Serial.println("\nSystem Status:");
    Serial.println("  GET STATUS            - Show all system status and settings");
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

#endif // ENABLE_SERIAL_COMMANDS
