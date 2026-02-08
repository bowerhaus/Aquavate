/**
 * Aquavate - Smart Water Bottle Firmware
 * Main entry point
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADXL343.h>
#include <RTClib.h>  // Adafruit RTClib for DS3231
#include "aquavate.h"
#include "config.h"

// Calibration system includes
#include "gestures.h"
#include "weight.h"
#include "storage.h"
#include "calibration.h"
#include "ui_calibration.h"  // Always include - has uiShowBottleEmptied for shake-to-empty

// Serial commands for time setting (conditional)
#if ENABLE_SERIAL_COMMANDS
#include "serial_commands.h"
#endif
#include <sys/time.h>
#include <time.h>
#include <nvs_flash.h>

// Daily intake tracking
#include "drinks.h"
#include "storage_drinks.h"

// Display state tracking
#include "display.h"

// Activity stats tracking
#include "activity_stats.h"

// BLE service (conditional)
#if ENABLE_BLE
#include "ble_service.h"
#endif


// FIX Bug #4: Sensor snapshot to prevent multiple reads per loop
struct SensorSnapshot {
    uint32_t timestamp;
    int32_t adc_reading;
    float water_ml;
    GestureType gesture;
};

Adafruit_NAU7802 nau;
Adafruit_ADXL343 adxl = Adafruit_ADXL343(12345);  // 12345 = sensor ID
RTC_DS3231 rtc;  // DS3231 external RTC
bool nauReady = false;
bool adxlReady = false;

unsigned long wakeTime = 0;

// Extended deep sleep tracking (Plan 034: renamed for clarity)
unsigned long g_time_since_stable_start = 0;    // When bottle last became NOT stable (for backpack detection)
bool g_in_extended_sleep_mode = false;          // Currently using extended sleep
uint32_t g_time_since_stable_threshold_sec = TIME_SINCE_STABLE_THRESHOLD_SEC; // Time without stability before extended sleep (3 min)
uint32_t g_extended_sleep_timer_sec = 60;       // Timer wake interval for extended sleep (legacy, tap-wake replaced this)
bool g_force_display_clear_sleep = false;       // Flag to clear Zzzz indicator on wake from extended sleep
bool g_rollover_wake_pending = false;           // Flag to process 4am rollover after initialization

// RTC memory persistence (survives deep sleep)
RTC_DATA_ATTR uint32_t rtc_extended_sleep_magic = 0;
RTC_DATA_ATTR bool rtc_in_extended_sleep_mode = false;
RTC_DATA_ATTR unsigned long rtc_time_since_stable_start = 0;
RTC_DATA_ATTR bool rtc_backpack_screen_shown = false;  // Track if backpack mode screen shown (Issue #38)
RTC_DATA_ATTR bool rtc_rollover_wake_pending = false;  // True if rollover timer was set for 4am wake
RTC_DATA_ATTR bool rtc_tap_wake_enabled = false;       // True if in backpack mode expecting tap wake
RTC_DATA_ATTR bool rtc_health_check_wake = false;      // True if timer was set for health check (not rollover)

// Sync timeout tracking - regular variable, set at wake time
// Only extend timeout if NEW drinks recorded during this wake session
uint16_t g_unsynced_at_wake = 0;

// Magic number for validating RTC memory after deep sleep
#define RTC_EXTENDED_SLEEP_MAGIC 0x45585400  // "EXT\0" in ASCII

// Bitmap data and drawing functions moved to display.cpp

// Calibration state
CalibrationData g_calibration;
bool g_calibrated = false;
#if ENABLE_STANDALONE_CALIBRATION
CalibrationState g_last_cal_state = CAL_IDLE;
bool g_cal_just_cancelled = false;  // Prevent immediate re-trigger after abort
#endif

// Time state
int8_t g_timezone_offset = 0;  // UTC offset in hours
bool g_time_valid = false;     // Has time been set?
bool g_rtc_ds3231_present = false;  // DS3231 RTC detected (future)

// Shake to empty gesture state (shake-while-inverted)
bool g_cancel_drink_pending = false;  // Set when shake gesture detected, cleared on upright stable

// Double-tap gating (reset each wake cycle)
bool g_has_been_upright_stable = false;  // Gate for double-tap backpack entry

// Runtime debug control (non-persistent, reset on boot)
// Global enable flag
bool g_debug_enabled = DEBUG_ENABLED;
// Individual category flags
bool g_debug_water_level = DEBUG_WATER_LEVEL;
bool g_debug_accelerometer = DEBUG_ACCELEROMETER;
bool g_debug_display = DEBUG_DISPLAY_UPDATES;
bool g_debug_drink_tracking = DEBUG_DRINK_TRACKING;
bool g_debug_calibration = DEBUG_CALIBRATION;
bool g_debug_ble = DEBUG_BLE;

// Runtime display mode (persistent via NVS)
uint8_t g_daily_intake_display_mode = DAILY_INTAKE_DISPLAY_MODE;

// Runtime activity timeout control (0 = disabled, non-persistent)
uint32_t g_sleep_timeout_ms = ACTIVITY_TIMEOUT_MS;  // Default 30 seconds

#if defined(BOARD_ADAFRUIT_FEATHER)
#include "Adafruit_ThinkInk.h"

// 2.13" Mono E-Paper display (GDEY0213B74 variant - no 8-pixel shift)
ThinkInk_213_Mono_GDEY0213B74 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

float getBatteryVoltage() {
    float voltage = analogReadMilliVolts(VBAT_PIN);
    voltage *= 2;     // Voltage divider, multiply back
    voltage /= 1000;  // Convert to volts
    return voltage;
}

int getBatteryPercent(float voltage) {
    // LiPo: BATTERY_VOLTAGE_FULL = 100%, BATTERY_VOLTAGE_EMPTY = 0%
    int percent = (voltage - BATTERY_VOLTAGE_EMPTY) / (BATTERY_VOLTAGE_FULL - BATTERY_VOLTAGE_EMPTY) * 100;
    if (percent > 100) percent = 100;
    if (percent < 0) percent = 0;
    return percent;
}

// Drawing helper functions moved to display.cpp
#endif

// Callback when time is set via serial command
void onTimeSet() {
    g_time_valid = true;
    Serial.println("Main: Time set callback - time is now valid");
}

void writeAccelReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(I2C_ADDR_ADXL343);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t readAccelReg(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDR_ADXL343);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)I2C_ADDR_ADXL343, (uint8_t)1);
    return Wire.read();
}

void testInterruptState() {
    // Manual test function - move/tilt bottle to see if activity interrupt triggers
    if (!adxlReady) {
        Serial.println("ADXL343 not ready");
        return;
    }

    Serial.println("\n=== INTERRUPT STATE TEST ===");

    // Read current accelerometer values
    int16_t x, y, z;
    adxl.getXYZ(x, y, z);
    float x_g = x / 256.0f;
    float y_g = y / 256.0f;
    float z_g = z / 256.0f;

    Serial.print("Current orientation: X=");
    Serial.print(x_g, 3);
    Serial.print("g, Y=");
    Serial.print(y_g, 3);
    Serial.print("g, Z=");
    Serial.print(z_g, 3);
    Serial.println("g");

    // Read interrupt registers (ADXL343 auto-clears on read)
    uint8_t int_source = readAccelReg(0x30);  // INT_SOURCE register
    uint8_t int_enable = readAccelReg(0x2E);  // INT_ENABLE register
    uint8_t thresh_act = readAccelReg(0x24);  // THRESH_ACT register
    uint8_t act_inact_ctl = readAccelReg(0x27);  // ACT_INACT_CTL register
    int pin_state = digitalRead(PIN_ACCEL_INT);

    Serial.print("INT_SOURCE: 0x");
    Serial.print(int_source, HEX);
    Serial.print(" - Activity=");
    Serial.println((int_source & 0x10) ? "1 (triggered!)" : "0");

    Serial.print("INT_ENABLE: 0x");
    Serial.print(int_enable, HEX);
    Serial.print(" (Activity=");
    Serial.print((int_enable & 0x10) ? "enabled" : "disabled");
    Serial.println(")");

    Serial.print("ACT_INACT_CTL: 0x");
    Serial.print(act_inact_ctl, HEX);
    Serial.print(" (Axes: ");
    Serial.print((act_inact_ctl & 0x70) == 0x70 ? "X/Y/Z all enabled" : "partial");
    Serial.println(")");

    Serial.print("Activity Threshold: 0x");
    Serial.print(thresh_act, HEX);
    Serial.print(" = ");
    Serial.print(thresh_act * 0.0625f, 3);
    Serial.println("g");

    Serial.print("INT pin state: ");
    Serial.println(pin_state ? "HIGH (interrupt active)" : "LOW (cleared)");

    Serial.println("\nTo test: Move or tilt the bottle - INT should pulse HIGH");
    Serial.println("=========================\n");
}

void configureADXL343Interrupt() {
    DEBUG_PRINTLN(g_debug_accelerometer, "\n=== ADXL343 Interrupt Configuration ===");

    // Set interrupt pin mode
    pinMode(PIN_ACCEL_INT, INPUT_PULLDOWN);  // Active-high interrupt

    // ADXL343 Register Definitions
    const uint8_t THRESH_ACT = 0x24;        // Activity threshold
    const uint8_t THRESH_TAP = 0x1D;        // Tap threshold
    const uint8_t DUR = 0x21;               // Tap duration
    const uint8_t LATENT = 0x22;            // Tap latency (for double-tap)
    const uint8_t WINDOW = 0x23;            // Tap window (for double-tap)
    const uint8_t ACT_INACT_CTL = 0x27;     // Axis enable for activity/inactivity
    const uint8_t TAP_AXES = 0x2A;          // Axis participation for tap
    const uint8_t POWER_CTL = 0x2D;         // Power control
    const uint8_t INT_ENABLE = 0x2E;        // Interrupt enable
    const uint8_t INT_MAP = 0x2F;           // Interrupt mapping
    const uint8_t INT_SOURCE = 0x30;        // Interrupt source (read to clear)
    const uint8_t DATA_FORMAT = 0x31;       // Data format

    // Step 1: Configure data format (±2g range)
    writeAccelReg(DATA_FORMAT, 0x00);       // ±2g, 13-bit resolution, right-justified
    DEBUG_PRINTLN(g_debug_accelerometer, "1. Data format: ±2g range");

    // Step 2: Set activity threshold for tilt/motion wake
    // AC-coupled mode detects CHANGES in acceleration (motion)
    // Scale = 62.5 mg/LSB
    writeAccelReg(THRESH_ACT, ACTIVITY_WAKE_THRESHOLD);
    DEBUG_PRINTF(g_debug_accelerometer, "2. Activity threshold: 0x%02X (%.1fg)\n", ACTIVITY_WAKE_THRESHOLD, ACTIVITY_WAKE_THRESHOLD * 0.0625f);

    // Step 3: Set double-tap latency (wait after first tap before window opens)
    // Scale = 1.25 ms/LSB, 100ms = 80 (0x50)
    writeAccelReg(LATENT, TAP_WAKE_LATENT);
    DEBUG_PRINTF(g_debug_accelerometer, "3. Tap latency: 0x%02X (%.0fms)\n", TAP_WAKE_LATENT, TAP_WAKE_LATENT * 1.25f);

    // Step 3b: Set double-tap window (time for second tap after latency)
    // Scale = 1.25 ms/LSB, 300ms = 240 (0xF0)
    writeAccelReg(WINDOW, TAP_WAKE_WINDOW);
    DEBUG_PRINTF(g_debug_accelerometer, "3b. Tap window: 0x%02X (%.0fms)\n", TAP_WAKE_WINDOW, TAP_WAKE_WINDOW * 1.25f);

    // Step 4: Enable all axes for activity detection (AC-coupled)
    // Bits: 7=ACT_acdc(1=AC), 6-4=ACT_X/Y/Z (111=all axes)
    // ACT_INACT_CTL[7:4] = 1111b = 0xF0
    writeAccelReg(ACT_INACT_CTL, 0xF0);     // All axes activity enable (AC-coupled)
    DEBUG_PRINTLN(g_debug_accelerometer, "4. Activity axes: X, Y, Z (AC-coupled)");

    // Step 5: Configure tap threshold (used by double-tap detection for backpack mode)
    // Scale = 62.5 mg/LSB, 3.0g = 48 (0x30)
    writeAccelReg(THRESH_TAP, TAP_WAKE_THRESHOLD);
    DEBUG_PRINTF(g_debug_accelerometer, "5. Tap threshold: 0x%02X (%.1fg)\n", TAP_WAKE_THRESHOLD, TAP_WAKE_THRESHOLD * 0.0625f);

    // Step 6: Set tap duration (max time above threshold for valid tap)
    // Scale = 625 us/LSB, 10ms = 16 (0x10)
    writeAccelReg(DUR, TAP_WAKE_DURATION);
    DEBUG_PRINTF(g_debug_accelerometer, "6. Tap duration: 0x%02X (%.1fms)\n", TAP_WAKE_DURATION, TAP_WAKE_DURATION * 0.625f);

    // Step 7: Enable all axes for tap detection
    writeAccelReg(TAP_AXES, 0x07);          // X, Y, Z participate in tap
    DEBUG_PRINTLN(g_debug_accelerometer, "7. Tap axes: X, Y, Z enabled");

    // Step 8: Enable measurement mode
    writeAccelReg(POWER_CTL, 0x08);         // Measurement mode (bit 3)
    DEBUG_PRINTLN(g_debug_accelerometer, "8. Power mode: measurement");

    // Step 9: Enable activity + double-tap interrupts
    // Bit 4 = Activity (0x10) - wake from normal sleep
    // Bit 5 = Double-tap (0x20) - manual backpack mode entry while awake
    // Note: Single-tap (0x40) removed - redundant with activity (1.5g < 3.0g tap threshold)
    writeAccelReg(INT_ENABLE, 0x30);        // Activity + double-tap interrupts
    DEBUG_PRINTLN(g_debug_accelerometer, "9. Interrupt enable: activity + double-tap");

    // Step 10: Route all interrupts to INT1 pin
    writeAccelReg(INT_MAP, 0x00);           // All interrupts to INT1
    DEBUG_PRINTLN(g_debug_accelerometer, "10. Interrupt routing: INT1");

    // Step 11: Clear any pending interrupts
    uint8_t int_source = readAccelReg(INT_SOURCE);
    DEBUG_PRINTF(g_debug_accelerometer, "11. Cleared INT_SOURCE: 0x%02X\n", int_source);

    DEBUG_PRINTLN(g_debug_accelerometer, "\n=== Configuration Complete ===");
    DEBUG_PRINTF(g_debug_accelerometer, "Sleep wake: Activity (>%.1fg)\n", ACTIVITY_WAKE_THRESHOLD * 0.0625f);
    DEBUG_PRINTLN(g_debug_accelerometer, "Awake: Double-tap detected via INT_SOURCE polling\n");

    // Unconditional one-line summary
    Serial.printf("ADXL343: Interrupts configured (activity >%.1fg, double-tap >%.1fg)\n",
                  ACTIVITY_WAKE_THRESHOLD * 0.0625f, TAP_WAKE_THRESHOLD * 0.0625f);
}

// Configure ADXL343 for double-tap detection (backpack mode wake)
void configureADXL343TapWake() {
    DEBUG_PRINTLN(g_debug_accelerometer, "\n=== ADXL343 Tap Wake Configuration ===");

    // Set interrupt pin mode
    pinMode(PIN_ACCEL_INT, INPUT_PULLDOWN);  // Active-high interrupt

    // ADXL343 Register Definitions for tap detection
    const uint8_t THRESH_TAP = 0x1D;        // Tap threshold
    const uint8_t DUR = 0x21;               // Tap duration
    const uint8_t LATENT = 0x22;            // Latency for double-tap
    const uint8_t WINDOW = 0x23;            // Window for double-tap
    const uint8_t TAP_AXES = 0x2A;          // Axis participation
    const uint8_t POWER_CTL = 0x2D;         // Power control
    const uint8_t INT_ENABLE = 0x2E;        // Interrupt enable
    const uint8_t INT_MAP = 0x2F;           // Interrupt mapping
    const uint8_t DATA_FORMAT = 0x31;       // Data format
    const uint8_t INT_SOURCE = 0x30;        // Interrupt source (read to clear)

    // Step 1: Configure data format (±2g range)
    writeAccelReg(DATA_FORMAT, 0x00);       // ±2g, 13-bit resolution
    DEBUG_PRINTLN(g_debug_accelerometer, "1. Data format: +/-2g range");

    // Step 2: Set tap threshold
    // Scale = 62.5 mg/LSB, 3.0g = 48 (0x30)
    writeAccelReg(THRESH_TAP, TAP_WAKE_THRESHOLD);
    DEBUG_PRINTF(g_debug_accelerometer, "2. Tap threshold: 0x%02X (%.1fg)\n", TAP_WAKE_THRESHOLD, TAP_WAKE_THRESHOLD * 0.0625f);

    // Step 3: Set tap duration (max time above threshold for valid tap)
    // Scale = 625 us/LSB, 10ms = 16 (0x10)
    writeAccelReg(DUR, TAP_WAKE_DURATION);
    DEBUG_PRINTF(g_debug_accelerometer, "3. Tap duration: 0x%02X (%.1fms)\n", TAP_WAKE_DURATION, TAP_WAKE_DURATION * 0.625f);

    // Step 4: Set latency (wait after first tap before window opens)
    // Scale = 1.25 ms/LSB, 100ms = 80 (0x50)
    writeAccelReg(LATENT, TAP_WAKE_LATENT);
    DEBUG_PRINTF(g_debug_accelerometer, "4. Tap latency: 0x%02X (%.0fms)\n", TAP_WAKE_LATENT, TAP_WAKE_LATENT * 1.25f);

    // Step 5: Set window (time for second tap after latency)
    // Scale = 1.25 ms/LSB, 300ms = 240 (0xF0)
    writeAccelReg(WINDOW, TAP_WAKE_WINDOW);
    DEBUG_PRINTF(g_debug_accelerometer, "5. Tap window: 0x%02X (%.0fms)\n", TAP_WAKE_WINDOW, TAP_WAKE_WINDOW * 1.25f);

    // Step 6: Enable all tap axes (X, Y, Z participate)
    writeAccelReg(TAP_AXES, 0x07);          // All three axes
    DEBUG_PRINTLN(g_debug_accelerometer, "6. Tap axes: X, Y, Z enabled");

    // Step 7: Enable measurement mode
    writeAccelReg(POWER_CTL, 0x08);         // Measurement mode (bit 3)
    DEBUG_PRINTLN(g_debug_accelerometer, "7. Power mode: measurement");

    // Step 8: Enable double-tap interrupt only (bit 5)
    writeAccelReg(INT_ENABLE, 0x20);        // Double-tap interrupt
    DEBUG_PRINTLN(g_debug_accelerometer, "8. Interrupt enable: double-tap");

    // Step 9: Route double-tap to INT1 pin (bit 5 = 0 for INT1)
    writeAccelReg(INT_MAP, 0x00);           // All interrupts to INT1
    DEBUG_PRINTLN(g_debug_accelerometer, "9. Interrupt routing: INT1");

    // Step 10: Read INT_SOURCE to clear any pending interrupts
    uint8_t int_source = readAccelReg(INT_SOURCE);
    DEBUG_PRINTF(g_debug_accelerometer, "10. Cleared INT_SOURCE: 0x%02X\n", int_source);

    DEBUG_PRINTLN(g_debug_accelerometer, "\n=== Tap Wake Configuration Complete ===");
    DEBUG_PRINTLN(g_debug_accelerometer, "Wake condition: Double-tap (firm tap, wait 100ms, tap again within 300ms)\n");

    // Unconditional one-line summary
    Serial.printf("ADXL343: Tap wake configured (threshold >%.1fg, double-tap)\n",
                  TAP_WAKE_THRESHOLD * 0.0625f);
}

// Save extended sleep state to RTC memory
void extendedSleepSaveToRTC() {
    rtc_extended_sleep_magic = RTC_EXTENDED_SLEEP_MAGIC;
    rtc_in_extended_sleep_mode = g_in_extended_sleep_mode;
    rtc_time_since_stable_start = g_time_since_stable_start;
}

// Restore extended sleep state from RTC memory
// Returns true if RTC memory was valid, false if power cycle
bool extendedSleepRestoreFromRTC() {
    if (rtc_extended_sleep_magic != RTC_EXTENDED_SLEEP_MAGIC) {
        Serial.println("Extended sleep: RTC memory invalid (power cycle)");
        // Reset to defaults
        g_in_extended_sleep_mode = false;
        g_time_since_stable_start = millis();
        return false;
    }

    Serial.println("Extended sleep: Restoring state from RTC memory");
    g_in_extended_sleep_mode = rtc_in_extended_sleep_mode;
    g_time_since_stable_start = rtc_time_since_stable_start;

    Serial.print("  in_extended_sleep_mode: ");
    Serial.println(g_in_extended_sleep_mode ? "true" : "false");
    Serial.print("  time_since_stable_start: ");
    Serial.println(g_time_since_stable_start);

    return true;
}

// Enter extended deep sleep mode (tap wake instead of motion wake)
void enterExtendedDeepSleep() {
    Serial.println("Entering extended deep sleep (double-tap wake)...");

#if EXTENDED_SLEEP_INDICATOR
    // Only show backpack mode screen on first entry
    if (!rtc_backpack_screen_shown) {
        Serial.println("Displaying Backpack Mode screen...");
        displayBackpackMode();
        rtc_backpack_screen_shown = true;
        delay(1000);  // Brief pause to ensure display updates
    } else {
        Serial.println("Backpack Mode screen already shown - skipping display refresh");
    }
#endif

    // Stop BLE advertising before sleep (Plan 034: awake = advertising, asleep = not)
#if ENABLE_BLE
    bleStopAdvertising();
#endif

    // Record entering extended sleep for activity tracking
    activityStatsRecordExtendedSleep();

    // Save state to RTC memory before sleeping
    displaySaveToRTC();
    drinksSaveToRTC();
    extendedSleepSaveToRTC();
    activityStatsSaveToRTC();

    // Configure for tap wake (replaces timer wake for battery efficiency)
    configureADXL343TapWake();
    rtc_tap_wake_enabled = true;

    // Add periodic health-check timer (ESP32 supports multiple wake sources)
    uint64_t timer_us = (uint64_t)HEALTH_CHECK_WAKE_INTERVAL_SEC * 1000000ULL;
    esp_sleep_enable_timer_wakeup(timer_us);
    rtc_health_check_wake = true;
    Serial.printf("Health-check timer set: %d seconds\n", HEALTH_CHECK_WAKE_INTERVAL_SEC);

    Serial.println("Entering extended sleep - wake on double-tap or health-check timer");
    Serial.flush();

    // Configure tap interrupt as wake source (alongside timer)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_ACCEL_INT, 1);  // Wake on HIGH

    // Enter deep sleep
    esp_deep_sleep_start();
}

void enterDeepSleep() {
    Serial.println("Entering normal deep sleep (motion wake)...");

    // Record entering normal sleep for activity tracking
    activityStatsRecordNormalSleep();

    // Save state to RTC memory before sleeping
    displaySaveToRTC();
    drinksSaveToRTC();
    extendedSleepSaveToRTC();
    activityStatsSaveToRTC();

    // CRITICAL FIX: Ensure ADXL343 interrupt is cleared before sleeping
    // Wait for bottle to return upright (|Y| > 0.81g) so interrupt clears
    if (adxlReady) {
        Serial.println("Checking ADXL343 interrupt state before sleep...");
        int attempts = 0;
        while (digitalRead(PIN_ACCEL_INT) == HIGH && attempts < 50) {
            Serial.print("  INT pin HIGH (bottle still tilted) - waiting for upright... ");

            // Read current orientation
            int16_t x, y, z;
            adxl.getXYZ(x, y, z);
            float y_g = y / 256.0f;
            Serial.print("Y=");
            Serial.print(y_g, 3);
            Serial.println("g");

            delay(100);
            attempts++;
        }

        // Read INT_SOURCE to clear any pending interrupt (ADXL343 auto-clears on read)
        uint8_t int_source = readAccelReg(0x30);

        int pin_state = digitalRead(PIN_ACCEL_INT);
        Serial.print("  INT pin final state: ");
        Serial.println(pin_state ? "HIGH (WARNING!)" : "LOW (ready)");

        if (pin_state == HIGH) {
            Serial.println("  WARNING: INT pin still HIGH - may not wake properly!");
        }
    }

    Serial.flush();

    // Configure wake-up interrupt from ADXL343 INT1 pin
    // Wake on HIGH level (interrupt pin goes HIGH when tilted)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_ACCEL_INT, 1);

    // Configure timer wake: use minimum of rollover time and health-check interval
    uint32_t timer_seconds = HEALTH_CHECK_WAKE_INTERVAL_SEC;
    rtc_health_check_wake = true;

    uint32_t seconds_until_rollover = getSecondsUntilRollover();
    if (seconds_until_rollover > 0 && seconds_until_rollover < 24 * 3600) {
        uint32_t rollover_with_buffer = seconds_until_rollover + 60;
        if (rollover_with_buffer < timer_seconds) {
            timer_seconds = rollover_with_buffer;
            rtc_health_check_wake = false;  // This is a rollover wake
        }
        rtc_rollover_wake_pending = true;
    } else {
        rtc_rollover_wake_pending = false;
    }

    uint64_t timer_us = (uint64_t)timer_seconds * 1000000ULL;
    esp_sleep_enable_timer_wakeup(timer_us);
    Serial.printf("Sleep timer set: %lu seconds (%s)\n", timer_seconds,
                  rtc_health_check_wake ? "health check" : "rollover");

    // Enter deep sleep
    esp_deep_sleep_start();
}

void blinkLED(int durationSeconds) {
    Serial.print("Blinking LED for ");
    Serial.print(durationSeconds);
    Serial.println(" seconds...");

    unsigned long endTime = millis() + (durationSeconds * 1000);
    while (millis() < endTime) {
        digitalWrite(PIN_LED, HIGH);
        delay(500);
        digitalWrite(PIN_LED, LOW);
        delay(500);
    }
    Serial.println("LED blink complete!");
}

#if defined(BOARD_ADAFRUIT_FEATHER)
// Drawing functions - most moved to display.cpp
// Keep drawWelcomeScreen here as it's a one-time boot screen

// Helper: Format time for display (needed for setup())
void formatTimeForDisplay(char* buffer, size_t buffer_size) {
    if (!g_time_valid) {
        snprintf(buffer, buffer_size, "--- --");
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec + (g_timezone_offset * 3600);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    const char* day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "---"};
    const char* day = (timeinfo.tm_wday >= 0 && timeinfo.tm_wday <=  6) ?
                      day_names[timeinfo.tm_wday] : day_names[7];

    int hour_12 = timeinfo.tm_hour % 12;
    if (hour_12 == 0) hour_12 = 12;
    const char* am_pm = (timeinfo.tm_hour < 12) ? "am" : "pm";

    snprintf(buffer, buffer_size, "%s %d%s", day, hour_12, am_pm);
}

void drawWelcomeScreen() {
    // Use water drop bitmap from display.cpp
    extern const unsigned char water_drop_bitmap[] PROGMEM;
    const int WATER_DROP_WIDTH = 60;
    const int WATER_DROP_HEIGHT = 60;

    Serial.println("Drawing welcome screen...");
    display.clearBuffer();
    display.setTextColor(EPD_BLACK);

    display.setTextSize(3);
    display.setCursor(20, 50);
    display.print("Aquavate");

    int drop_x = 180;
    int drop_y = 30;
    display.drawBitmap(drop_x, drop_y, water_drop_bitmap,
                      WATER_DROP_WIDTH, WATER_DROP_HEIGHT, EPD_BLACK);

    display.display();
}

// Helper function to force display refresh (called by serial commands like TARE)
void forceDisplayRefresh() {
#if defined(BOARD_ADAFRUIT_FEATHER)
    if (!nauReady || !g_calibrated) {
        Serial.println("Display refresh skipped - NAU7802 not ready or not calibrated");
        return;
    }

    // Read current sensor values
    int32_t current_adc = nau.read();
    float water_ml = calibrationGetWaterWeight(current_adc, g_calibration);

    // Get current daily total (computed from records)
    uint16_t daily_total = drinksGetDailyTotal();

    // Get current time
    uint8_t time_hour = 0, time_minute = 0;
    if (g_time_valid) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec + (g_timezone_offset * 3600);
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        time_hour = timeinfo.tm_hour;
        time_minute = timeinfo.tm_min;
    }

    // Get battery percent
    uint8_t battery_pct = 50;
    float voltage = getBatteryVoltage();
    battery_pct = getBatteryPercent(voltage);

    Serial.println("Forcing display refresh...");
    displayForceUpdate(water_ml, daily_total,
                      time_hour, time_minute, battery_pct, false);
#endif
}
#endif

void setup() {
    Serial.begin(115200);
    delay(1000);

    wakeTime = millis();

    // Initialize LED
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    // Print wake reason with detailed diagnostics
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    Serial.println("=================================");
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Woke up from EXT0 (tilt/motion interrupt!)");
            Serial.print("GPIO ");
            Serial.print(PIN_ACCEL_INT);
            Serial.println(" triggered wake");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println(rtc_health_check_wake ? "Woke up from timer (health check)" : "Woke up from timer (rollover)");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            Serial.println("Not from deep sleep (power on/reset/upload)");
            break;
        default:
            Serial.print("Woke up from unknown cause: ");
            Serial.println(wakeup_reason);
            break;
    }
    Serial.println("=================================");

    // Handle extended sleep mode wake logic
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        if (rtc_tap_wake_enabled) {
            // Woke from double-tap in backpack mode - exit backpack mode
            Serial.println("=== TAP WAKE from backpack mode ===");
            g_in_extended_sleep_mode = false;
            g_time_since_stable_start = millis();
            rtc_backpack_screen_shown = false;
            g_force_display_clear_sleep = true;
            // rtc_tap_wake_enabled cleared after accel reconfig below
        } else {
            // Woke from motion - was in normal sleep, return to normal mode
            Serial.println("Motion wake detected, returning to normal mode");
            g_in_extended_sleep_mode = false;
            g_time_since_stable_start = millis();
            rtc_backpack_screen_shown = false;
            // Current gesture checked at sleep time (no per-cycle flag needed)
        }
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        if (rtc_health_check_wake) {
            Serial.println("Timer wake detected (health check)");
            // Health check: go through normal boot, will auto-sleep after inactivity timeout
            if (rtc_tap_wake_enabled) {
                // Was in extended sleep - stay in backpack mode context
                Serial.println("  (from backpack mode - will re-evaluate)");
            }
            g_time_since_stable_start = millis();
        } else {
            Serial.println("Timer wake detected (daily rollover)");
        }
    } else {
        // Power on/reset - initialize continuous awake tracking
        g_time_since_stable_start = millis();
        g_in_extended_sleep_mode = false;
        rtc_backpack_screen_shown = false;
        rtc_tap_wake_enabled = false;
    }

#if defined(BOARD_ADAFRUIT_FEATHER)
    Serial.printf("Aquavate v%d.%d.%d | Adafruit ESP32 Feather V2\n",
                  AQUAVATE_VERSION_MAJOR, AQUAVATE_VERSION_MINOR, AQUAVATE_VERSION_PATCH);
#elif defined(BOARD_SPARKFUN_QWIIC)
    Serial.printf("Aquavate v%d.%d.%d | SparkFun ESP32-C6 Qwiic Pocket\n",
                  AQUAVATE_VERSION_MAJOR, AQUAVATE_VERSION_MINOR, AQUAVATE_VERSION_PATCH);
#endif

    // Initialize E-Paper display
#if defined(BOARD_ADAFRUIT_FEATHER)
    display.begin();
    display.setRotation(2);  // Rotate 180 degrees

    // Immediate visual feedback for tap wake (clear backpack mode screen right away)
    // This runs before sensor initialization to give instant user feedback
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 && rtc_tap_wake_enabled) {
        displayInit(display);  // Initialize display module first
        displayTapWakeFeedback();
    }

    // Draw welcome screen only on power-on/reset (not on wake from sleep)
    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT0 && wakeup_reason != ESP_SLEEP_WAKEUP_TIMER) {
        drawWelcomeScreen();
    }

    // Print battery info
    float batteryV = getBatteryVoltage();
    int batteryPct = getBatteryPercent(batteryV);
    Serial.printf("Battery: %.2fV (%d%%)\n", batteryV, batteryPct);

    Serial.println("E-Paper: OK");
#endif

    // Initialize I2C and NAU7802
    Wire.begin();
    if (nau.begin()) {
        nau.setLDO(NAU7802_3V3);
        nau.setGain(NAU7802_GAIN_128);
        nau.setRate(NAU7802_RATE_10SPS);
        nauReady = true;
        Serial.println("NAU7802: OK");
    } else {
        Serial.println("NAU7802: FAILED");
    }

    // Initialize ADXL343 accelerometer
    if (adxl.begin(I2C_ADDR_ADXL343)) {
        adxl.setRange(ADXL343_RANGE_2_G);
        adxl.setDataRate(ADXL343_DATARATE_12_5_HZ);  // Closest to 10 Hz
        adxlReady = true;

        // If we woke from EXT0, read current accelerometer state and clear interrupt
        if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
            DEBUG_PRINTLN(g_debug_accelerometer, "Checking accelerometer state after wake...");
            int16_t x, y, z;
            adxl.getXYZ(x, y, z);
            float x_g = x / 256.0f;
            float y_g = y / 256.0f;
            float z_g = z / 256.0f;
            DEBUG_PRINTF(g_debug_accelerometer, "  Current orientation: X=%.2fg Y=%.2fg Z=%.2fg\n", x_g, y_g, z_g);

            // Read INT_SOURCE to clear the interrupt (ADXL343 auto-clears, but read for diagnostics)
            uint8_t int_source = readAccelReg(0x30);  // INT_SOURCE register
            DEBUG_PRINTF(g_debug_accelerometer, "  INT_SOURCE: 0x%02X (cleared)\n", int_source);
        }

        // Configure interrupt for wake-on-tilt (or restore after tap wake)
        configureADXL343Interrupt();

        // Clear tap wake flag after restoring motion detection
        if (rtc_tap_wake_enabled) {
            DEBUG_PRINTLN(g_debug_accelerometer, "Restored motion detection after tap wake");
            rtc_tap_wake_enabled = false;
        }
    } else {
        Serial.println("ADXL343: FAILED");
        rtc_tap_wake_enabled = false;  // Clear flag even if accel failed
    }

    // Initialize DS3231 RTC
    if (!rtc.begin()) {
        Serial.println("DS3231: not detected (using ESP32 RTC)");
        g_rtc_ds3231_present = false;
    } else {
        g_rtc_ds3231_present = true;

        // Sync ESP32 RTC from DS3231 on every boot/wake
        DateTime now = rtc.now();
        struct timeval tv = {
            .tv_sec = static_cast<time_t>(now.unixtime()),
            .tv_usec = 0
        };
        settimeofday(&tv, NULL);
        g_time_valid = true;  // DS3231 is always valid (battery-backed)

        Serial.printf("DS3231: OK (synced %04d-%02d-%02d %02d:%02d:%02d)\n",
                      now.year(), now.month(), now.day(),
                      now.hour(), now.minute(), now.second());
    }

    // Initialize calibration system and storage (NVS for calibration/settings)
    if (storageInit()) {

        // Initialize LittleFS for drink record storage
        if (!storageInitDrinkFS()) {
            Serial.println("WARNING: Drink storage (LittleFS) initialization failed");
        }

        // Load calibration from NVS
        g_calibrated = storageLoadCalibration(g_calibration);

        if (g_calibrated) {
            Serial.printf("Calibration: valid (scale=%.2f)\n", g_calibration.scale_factor);
        } else {
            Serial.println("Calibration: not found - calibration required");
        }

        // Load timezone and time_valid from NVS
        g_timezone_offset = storageLoadTimezone();
        g_time_valid = storageLoadTimeValid();

        // DS3231 overrides NVS time_valid - battery-backed time is always valid
        if (g_rtc_ds3231_present) {
            g_time_valid = true;
            DEBUG_PRINTLN(g_debug_calibration, "Time valid: true (DS3231 battery-backed)");
        }

        // Load display mode from NVS
        g_daily_intake_display_mode = storageLoadDisplayMode();
        DEBUG_PRINTF(g_debug_calibration, "Display mode loaded: %d (%s)\n",
                     g_daily_intake_display_mode,
                     (g_daily_intake_display_mode == 0) ? "Human figure" : "Tumbler grid");

        // Load sleep timeout from NVS
        uint32_t sleep_timeout_seconds = storageLoadSleepTimeout();
        g_sleep_timeout_ms = sleep_timeout_seconds * 1000;
        if (sleep_timeout_seconds == 0) {
            DEBUG_PRINTLN(g_debug_calibration, "Sleep timeout: DISABLED (debug mode)");
        } else {
            DEBUG_PRINTF(g_debug_calibration, "Sleep timeout: %u seconds\n", sleep_timeout_seconds);
        }

        // Load extended sleep threshold from NVS (tap-to-wake replaced timer wake)
        g_time_since_stable_threshold_sec = storageLoadExtendedSleepThreshold();
        DEBUG_PRINTF(g_debug_calibration, "Extended sleep threshold: %u seconds\n", g_time_since_stable_threshold_sec);

        if (g_time_valid) {
            // Only restore from NVS on cold boot (not wake from deep sleep)
            // On wake from deep sleep, ESP32 RTC continues running and has correct time
            if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
                // Cold boot - restore from NVS (RTC was reset)
                // Skip if DS3231 is present (already synced from DS3231)
                if (!g_rtc_ds3231_present) {
                    uint32_t last_boot_time = storageLoadLastBootTime();
                    if (last_boot_time > 0) {
                        struct timeval tv;
                        tv.tv_sec = last_boot_time;
                        tv.tv_usec = 0;
                        settimeofday(&tv, NULL);
                        DEBUG_PRINTLN(g_debug_calibration, "  RTC restored from NVS (cold boot)");
                        Serial.println("WARNING: Time may be inaccurate (ESP32 internal RTC drift)");
                    }
                } else {
                    DEBUG_PRINTLN(g_debug_calibration, "  RTC synced from DS3231 (cold boot)");
                }
            } else {
                DEBUG_PRINTLN(g_debug_calibration, "  RTC time preserved (wake from deep sleep)");
            }

            // Show current time as single summary line
            char time_str[64];
            formatTimeForDisplay(time_str, sizeof(time_str));
            Serial.printf("Time: %s (UTC%+d, %s)\n", time_str, g_timezone_offset,
                          g_rtc_ds3231_present ? "DS3231" : "ESP32");
        } else {
            Serial.println("WARNING: Time not set!");
            Serial.println("Use SET_DATETIME command to set time");
        }
    } else {
        Serial.println("Storage initialization failed");
    }

    // Initialize serial command handler (conditional)
#if ENABLE_SERIAL_COMMANDS
    serialCommandsInit();
    serialCommandsSetTimeCallback(onTimeSet);
    DEBUG_PRINTLN(g_debug_calibration, "Serial command handler initialized");
#endif

    // Initialize gesture detection
    if (adxlReady) {
        gesturesInit(adxl);
        DEBUG_PRINTLN(g_debug_calibration, "Gesture detection initialized");
    }

    // Initialize weight measurement
    if (nauReady) {
        weightInit(nau);
        DEBUG_PRINTLN(g_debug_calibration, "Weight measurement initialized");
    }

    // Handle timer wake (rollover or extended sleep)
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // Check if this is a rollover wake (4am daily reset)
        bool is_rollover_wake = false;
        if (rtc_rollover_wake_pending) {
            rtc_rollover_wake_pending = false;  // Clear flag

            // Verify we're near 4am (within 10 minutes after rollover time)
            if (g_time_valid) {
                uint32_t current_time = getCurrentUnixTime();
                time_t current_t = current_time;
                struct tm current_tm;
                gmtime_r(&current_t, &current_tm);

                // Check if within 10 minutes after 4am
                if (current_tm.tm_hour == DRINK_DAILY_RESET_HOUR && current_tm.tm_min <= 10) {
                    is_rollover_wake = true;
                    Serial.println("=== DAILY ROLLOVER WAKE ===");
                    Serial.printf("Time: %02d:%02d - Refreshing display with reset daily total\n",
                                  current_tm.tm_hour, current_tm.tm_min);
                }
            }
        }

        if (is_rollover_wake) {
            // Rollover wake: refresh display and return to sleep
            // Note: Display and drinks will be initialized below, then we handle rollover refresh
            // Set flag to process rollover after all initialization is complete
            g_rollover_wake_pending = true;
            g_in_extended_sleep_mode = false;
            g_time_since_stable_start = millis();  // Reset tracking
            rtc_backpack_screen_shown = false;  // Reset for next backpack mode entry

            // Set flag to force display update to clear backpack mode screen
            g_force_display_clear_sleep = true;
            Serial.println("Rollover wake: Will update display after init");
        } else {
            // Unexpected timer wake (not rollover) - just continue normally
            Serial.println("Timer wake but not rollover - continuing normally");
            g_in_extended_sleep_mode = false;
            g_time_since_stable_start = millis();
        }
    }

    // Initialize calibration state machine
#if ENABLE_STANDALONE_CALIBRATION
    calibrationInit();
    DEBUG_PRINTLN(g_debug_calibration, "Calibration state machine initialized");

    // Initialize calibration UI
#if defined(BOARD_ADAFRUIT_FEATHER)
    uiCalibrationInit(display);
    DEBUG_PRINTLN(g_debug_calibration, "Calibration UI initialized");

    // Show calibration status on display if not calibrated
    if (!g_calibrated) {
        display.clearBuffer();
        display.setTextSize(2);
        display.setTextColor(EPD_BLACK);
        display.setCursor(20, 30);
        display.print("Calibration");
        display.setCursor(40, 55);
        display.print("Required");
        display.setTextSize(1);
        display.setCursor(10, 85);
        display.print("Hold bottle inverted");
        display.setCursor(10, 100);
        display.print("for 5 seconds");
        display.display();
    }
#endif
#endif // ENABLE_STANDALONE_CALIBRATION

    // Initialize drink tracking system (only if time is valid)
    if (g_time_valid) {
        drinksInit();
        DEBUG_PRINTLN(g_debug_drink_tracking, "Drink tracking system initialized");

        // Dump buffer metadata for diagnostics
        CircularBufferMetadata meta;
        if (storageLoadBufferMetadata(meta)) {
            DEBUG_PRINTF(g_debug_drink_tracking, "\n=== DRINK BUFFER STATUS (LittleFS) ===\n");
            DEBUG_PRINTF(g_debug_drink_tracking, "Record count: %d / %d (max)\n", meta.record_count, DRINK_MAX_RECORDS);
            DEBUG_PRINTF(g_debug_drink_tracking, "Write index: %d\n", meta.write_index);
            DEBUG_PRINTF(g_debug_drink_tracking, "Total writes: %u\n", meta.total_writes);
            DEBUG_PRINTF(g_debug_drink_tracking, "Next record ID: %u\n", meta.next_record_id);
            DEBUG_PRINTF(g_debug_drink_tracking, "======================================\n\n");
        }
    } else {
        Serial.println("WARNING: Drink tracking not initialized - time not set");
    }

    // Initialize display state tracking module
#if defined(BOARD_ADAFRUIT_FEATHER)
    displayInit(display);

    // Try to restore drink baseline from RTC on ALL boot types.
    // RTC memory survives EN-pin resets (e.g. serial monitor reconnect).
    // The magic number check inside drinksRestoreFromRTC() handles true
    // power cycles (RTC cleared → magic invalid → returns false, NVS fallback used).
    bool drinks_restored = drinksRestoreFromRTC();
    if (drinks_restored) {
        Serial.println("Drinks: Baseline restored from RTC");
    } else {
        Serial.println("Drinks: No RTC baseline (power cycle) - using NVS fallback");
    }

    // Restore other state from RTC memory if waking from deep sleep
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        bool display_restored = displayRestoreFromRTC();
        bool activity_restored = activityStatsRestoreFromRTC();

        if (display_restored && drinks_restored) {
            DEBUG_PRINTLN(g_debug_display, "State restored from RTC memory (wake from sleep)");
            // Mark display as initialized to prevent unnecessary updates
            displayMarkInitialized();
        } else {
            DEBUG_PRINTLN(g_debug_display, "No valid RTC state (power cycle) - will force display update");
        }

        // Record wake event for activity tracking
        if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
            activityStatsRecordWakeStart(WAKE_REASON_MOTION);

            // If returning from backpack mode via tap wake, immediately refresh display
            if (g_force_display_clear_sleep && nauReady) {
                DEBUG_PRINTLN(g_debug_display, "Tap wake: Immediately refreshing display after exiting backpack mode");
                g_force_display_clear_sleep = false;

                // Get current water level and daily total
                int32_t raw_adc = weightReadRaw();
                float water_ml = calibrationGetWaterWeight(raw_adc, g_calibration);
                if (water_ml < 0) water_ml = 0;
                if (water_ml > 830) water_ml = 830;
                uint16_t daily_total = drinksGetDailyTotal();

                // Get current time
                uint8_t hour = 0, minute = 0;
                if (g_time_valid) {
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    time_t now = tv.tv_sec + (g_timezone_offset * 3600);
                    struct tm timeinfo;
                    gmtime_r(&now, &timeinfo);
                    hour = timeinfo.tm_hour;
                    minute = timeinfo.tm_min;
                }

                // Get battery level
                float voltage = getBatteryVoltage();
                uint8_t battery_pct = getBatteryPercent(voltage);

                // Force display update with current values (not sleeping)
                displayForceUpdate(water_ml, daily_total, hour, minute, battery_pct, false);
            }
        } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
            if (rtc_health_check_wake) {
                // Health-check wake — record as visible individual event
                activityStatsRecordWakeStart(WAKE_REASON_TIMER);
            } else {
                activityStatsRecordTimerWake();
            }
        }
    } else {
        DEBUG_PRINTLN(g_debug_display, "Display state tracking initialized (power on/reset)");
        // Initialize fresh activity buffer on power cycle
        activityStatsInit();
        activityStatsRecordWakeStart(WAKE_REASON_POWER_ON);
    }

    // Note: Display will update on first stable check to ensure correct values shown
#endif

    // Initialize NVS (required by ESP-IDF BLE)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println("NVS: Erasing and reinitializing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize BLE service (conditional)
#if ENABLE_BLE
    if (bleInit()) {
        // Sync daily goal from BLE config to display
        displaySetDailyGoal(bleGetDailyGoalMl());

        // Start advertising on motion wake (not timer wake)
        if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
            Serial.println("BLE initialized (advertising)");
            bleStartAdvertising();
        } else if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
            Serial.println("BLE initialized (advertising)");
            bleStartAdvertising();
        } else {
            Serial.println("BLE initialized (not advertising - timer wake)");
        }
    } else {
        Serial.println("BLE: FAILED");
    }

    // Snapshot unsynced count at wake time
    // Only extend timeout if NEW drinks recorded during this wake session
    g_unsynced_at_wake = storageGetUnsyncedCount();
    Serial.printf("Unsynced records: %d\n", g_unsynced_at_wake);
#endif

    // Handle daily rollover wake (4am) - refresh display and return to sleep
    if (g_rollover_wake_pending) {
        g_rollover_wake_pending = false;

        Serial.println("Processing rollover wake...");

        // Recalculate daily totals (will be 0 after rollover)
        drinksRecalculateTotals();
        uint16_t daily_total = drinksGetDailyTotal();
        Serial.printf("Daily total after rollover: %d ml\n", daily_total);

#if defined(BOARD_ADAFRUIT_FEATHER)
        // Get current time for display
        uint8_t time_hour = DRINK_DAILY_RESET_HOUR;
        uint8_t time_minute = 0;
        if (g_time_valid) {
            uint32_t current_time = getCurrentUnixTime();
            time_t current_t = current_time;
            struct tm current_tm;
            gmtime_r(&current_t, &current_tm);
            time_hour = current_tm.tm_hour;
            time_minute = current_tm.tm_min;
        }

        // Get battery level
        float batteryV = getBatteryVoltage();
        int batteryPct = getBatteryPercent(batteryV);

        // Get last water level from RTC state (bottle hasn't moved)
        DisplayState last_state = displayGetState();
        float water_ml = last_state.water_ml;

        // Force display update showing reset daily total
        Serial.printf("Updating display: water=%.0f ml, daily=%d ml, time=%02d:%02d, battery=%d%%\n",
                      water_ml, daily_total, time_hour, time_minute, batteryPct);
        displayUpdate(water_ml, daily_total, time_hour, time_minute, batteryPct, false);
#endif

        // Return to deep sleep immediately (no user activity, no BLE)
        Serial.println("Rollover complete - returning to sleep");
        Serial.flush();
        delay(100);  // Ensure display update completes
        enterDeepSleep();
        // Will not return from deep sleep
    }

    Serial.printf("Setup complete! Activity timeout: %ds\n", ACTIVITY_TIMEOUT_MS / 1000);
}

void loop() {
    // Check for serial commands (conditional)
#if ENABLE_SERIAL_COMMANDS
    serialCommandsUpdate();
#endif

    // Update BLE service (conditional)
#if ENABLE_BLE
    bleUpdate();

    // Check for BLE data activity (sync, commands) and reset activity timeout
    // This ensures device stays awake during active BLE communication
    // Also reset extended sleep timer so 180s threshold doesn't fire during BLE sync
    // (Plan 067: prevents 180s backpack mode timer from firing during active BLE session)
    if (bleCheckDataActivity()) {
        wakeTime = millis();
        g_time_since_stable_start = millis();
    }
#endif

    // Handle BLE commands (Phase 3C - conditional)
#if ENABLE_BLE
    if (bleCheckResetDailyRequested()) {
        Serial.println("BLE Command: RESET_DAILY");
        drinksResetDaily();
        wakeTime = millis(); // Reset sleep timer
    }

    if (bleCheckClearHistoryRequested()) {
        Serial.println("BLE Command: CLEAR_HISTORY");
        drinksClearAll();
        wakeTime = millis(); // Reset sleep timer
    }

    // Note: SET_DAILY_TOTAL command deprecated - daily totals computed from records
    uint16_t setDailyValue;
    if (bleCheckSetDailyTotalRequested(setDailyValue)) {
        Serial.printf("BLE Command: SET_DAILY_TOTAL ignored (deprecated) - value was %dml\n", setDailyValue);
        // This command is no longer supported since daily totals are computed from records
        wakeTime = millis(); // Reset sleep timer
    }

    // Note: TARE_NOW command will be handled below in the weight reading section
#endif

    // FIX Bug #4: READ SENSORS ONCE - create snapshot for this loop iteration
    SensorSnapshot sensors;
    sensors.timestamp = millis();
    sensors.adc_reading = 0;
    sensors.water_ml = 0.0f;
    sensors.gesture = GESTURE_NONE;

    // Read load cell
    if (nauReady && nau.available()) {
        sensors.adc_reading = nau.read();
        if (g_calibrated) {
            sensors.water_ml = calibrationGetWaterWeight(sensors.adc_reading, g_calibration);
        }
    }

    // Handle BLE TARE command (must be after ADC read - conditional)
#if ENABLE_BLE
    if (bleCheckTareRequested() && g_calibrated && nauReady) {
        Serial.println("BLE Command: TARE_NOW");
        // Update empty bottle ADC to current reading (tare weight)
        g_calibration.empty_bottle_adc = sensors.adc_reading;
        // Recalculate full bottle ADC (empty + 830ml * scale_factor)
        g_calibration.full_bottle_adc = g_calibration.empty_bottle_adc + (int32_t)(830.0f * g_calibration.scale_factor);
        // Save updated calibration to NVS
        if (storageSaveCalibration(g_calibration)) {
            Serial.println("BLE Command: Tare complete, calibration updated");
        } else {
            Serial.println("BLE Command: Tare failed - could not save calibration");
        }
        wakeTime = millis(); // Reset sleep timer
    }
#endif

    // Read accelerometer and get gesture (ONCE)
    if (adxlReady) {
        sensors.gesture = gesturesUpdate(sensors.water_ml);

        // Check for hardware double-tap (ADXL343 INT_SOURCE bit 5)
        // Reading INT_SOURCE clears all interrupt flags - safe during awake mode
        // since the interrupt pin is only used as a wake source from deep sleep
        uint8_t int_source = readAccelReg(0x30);  // INT_SOURCE
        if (int_source & 0x20) {  // Bit 5 = DOUBLE_TAP
            sensors.gesture = GESTURE_DOUBLE_TAP;
            Serial.println("=== DOUBLE-TAP DETECTED (hardware) ===");
        }
    }

    // NOW use snapshot for all logic below
    int32_t current_adc = sensors.adc_reading;
    float current_water_ml = sensors.water_ml;
    GestureType gesture = sensors.gesture;

    // Handle shake-while-inverted gesture (shake to empty)
    // Can be disabled via iOS app settings (Issue #32)
    if (gesture == GESTURE_SHAKE_WHILE_INVERTED) {
#if ENABLE_BLE
        bool shake_enabled = bleGetShakeToEmptyEnabled();
#else
        bool shake_enabled = true;  // Always enabled when BLE disabled
#endif
        if (!shake_enabled) {
            // Shake-to-empty disabled via iOS app - ignore gesture
            // (logged once when gesture first detected via gestures.cpp)
        } else if (!g_cancel_drink_pending) {
            g_cancel_drink_pending = true;
            Serial.println("Main: Shake gesture detected - bottle emptied pending");
            // Reset extended sleep timer - shake is unambiguous user interaction
            g_time_since_stable_start = millis();
        }
    }

    // Handle double-tap gesture (manual extended deep sleep entry)
    if (gesture == GESTURE_DOUBLE_TAP) {
        bool sleep_blocked = false;

#if ENABLE_STANDALONE_CALIBRATION
        if (calibrationIsActive()) {
            Serial.println("Double-tap: Ignored - standalone calibration in progress");
            sleep_blocked = true;
        }
#endif
#if ENABLE_BLE
        if (bleIsCalibrationInProgress()) {
            Serial.println("Double-tap: Ignored - BLE calibration in progress");
            sleep_blocked = true;
        }
#endif

        if (!sleep_blocked && g_has_been_upright_stable) {
            Serial.println("=== DOUBLE-TAP → ENTERING BACKPACK MODE ===");
            g_in_extended_sleep_mode = true;
            enterExtendedDeepSleep();
            // Will not return from deep sleep
        } else if (!sleep_blocked && !g_has_been_upright_stable) {
            Serial.println("Double-tap: Ignored - bottle not yet placed on surface this wake cycle");
        }
    }

    // Handle bottle emptied when bottle returns to upright stable
    // Workflow: user pours out water → shakes while inverted → puts down
    // The shake signals "I emptied the bottle, don't count this as a drink"
    // We skip drinksUpdate() and just reset the baseline to prevent recording a false drink
    if (gesture == GESTURE_UPRIGHT_STABLE && g_cancel_drink_pending) {
        g_cancel_drink_pending = false;
        Serial.println("Main: Bottle emptied - skipping drink detection, resetting baseline");

#if defined(BOARD_ADAFRUIT_FEATHER)
        uiShowBottleEmptied(display);
        delay(3000);  // Show confirmation for 3 seconds

        // Get current values for display update
        uint16_t daily_total = drinksGetDailyTotal();

        uint8_t time_hour = 0, time_minute = 0;
        if (g_time_valid) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            time_t now = tv.tv_sec + (g_timezone_offset * 3600);
            struct tm timeinfo;
            gmtime_r(&now, &timeinfo);
            time_hour = timeinfo.tm_hour;
            time_minute = timeinfo.tm_min;
        }

        uint8_t battery_pct = getBatteryPercent(getBatteryVoltage());

        displayForceUpdate(current_water_ml, daily_total,
                          time_hour, time_minute, battery_pct, false);
#endif
        // Reset baseline to current level - this prevents drinksUpdate() from detecting a drink
        drinksResetBaseline(current_adc);
    }

    // Check calibration state (used in display logic even if standalone calibration disabled)
#if ENABLE_STANDALONE_CALIBRATION
    CalibrationState cal_state = calibrationGetState();
#else
    CalibrationState cal_state = CAL_IDLE;  // Always idle when standalone calibration disabled
#endif

#if ENABLE_STANDALONE_CALIBRATION
    // Check for BLE calibration start request (Plan 060 - Bottle-Driven iOS Calibration)
#if ENABLE_BLE
    if (bleCheckCalibrationStartRequested()) {
        if (cal_state == CAL_IDLE) {
            Serial.println("Main: BLE calibration start requested");
            calibrationInit();
            calibrationStart();
            cal_state = calibrationGetState();
            bleNotifyCalibrationState();  // Notify iOS of initial state
        }
    }

    // Check for BLE calibration cancel request
    if (bleCheckCalibrationCancelRequested()) {
        if (calibrationIsActive()) {
            Serial.println("Main: BLE calibration cancel requested");
            calibrationCancel();
            cal_state = CAL_IDLE;
            g_last_cal_state = CAL_IDLE;
#if defined(BOARD_ADAFRUIT_FEATHER)
            // For BLE cancel, skip "Calibration Aborted" screen and go straight to main screen
            // (iOS provides the cancel feedback - user is watching their phone, not the bottle)
            float water_ml = 0.0f;
            if (nauReady && nau.available()) {
                int32_t adc = nau.read();
                water_ml = calibrationGetWaterWeight(adc, g_calibration);
                if (water_ml < 0) water_ml = 0;
                if (water_ml > 830) water_ml = 830;
            }

            uint16_t daily_total = drinksGetDailyTotal();

            uint8_t time_hour = 0, time_minute = 0;
            if (g_time_valid) {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                time_t now = tv.tv_sec + (g_timezone_offset * 3600);
                struct tm timeinfo;
                gmtime_r(&now, &timeinfo);
                time_hour = timeinfo.tm_hour;
                time_minute = timeinfo.tm_min;
            }

            uint8_t battery_pct = 50;
            float voltage = getBatteryVoltage();
            battery_pct = getBatteryPercent(voltage);

            displayForceUpdate(water_ml, daily_total,
                              time_hour, time_minute, battery_pct, false);
#endif
            bleNotifyCalibrationState();  // Notify iOS of cancelled state
            wakeTime = millis();  // Reset sleep timer
        }
    }
#endif

    // If not in calibration mode, check for calibration trigger
    if (cal_state == CAL_IDLE) {
        // Check for calibration trigger gesture (hold inverted for 5s)
        // But don't re-trigger immediately after an abort - require gesture release first
        if (gesture == GESTURE_INVERTED_HOLD) {
            if (!g_cal_just_cancelled) {
                Serial.println("Main: Calibration triggered!");
                calibrationStart();
                cal_state = calibrationGetState();
#if ENABLE_BLE
                bleNotifyCalibrationState();  // Notify iOS if connected
#endif
            }
        } else {
            // Gesture released - allow new calibration trigger
            g_cal_just_cancelled = false;
        }
    }
#endif

    // If calibrated, show water level (always available, not just in standalone calibration mode)
    if (g_debug_enabled && g_debug_water_level && g_calibrated && nauReady) {
        float water_ml = calibrationGetWaterWeight(current_adc, g_calibration);
        Serial.print("Water level: ");
        Serial.print(water_ml);
        Serial.println(" ml");
    }

#if ENABLE_STANDALONE_CALIBRATION
    // If in calibration mode, update state machine
    if (calibrationIsActive()) {
        CalibrationState new_state = calibrationUpdate(gesture, current_adc);

        // Update UI when state changes
        if (new_state != g_last_cal_state) {
            Serial.print("Main: Calibration state changed: ");
            Serial.print(calibrationGetStateName(g_last_cal_state));
            Serial.print(" -> ");
            Serial.println(calibrationGetStateName(new_state));

            // Update display for new state
#if defined(BOARD_ADAFRUIT_FEATHER)
            int32_t display_adc = 0;
            if (new_state == CAL_CONFIRM_EMPTY) {
                CalibrationResult result = calibrationGetResult();
                display_adc = result.data.empty_bottle_adc;
            } else if (new_state == CAL_CONFIRM_FULL) {
                CalibrationResult result = calibrationGetResult();
                display_adc = result.data.full_bottle_adc;
            }

            CalibrationResult result = calibrationGetResult();
            uiCalibrationUpdateForState(new_state, display_adc, result.data.scale_factor);
#endif

            // Notify iOS of state change (Plan 060 - Bottle-Driven iOS Calibration)
#if ENABLE_BLE
            bleNotifyCalibrationState();
#endif

            // Check for calibration error BEFORE updating g_last_cal_state
            if (new_state == CAL_ERROR) {
                CalibrationResult result = calibrationGetResult();
                Serial.print("Main: Calibration error: ");
                Serial.println(result.error_message);

                // Show error screen for 3 seconds (matches other info screens)
                delay(CAL_STARTED_DISPLAY_DURATION);

                // Return to IDLE and redraw main screen
                calibrationCancel();
                g_last_cal_state = CAL_IDLE;
                wakeTime = millis();  // Reset sleep timer - give user 30 more seconds
                Serial.println("Main: Returning to main screen after error");
#if defined(BOARD_ADAFRUIT_FEATHER)
                // Get current values for force update
                float water_ml = 0.0f;
                if (nauReady && nau.available()) {
                    int32_t adc = nau.read();
                    water_ml = calibrationGetWaterWeight(adc, g_calibration);
                    if (water_ml < 0) water_ml = 0;
                    if (water_ml > 830) water_ml = 830;
                }

                uint16_t daily_total = drinksGetDailyTotal();

                uint8_t time_hour = 0, time_minute = 0;
                if (g_time_valid) {
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    time_t now = tv.tv_sec + (g_timezone_offset * 3600);
                    struct tm timeinfo;
                    gmtime_r(&now, &timeinfo);
                    time_hour = timeinfo.tm_hour;
                    time_minute = timeinfo.tm_min;
                }

                uint8_t battery_pct = 50;
                float voltage = getBatteryVoltage();
                battery_pct = getBatteryPercent(voltage);

                displayForceUpdate(water_ml, daily_total,
                                  time_hour, time_minute, battery_pct, false);
#endif
                return;  // Exit early - don't update g_last_cal_state
            }

            // Check if calibration was aborted (returned to IDLE with inverted hold)
            if (new_state == CAL_IDLE && gesture == GESTURE_INVERTED_HOLD) {
                Serial.println("Main: Calibration aborted - showing aborted screen");
                g_cal_just_cancelled = true;
                g_last_cal_state = CAL_IDLE;

#if defined(BOARD_ADAFRUIT_FEATHER)
                // Show "Calibration Aborted" screen for 3 seconds
                uiCalibrationShowAborted();
                delay(CAL_STARTED_DISPLAY_DURATION);

                // Then redraw main screen
                float water_ml = 0.0f;
                if (nauReady && nau.available()) {
                    int32_t adc = nau.read();
                    water_ml = calibrationGetWaterWeight(adc, g_calibration);
                    if (water_ml < 0) water_ml = 0;
                    if (water_ml > 830) water_ml = 830;
                }

                uint16_t daily_total = drinksGetDailyTotal();

                uint8_t time_hour = 0, time_minute = 0;
                if (g_time_valid) {
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    time_t now = tv.tv_sec + (g_timezone_offset * 3600);
                    struct tm timeinfo;
                    gmtime_r(&now, &timeinfo);
                    time_hour = timeinfo.tm_hour;
                    time_minute = timeinfo.tm_min;
                }

                uint8_t battery_pct = 50;
                float voltage = getBatteryVoltage();
                battery_pct = getBatteryPercent(voltage);

                displayForceUpdate(water_ml, daily_total,
                                  time_hour, time_minute, battery_pct, false);
#endif
                return;  // Exit early
            }

            // Check if calibration completed
            if (new_state == CAL_COMPLETE) {
                CalibrationResult result = calibrationGetResult();
                if (result.success) {
                    Serial.println("Main: Calibration completed successfully!");
                    g_calibration = result.data;
                    g_calibrated = true;

                    // Show complete screen for 3 seconds (matches other info screens)
                    delay(CAL_STARTED_DISPLAY_DURATION);

                    // Return to IDLE and redraw main screen
                    calibrationCancel();
                    g_last_cal_state = CAL_IDLE;
                    wakeTime = millis();  // Reset sleep timer - give user 30 more seconds
                    Serial.println("Main: Returning to main screen");
#if defined(BOARD_ADAFRUIT_FEATHER)
                // Get current values for force update
                float water_ml = 0.0f;
                if (nauReady && nau.available()) {
                    int32_t adc = nau.read();
                    water_ml = calibrationGetWaterWeight(adc, g_calibration);
                    if (water_ml < 0) water_ml = 0;
                    if (water_ml > 830) water_ml = 830;
                }

                uint16_t daily_total = drinksGetDailyTotal();

                uint8_t time_hour = 0, time_minute = 0;
                if (g_time_valid) {
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    time_t now = tv.tv_sec + (g_timezone_offset * 3600);
                    struct tm timeinfo;
                    gmtime_r(&now, &timeinfo);
                    time_hour = timeinfo.tm_hour;
                    time_minute = timeinfo.tm_min;
                }

                uint8_t battery_pct = 50;
                float voltage = getBatteryVoltage();
                battery_pct = getBatteryPercent(voltage);

                displayForceUpdate(water_ml, daily_total,
                                  time_hour, time_minute, battery_pct, false);
#endif
                }
                return;  // Exit early - don't update g_last_cal_state
            }

            g_last_cal_state = new_state;
        }
    }
#endif // ENABLE_STANDALONE_CALIBRATION

    // Periodically check display state and update if needed (smart tracking)
    static unsigned long last_level_check = 0;
    static unsigned long last_time_check = 0;
    static unsigned long last_battery_check = 0;
    static bool interval_timers_initialized = false;
    static GestureType last_gesture = GESTURE_NONE;

    // Initialize interval timers on first loop after boot/wake
    // This prevents false "interval elapsed" triggers immediately after wake
    if (!interval_timers_initialized) {
        last_level_check = millis();
        last_time_check = millis();
        last_battery_check = millis();
        interval_timers_initialized = true;
    }

    // Deep sleep timer management - reset timer whenever gesture changes
    if (gesture != last_gesture) {
        wakeTime = millis();
        if (g_debug_enabled && g_debug_display) {
            Serial.print("Sleep timer: Reset (gesture changed to ");
            switch (gesture) {
                case GESTURE_INVERTED_HOLD: Serial.print("INVERTED_HOLD"); break;
                case GESTURE_UPRIGHT_STABLE: Serial.print("UPRIGHT_STABLE"); break;
                case GESTURE_SIDEWAYS_TILT: Serial.print("SIDEWAYS_TILT"); break;
                case GESTURE_UPRIGHT: Serial.print("UPRIGHT"); break;
                case GESTURE_SHAKE_WHILE_INVERTED: Serial.print("SHAKE_WHILE_INVERTED"); break;
                case GESTURE_DOUBLE_TAP: Serial.print("DOUBLE_TAP"); break;
                default: Serial.print("NONE"); break;
            }
            Serial.println(")");
        }
    }
    last_gesture = gesture;

    // Check display when bottle is upright stable OR if display not yet initialized
    // This ensures the display updates from splash screen even with negative weight
    bool display_not_initialized = false;
#if defined(BOARD_ADAFRUIT_FEATHER)
    DisplayState display_state = displayGetState();
    display_not_initialized = !display_state.initialized;
#endif

    if (cal_state == CAL_IDLE && g_calibrated &&
        (gesture == GESTURE_UPRIGHT_STABLE || display_not_initialized)) {

        // Check every DISPLAY_UPDATE_INTERVAL_MS when bottle is upright stable
        // OR immediately if display not initialized
        if (display_not_initialized || (millis() - last_level_check >= DISPLAY_UPDATE_INTERVAL_MS)) {
            last_level_check = millis();

#if defined(BOARD_ADAFRUIT_FEATHER)
            if (nauReady) {
                // FIX Bug #4: Use snapshot data instead of reading sensors again
                // Don't clamp negative values - display needs to see them to show "?" indicator
                float display_water_ml = current_water_ml;

                // Only clamp upper bound
                if (display_water_ml > 830) display_water_ml = 830;

                // Process drink tracking (we're already UPRIGHT_STABLE)
                // Only track drinks if weight is valid (>= -50ml threshold)
                if (g_time_valid && display_water_ml >= -50.0f) {
                    bool drink_recorded = drinksUpdate(current_adc, g_calibration);
                    if (drink_recorded) {
                        // Reset extended sleep timer - drink is unambiguous user interaction
                        g_time_since_stable_start = millis();
                    }
                }

                // Check interval timers for time and battery updates
                bool time_interval_elapsed = (millis() - last_time_check >= DISPLAY_TIME_UPDATE_INTERVAL_MS);
                bool battery_interval_elapsed = (millis() - last_battery_check >= DISPLAY_BATTERY_UPDATE_INTERVAL_MS);

                // Get current daily total (computed from records)
                uint16_t daily_total = drinksGetDailyTotal();

                // Get current time
                uint8_t time_hour = 0, time_minute = 0;
                if (g_time_valid) {
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    time_t now = tv.tv_sec + (g_timezone_offset * 3600);
                    struct tm timeinfo;
                    gmtime_r(&now, &timeinfo);
                    time_hour = timeinfo.tm_hour;
                    time_minute = timeinfo.tm_min;
                }

                // Get battery percent
                uint8_t battery_pct = 50;  // Default
                float voltage = getBatteryVoltage();
                battery_pct = getBatteryPercent(voltage);

                // Check for BLE force display refresh (e.g., after SET_DAILY_TOTAL command)
#if ENABLE_BLE
                bool ble_force_refresh = bleCheckForceDisplayRefresh();
#else
                bool ble_force_refresh = false;
#endif

                // Check if display needs update (water, daily intake, time, or battery changed)
                // OR if we need to clear Zzzz indicator after extended sleep
                // OR if BLE command requested a forced refresh
                if (g_force_display_clear_sleep || ble_force_refresh ||
                    displayNeedsUpdate(display_water_ml, daily_total,
                                      time_interval_elapsed, battery_interval_elapsed)) {

                    if (g_force_display_clear_sleep) {
                        Serial.println("Extended sleep: Clearing Zzzz indicator");
                        g_force_display_clear_sleep = false;
                    }
                    if (ble_force_refresh) {
                        Serial.println("BLE: Forced display refresh");
                    }

                    displayUpdate(display_water_ml, daily_total,
                                 time_hour, time_minute, battery_pct, false);

                    // Reset interval timers if they triggered the update
                    if (time_interval_elapsed) last_time_check = millis();
                    if (battery_interval_elapsed) last_battery_check = millis();
                }

                // Update BLE Current State (send notifications if connected - conditional)
#if ENABLE_BLE
                bleUpdateCurrentState(daily_total, current_adc, g_calibration,
                                    battery_pct, g_calibrated, g_time_valid,
                                    gesture == GESTURE_UPRIGHT_STABLE);

                // Update BLE battery level (separate characteristic)
                bleUpdateBatteryLevel(battery_pct);
#endif
            }
#endif
        }
    }

    // Check for BLE config changes that require display update (e.g., daily goal changed)
    // This check is OUTSIDE the gesture block so display updates even when bottle isn't upright
#if defined(BOARD_ADAFRUIT_FEATHER)
    if (g_calibrated && cal_state == CAL_IDLE && displayCheckGoalChanged()) {
        DEBUG_PRINTLN(g_debug_display, "Display: Goal changed via BLE - forcing update");
        DisplayState last_state = displayGetState();

        // Get current time
        uint8_t time_hour = 0, time_minute = 0;
        if (g_time_valid) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            time_t now = tv.tv_sec + (g_timezone_offset * 3600);
            struct tm timeinfo;
            gmtime_r(&now, &timeinfo);
            time_hour = timeinfo.tm_hour;
            time_minute = timeinfo.tm_min;
        }

        // Get battery percent
        float voltage = getBatteryVoltage();
        uint8_t battery_pct = getBatteryPercent(voltage);

        // Get daily total
        uint16_t daily_total = drinksGetDailyTotal();

        // Force display update with current values
        displayForceUpdate(last_state.water_ml, daily_total,
                          time_hour, time_minute, battery_pct, false);
    }
#endif

    // Send BLE updates during iOS calibration mode (even if bottle not yet calibrated)
    // This allows the iOS app to show real-time weight and stability during calibration
#if ENABLE_BLE
    if (bleIsCalibrationInProgress()) {
        static unsigned long last_cal_ble_update = 0;
        const unsigned long CAL_BLE_UPDATE_INTERVAL_MS = 500;  // Update every 500ms during calibration

        if (millis() - last_cal_ble_update >= CAL_BLE_UPDATE_INTERVAL_MS) {
            last_cal_ble_update = millis();

            // Get battery percent
            uint8_t battery_pct = 50;  // Default
            float voltage = getBatteryVoltage();
            battery_pct = getBatteryPercent(voltage);

            // Get daily total
            uint16_t daily_total = drinksGetDailyTotal();

            // Send BLE state update with current values
            // Note: During calibration, weight values may not be meaningful if not calibrated,
            // but stability flag (UPRIGHT_STABLE) is still useful
            bleUpdateCurrentState(daily_total, current_adc, g_calibration,
                                battery_pct, g_calibrated, g_time_valid,
                                gesture == GESTURE_UPRIGHT_STABLE);
        }
    }
#endif

    // Status line: gesture + mode + countdowns (always shown, every 3s)
    // Silenced by d0 (g_debug_enabled = false)
    if (g_debug_enabled) {
        static unsigned long last_status_print = 0;
        if (adxlReady && (millis() - last_status_print >= 3000)) {
            last_status_print = millis();

            // Gesture name
            Serial.print("Gesture: ");
            switch (gesture) {
                case GESTURE_INVERTED_HOLD: Serial.print("INVERTED_HOLD"); break;
                case GESTURE_UPRIGHT_STABLE: Serial.print("UPRIGHT_STABLE"); break;
                case GESTURE_SIDEWAYS_TILT: Serial.print("SIDEWAYS_TILT"); break;
                case GESTURE_SHAKE_WHILE_INVERTED: Serial.print("SHAKE_WHILE_INVERTED"); break;
                case GESTURE_DOUBLE_TAP: Serial.print("DOUBLE_TAP"); break;
                default: Serial.print("NONE"); break;
            }

            // Show mode and both timer countdowns
            // Modes: NORM=normal 30s, SYNC=waiting for NEW unsynced drinks 4min, EXT=backpack/timer wake
            Serial.print("  [");
#if ENABLE_BLE
            uint16_t unsynced = storageGetUnsyncedCount();
            bool has_new_unsynced = (unsynced > g_unsynced_at_wake);
#else
            uint16_t unsynced = 0;
            bool has_new_unsynced = false;
#endif
            if (g_in_extended_sleep_mode) {
                Serial.print("EXT");
            } else if (has_new_unsynced) {
                Serial.print("SYNC:");
                Serial.print(unsynced - g_unsynced_at_wake);  // Show NEW unsynced count
            } else {
                Serial.print("NORM");
                if (unsynced > 0) {
                    Serial.print(":");
                    Serial.print(unsynced);  // Show total unsynced (not extending timeout)
                }
            }
            Serial.print("]");

            // Activity timeout countdown (normal sleep)
            // Use the actual timeout that will be applied (extended only for NEW unsynced records)
            uint32_t effective_timeout = g_sleep_timeout_ms;
            if (has_new_unsynced) {
                effective_timeout = ACTIVITY_TIMEOUT_EXTENDED_MS;
            }
            if (effective_timeout > 0) {
                unsigned long elapsed = millis() - wakeTime;
                if (elapsed < effective_timeout) {
                    unsigned long remaining = (effective_timeout - elapsed) / 1000;
                    Serial.print(" act:");
                    Serial.print(remaining);
                    Serial.print("s");
                }
            }

            // Extended sleep timer countdown (time since last UPRIGHT_STABLE)
            unsigned long time_since_stable = (millis() - g_time_since_stable_start) / 1000;
            unsigned long ext_remaining = (time_since_stable < g_time_since_stable_threshold_sec)
                ? (g_time_since_stable_threshold_sec - time_since_stable) : 0;
            Serial.print(" ext:");
            Serial.print(ext_remaining);
            Serial.print("s");

            Serial.println();
        }
    }

    // Verbose accelerometer debug (only at d4+)
    if (g_debug_enabled && g_debug_accelerometer) {
        static unsigned long last_accel_print = 0;
        if (adxlReady && (millis() - last_accel_print >= 3000)) {
            last_accel_print = millis();

            float x, y, z;
            gesturesGetAccel(x, y, z);
            Serial.printf("Accel X: %.2fg  Y: %.2fg  Z: %.2fg  Cal State: %s\n",
                          x, y, z, calibrationGetStateName(cal_state));
        }
    }

    // Periodically save current timestamp to NVS (for time persistence across power cycles)
    // Only save if DS3231 RTC is not present (when DS3231 is added, this is unnecessary)
    // Save every hour on the hour to minimize NVS wear
    // With hourly interval: 24 writes/day → ~11 years flash lifespan
    if (g_time_valid && !g_rtc_ds3231_present) {
        static unsigned long last_time_save = 0;
        static int last_saved_hour = -1;

        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec + (g_timezone_offset * 3600);
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);

        // Save on hour boundaries (e.g., 14:00, 15:00, etc.)
        if (timeinfo.tm_hour != last_saved_hour && timeinfo.tm_min == 0) {
            storageSaveLastBootTime(tv.tv_sec);
            last_saved_hour = timeinfo.tm_hour;
            Serial.println("Time: Hourly timestamp saved to NVS");
        }
    }

    // Check for extended sleep trigger (backpack mode - continuous motion prevents normal sleep)
    // UPRIGHT_STABLE is an unambiguous user interaction - bottle placed on surface
    // Reset extended sleep timer whenever this occurs (not just after threshold)
    if (gesture == GESTURE_UPRIGHT_STABLE) {
        g_has_been_upright_stable = true;  // Enable double-tap backpack entry
        g_time_since_stable_start = millis();
    }

    if (!g_in_extended_sleep_mode
#if ENABLE_STANDALONE_CALIBRATION
        && !calibrationIsActive()
#endif
#if ENABLE_BLE
        && !bleIsCalibrationInProgress()
#endif
    ) {
        unsigned long total_awake_time = millis() - g_time_since_stable_start;
        if (total_awake_time >= (g_time_since_stable_threshold_sec * 1000)) {
            // Plan 034: Enter extended sleep regardless of BLE connection status
            // BLE connection alone is not user activity - same as activity timeout logic
            Serial.print("Extended sleep: Time since stable threshold exceeded (");
            Serial.print(total_awake_time / 1000);
            Serial.print("s >= ");
            Serial.print(g_time_since_stable_threshold_sec);
            Serial.println("s)");
            Serial.println("Extended sleep: Switching to extended sleep mode");
            g_in_extended_sleep_mode = true;
            enterExtendedDeepSleep();
            // Will not return from deep sleep
        }
    }

    // Check if activity timeout expired (only if sleep enabled)
    // Plan 035: Use extended timeout for NEW unsynced records to allow iOS background sync
    // Only extend if there are MORE unsynced records than when we last slept (new drinks)
    // This prevents waiting 4 minutes on every wake when iOS app isn't running
    // BLE data activity resets the timeout via bleCheckDataActivity() above
    uint32_t timeout_ms = g_sleep_timeout_ms;
#if ENABLE_BLE
    uint16_t unsynced = storageGetUnsyncedCount();
    bool has_new_unsynced = (unsynced > g_unsynced_at_wake);
    if (has_new_unsynced) {
        timeout_ms = ACTIVITY_TIMEOUT_EXTENDED_MS;
    }
#endif
    if (g_sleep_timeout_ms > 0 && millis() - wakeTime >= timeout_ms) {
        bool sleep_blocked = false;

        // Prevent sleep during active calibration
#if ENABLE_STANDALONE_CALIBRATION
        if (calibrationIsActive()) {
            Serial.println("Sleep blocked - standalone calibration in progress");
            wakeTime = millis(); // Reset timer to prevent immediate sleep after calibration
            sleep_blocked = true;
        }
#endif
#if ENABLE_BLE
        // Block sleep during iOS-driven calibration (Plan 060)
        if (bleIsCalibrationInProgress()) {
            Serial.println("Sleep blocked - BLE calibration in progress");
            wakeTime = millis(); // Reset timer to prevent immediate sleep after calibration
            sleep_blocked = true;
        }
#endif
        // Plan 067: If bottle was never placed on a surface (UPRIGHT_STABLE) this
        // wake cycle, don't sleep — stay awake and let the 180s timer handle
        // backpack mode entry. This avoids cross-cycle state issues since the
        // timer runs continuously while awake.
        if (gesture != GESTURE_UPRIGHT_STABLE && !sleep_blocked) {
            Serial.println("Sleep deferred - no UPRIGHT_STABLE, waiting for 180s backpack mode timer");
            wakeTime = millis(); // Reset activity timeout, stay awake
            sleep_blocked = true;
        }

        // Note: BLE connection alone no longer blocks sleep (Plan 034)
        // Only BLE data activity (sync, commands) resets the activity timeout
        // This prevents the device staying awake forever when connected but idle

        if (!sleep_blocked) {
#if DISPLAY_SLEEP_INDICATOR
            // Show Zzzz indicator before sleeping
            Serial.println("Displaying Zzzz indicator...");

            // Use last valid display state values (don't re-read sensors at sleep time
            // as bottle may be in invalid orientation)
            DisplayState last_state = displayGetState();

            // Display with Zzzz indicator (sleeping = true), reusing last valid state
            displayUpdate(last_state.water_ml, last_state.daily_total_ml,
                         last_state.hour, last_state.minute,
                         last_state.battery_percent, true);

            delay(1000); // Brief pause to ensure display updates
#else
            Serial.println("Entering sleep without display update (DISPLAY_SLEEP_INDICATOR=0)");
#endif

            // Stop BLE advertising before sleep (Plan 034: awake = advertising, asleep = not)
#if ENABLE_BLE
            bleStopAdvertising();
#endif

            // Log which timeout was used (Plan 035)
            Serial.print("Activity timeout expired (");
            Serial.print(timeout_ms / 1000);
            Serial.print("s");
#if ENABLE_BLE
            if (unsynced > 0) {
                Serial.print(" - extended due to ");
                Serial.print(unsynced);
                Serial.print(" unsynced records");
            }
#endif
            Serial.println(")");

            if (adxlReady) {
                // ADXL343 auto-clears interrupt, but read for diagnostics
                readAccelReg(0x30);  // INT_SOURCE register
                Serial.print("INT pin before sleep: ");
                Serial.println(digitalRead(PIN_ACCEL_INT));
            }

            enterDeepSleep();
        }
    }

    // Reduced delay for more responsive gesture detection
    delay(200);
}
