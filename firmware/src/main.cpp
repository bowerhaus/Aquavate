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

// Display state tracking
#include "display.h"

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

// Extended deep sleep tracking
unsigned long g_continuous_awake_start = 0;     // When current awake period started
bool g_in_extended_sleep_mode = false;          // Currently using extended sleep
uint32_t g_extended_sleep_timer_sec = EXTENDED_SLEEP_TIMER_SEC;       // Timer wake duration (default 60s)
uint32_t g_extended_sleep_threshold_sec = EXTENDED_SLEEP_THRESHOLD_SEC; // Awake threshold (default 120s)
bool g_force_display_clear_sleep = false;       // Flag to clear Zzzz indicator on wake from extended sleep

// RTC memory persistence (survives deep sleep)
RTC_DATA_ATTR uint32_t rtc_extended_sleep_magic = 0;
RTC_DATA_ATTR bool rtc_in_extended_sleep_mode = false;
RTC_DATA_ATTR unsigned long rtc_continuous_awake_start = 0;

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

// Runtime sleep timeout control (0 = disabled, non-persistent)
uint32_t g_sleep_timeout_ms = AWAKE_DURATION_MS;  // Default 30 seconds

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
    uint8_t thresh_act = readAccelReg(0x1C);  // THRESH_ACT register
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
    Serial.println("\n=== ADXL343 Interrupt Configuration ===");

    // Set interrupt pin mode
    pinMode(PIN_ACCEL_INT, INPUT_PULLDOWN);  // Active-high interrupt

    // ADXL343 Register Definitions
    const uint8_t THRESH_ACT = 0x1C;        // Activity threshold
    const uint8_t TIME_ACT = 0x22;          // Activity duration
    const uint8_t ACT_INACT_CTL = 0x27;     // Axis enable for activity/inactivity
    const uint8_t POWER_CTL = 0x2D;         // Power control
    const uint8_t INT_ENABLE = 0x2E;        // Interrupt enable
    const uint8_t INT_MAP = 0x2F;           // Interrupt mapping
    const uint8_t DATA_FORMAT = 0x31;       // Data format

    // Step 1: Configure data format (±2g range)
    writeAccelReg(DATA_FORMAT, 0x00);       // ±2g, 13-bit resolution, right-justified
    Serial.println("1. Data format: ±2g range");

    // Step 2: Set activity threshold - EXTREMELY HIGH to only detect pick-up
    // AC-coupled mode detects CHANGES in acceleration (motion)
    // Threshold = 3.0g = 3000 mg (extremely aggressive - only very strong deliberate motion)
    // Scale = 62.5 mg/LSB
    // Value = 3000 / 62.5 = 48 (0x30)
    writeAccelReg(THRESH_ACT, 0x30);        // ~3.0g activity threshold
    Serial.println("2. Activity threshold: 0x30 (3.0g)");

    // Step 3: Set activity duration - require very sustained motion
    // At 12.5 Hz sample rate: 1 LSB = 1/12.5 = 80ms
    // Value = 20 → 1600ms (~1.6 seconds) of sustained motion required
    writeAccelReg(TIME_ACT, 0x14);          // 20 samples = ~1600ms
    Serial.println("3. Activity duration: 0x14 (~1600ms)");

    // Step 4: Enable all axes for activity detection (AC-coupled)
    // Bits: 7=ACT_acdc(1=AC), 6-4=ACT_X/Y/Z (111=all axes)
    // ACT_INACT_CTL[7:4] = 1111b = 0xF0
    writeAccelReg(ACT_INACT_CTL, 0xF0);     // All axes activity enable (AC-coupled)
    Serial.println("4. Activity axes: X, Y, Z (AC-coupled)");

    // Step 5: Enable measurement mode
    writeAccelReg(POWER_CTL, 0x08);         // Measurement mode (bit 3)
    Serial.println("5. Power mode: measurement");

    // Step 6: Enable activity interrupt
    writeAccelReg(INT_ENABLE, 0x10);        // Activity interrupt (bit 4)
    Serial.println("6. Interrupt enable: activity");

    // Step 7: Route activity to INT1 pin (bit 4 = 0 for INT1)
    writeAccelReg(INT_MAP, 0x00);           // All interrupts to INT1
    Serial.println("7. Interrupt routing: INT1");

    Serial.println("\n=== Configuration Complete ===");
    Serial.println("Wake condition: Very strong sustained motion (>3.0g for >1.6sec, AC-coupled)");
    Serial.println("Expected: Table/train vibrations ignored, requires deliberate shake/pick-up\n");
}

// Save extended sleep state to RTC memory
void extendedSleepSaveToRTC() {
    rtc_extended_sleep_magic = RTC_EXTENDED_SLEEP_MAGIC;
    rtc_in_extended_sleep_mode = g_in_extended_sleep_mode;
    rtc_continuous_awake_start = g_continuous_awake_start;
}

// Restore extended sleep state from RTC memory
// Returns true if RTC memory was valid, false if power cycle
bool extendedSleepRestoreFromRTC() {
    if (rtc_extended_sleep_magic != RTC_EXTENDED_SLEEP_MAGIC) {
        Serial.println("Extended sleep: RTC memory invalid (power cycle)");
        // Reset to defaults
        g_in_extended_sleep_mode = false;
        g_continuous_awake_start = millis();
        return false;
    }

    Serial.println("Extended sleep: Restoring state from RTC memory");
    g_in_extended_sleep_mode = rtc_in_extended_sleep_mode;
    g_continuous_awake_start = rtc_continuous_awake_start;

    Serial.print("  in_extended_sleep_mode: ");
    Serial.println(g_in_extended_sleep_mode ? "true" : "false");
    Serial.print("  continuous_awake_start: ");
    Serial.println(g_continuous_awake_start);

    return true;
}

// Check if device is still experiencing constant motion (backpack scenario)
bool isStillMovingConstantly() {
    if (!adxlReady) {
        Serial.println("Extended sleep: LIS3DH not ready, assuming stationary");
        return false;
    }

    Serial.println("Extended sleep: Checking motion state...");

    // Sample accelerometer for brief period (~500ms)
    GestureType initial_gesture = gesturesUpdate();
    delay(500);
    GestureType final_gesture = gesturesUpdate();

    // If gesture changed or is unstable (sideways tilt), still moving
    bool still_moving = (initial_gesture != final_gesture ||
                        initial_gesture == GESTURE_SIDEWAYS_TILT ||
                        final_gesture == GESTURE_SIDEWAYS_TILT);

    Serial.print("  Initial gesture: ");
    switch (initial_gesture) {
        case GESTURE_INVERTED_HOLD: Serial.print("INVERTED_HOLD"); break;
        case GESTURE_UPRIGHT_STABLE: Serial.print("UPRIGHT_STABLE"); break;
        case GESTURE_SIDEWAYS_TILT: Serial.print("SIDEWAYS_TILT"); break;
        case GESTURE_UPRIGHT: Serial.print("UPRIGHT"); break;
        case GESTURE_SHAKE_WHILE_INVERTED: Serial.print("SHAKE_WHILE_INVERTED"); break;
        default: Serial.print("NONE"); break;
    }
    Serial.print(", Final gesture: ");
    switch (final_gesture) {
        case GESTURE_INVERTED_HOLD: Serial.print("INVERTED_HOLD"); break;
        case GESTURE_UPRIGHT_STABLE: Serial.print("UPRIGHT_STABLE"); break;
        case GESTURE_SIDEWAYS_TILT: Serial.print("SIDEWAYS_TILT"); break;
        case GESTURE_UPRIGHT: Serial.print("UPRIGHT"); break;
        case GESTURE_SHAKE_WHILE_INVERTED: Serial.print("SHAKE_WHILE_INVERTED"); break;
        default: Serial.print("NONE"); break;
    }
    Serial.println();
    Serial.print("  Still moving: ");
    Serial.println(still_moving ? "YES" : "NO");

    return still_moving;
}

// Enter extended deep sleep mode (timer wake instead of motion wake)
void enterExtendedDeepSleep() {
    Serial.print("Entering extended deep sleep (timer wake: ");
    Serial.print(g_extended_sleep_timer_sec);
    Serial.println("s)...");

#if EXTENDED_SLEEP_INDICATOR
    // Show Zzzz indicator before sleeping (if enabled)
    Serial.println("Displaying Zzzz indicator (extended sleep)...");

    // Use last valid display state values
    DisplayState last_state = displayGetState();

    // Display with Zzzz indicator (sleeping = true)
    displayUpdate(last_state.water_ml, last_state.daily_total_ml,
                 last_state.hour, last_state.minute,
                 last_state.battery_percent, true);

    delay(1000); // Brief pause to ensure display updates
#endif

    // Save state to RTC memory before sleeping
    displaySaveToRTC();
    drinksSaveToRTC();
    extendedSleepSaveToRTC();

    Serial.flush();

    // Configure timer wake (NO motion wake in extended mode)
    esp_sleep_enable_timer_wakeup(g_extended_sleep_timer_sec * 1000000ULL);

    // Enter deep sleep
    esp_deep_sleep_start();
}

void enterDeepSleep() {
    Serial.println("Entering normal deep sleep (motion wake)...");

    // Save state to RTC memory before sleeping
    displaySaveToRTC();
    drinksSaveToRTC();
    extendedSleepSaveToRTC();

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

    // Get current daily state
    DailyState daily_state;
    drinksGetState(daily_state);

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
    displayForceUpdate(water_ml, daily_state.daily_total_ml,
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
            Serial.println("Woke up from timer (extended sleep mode)");
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
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // Woke from timer - was in extended sleep mode
        // Note: Will check motion state and decide mode after sensors initialized
        Serial.println("Extended sleep: Timer wake detected, will check motion state after init");
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        // Woke from motion - was in normal sleep, return to normal mode
        Serial.println("Extended sleep: Motion wake detected, returning to normal mode");
        g_in_extended_sleep_mode = false;
        g_continuous_awake_start = millis();
    } else {
        // Power on/reset - initialize continuous awake tracking
        g_continuous_awake_start = millis();
        g_in_extended_sleep_mode = false;
    }

    Serial.print("Aquavate v");
    Serial.print(AQUAVATE_VERSION_MAJOR);
    Serial.print(".");
    Serial.print(AQUAVATE_VERSION_MINOR);
    Serial.print(".");
    Serial.println(AQUAVATE_VERSION_PATCH);
    Serial.println("=================================");

#if defined(BOARD_ADAFRUIT_FEATHER)
    Serial.println("Board: Adafruit ESP32 Feather V2");
    Serial.println("Display: 2.13\" E-Paper FeatherWing");
#elif defined(BOARD_SPARKFUN_QWIIC)
    Serial.println("Board: SparkFun ESP32-C6 Qwiic Pocket");
    Serial.println("Display: Waveshare 1.54\" E-Paper");
#endif

    Serial.println();

    // Initialize E-Paper display
#if defined(BOARD_ADAFRUIT_FEATHER)
    Serial.println("Initializing E-Paper display...");
    display.begin();
    display.setRotation(2);  // Rotate 180 degrees

    // Draw welcome screen only on power-on/reset (not on wake from sleep)
    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT0 && wakeup_reason != ESP_SLEEP_WAKEUP_TIMER) {
        drawWelcomeScreen();
    }

    // Print battery info
    float batteryV = getBatteryVoltage();
    int batteryPct = getBatteryPercent(batteryV);
    Serial.print("Battery: ");
    Serial.print(batteryV);
    Serial.print("V (");
    Serial.print(batteryPct);
    Serial.println("%)");

    Serial.println("E-Paper display initialized!");
#endif

    Serial.println("Initializing...");

    // Initialize I2C and NAU7802
    Wire.begin();
    Serial.println("Initializing NAU7802...");
    if (nau.begin()) {
        Serial.println("NAU7802 found!");
        nau.setLDO(NAU7802_3V3);
        nau.setGain(NAU7802_GAIN_128);
        nau.setRate(NAU7802_RATE_10SPS);
        nauReady = true;
    } else {
        Serial.println("NAU7802 not found!");
    }

    // Initialize ADXL343 accelerometer
    Serial.println("Initializing ADXL343...");
    if (adxl.begin(I2C_ADDR_ADXL343)) {
        Serial.println("ADXL343 found!");
        adxl.setRange(ADXL343_RANGE_2_G);
        adxl.setDataRate(ADXL343_DATARATE_12_5_HZ);  // Closest to 10 Hz
        adxlReady = true;

        // If we woke from EXT0, read current accelerometer state and clear interrupt
        if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
            Serial.println("Checking accelerometer state after wake...");
            int16_t x, y, z;
            adxl.getXYZ(x, y, z);
            float x_g = x / 256.0f;
            float y_g = y / 256.0f;
            float z_g = z / 256.0f;
            Serial.print("  Current orientation: X=");
            Serial.print(x_g, 2);
            Serial.print("g Y=");
            Serial.print(y_g, 2);
            Serial.print("g Z=");
            Serial.print(z_g, 2);
            Serial.println("g");

            // Read INT_SOURCE to clear the interrupt (ADXL343 auto-clears, but read for diagnostics)
            uint8_t int_source = readAccelReg(0x30);  // INT_SOURCE register
            Serial.print("  INT_SOURCE: 0x");
            Serial.print(int_source, HEX);
            Serial.println(" (cleared)");
        }

        // Configure interrupt for wake-on-tilt
        configureADXL343Interrupt();
    } else {
        Serial.println("ADXL343 not found!");
    }

    // Initialize DS3231 RTC
    Serial.println("Initializing DS3231 RTC...");
    if (!rtc.begin()) {
        Serial.println("DS3231 not detected - using ESP32 internal RTC");
        g_rtc_ds3231_present = false;
    } else {
        Serial.println("DS3231 detected");
        g_rtc_ds3231_present = true;

        // Sync ESP32 RTC from DS3231 on every boot/wake
        DateTime now = rtc.now();
        struct timeval tv = {
            .tv_sec = now.unixtime(),
            .tv_usec = 0
        };
        settimeofday(&tv, NULL);
        g_time_valid = true;  // DS3231 is always valid (battery-backed)

        Serial.printf("ESP32 RTC synced from DS3231: %04d-%02d-%02d %02d:%02d:%02d\n",
                      now.year(), now.month(), now.day(),
                      now.hour(), now.minute(), now.second());
    }

    // Initialize calibration system
    Serial.println("Initializing calibration system...");

    // Initialize storage (NVS)
    if (storageInit()) {
        Serial.println("Storage initialized");

        // Load calibration from NVS
        g_calibrated = storageLoadCalibration(g_calibration);

        if (g_calibrated) {
            Serial.println("Calibration loaded from NVS:");
            Serial.print("  Scale factor: ");
            Serial.print(g_calibration.scale_factor);
            Serial.println(" ADC/g");
            Serial.print("  Empty ADC: ");
            Serial.println(g_calibration.empty_bottle_adc);
            Serial.print("  Full ADC: ");
            Serial.println(g_calibration.full_bottle_adc);
        } else {
            Serial.println("No valid calibration found - calibration required");
        }

        // Load timezone and time_valid from NVS
        g_timezone_offset = storageLoadTimezone();
        g_time_valid = storageLoadTimeValid();

        // DS3231 overrides NVS time_valid - battery-backed time is always valid
        if (g_rtc_ds3231_present) {
            g_time_valid = true;
            Serial.println("Time valid: true (DS3231 battery-backed)");
        }

        // Load display mode from NVS
        g_daily_intake_display_mode = storageLoadDisplayMode();
        const char* mode_name = (g_daily_intake_display_mode == 0) ? "Human figure" : "Tumbler grid";
        Serial.print("Display mode loaded: ");
        Serial.print(g_daily_intake_display_mode);
        Serial.print(" (");
        Serial.print(mode_name);
        Serial.println(")");

        // Load sleep timeout from NVS
        uint32_t sleep_timeout_seconds = storageLoadSleepTimeout();
        g_sleep_timeout_ms = sleep_timeout_seconds * 1000;
        if (sleep_timeout_seconds == 0) {
            Serial.println("Sleep timeout: DISABLED (debug mode)");
        } else {
            Serial.print("Sleep timeout: ");
            Serial.print(sleep_timeout_seconds);
            Serial.println(" seconds");
        }

        // Load extended sleep settings from NVS
        g_extended_sleep_timer_sec = storageLoadExtendedSleepTimer();
        g_extended_sleep_threshold_sec = storageLoadExtendedSleepThreshold();
        Serial.print("Extended sleep timer: ");
        Serial.print(g_extended_sleep_timer_sec);
        Serial.println(" seconds");
        Serial.print("Extended sleep threshold: ");
        Serial.print(g_extended_sleep_threshold_sec);
        Serial.println(" seconds");

        if (g_time_valid) {
            Serial.println("Time configuration loaded:");
            Serial.print("  Timezone offset: ");
            Serial.println(g_timezone_offset);

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
                        Serial.println("  RTC restored from NVS (cold boot)");
                        Serial.println("  WARNING: Time may be inaccurate (ESP32 internal RTC drift)");
                    }
                } else {
                    Serial.println("  RTC synced from DS3231 (cold boot)");
                }
            } else {
                // Wake from deep sleep - RTC time is still valid, don't overwrite
                Serial.println("  RTC time preserved (wake from deep sleep)");
            }

            // Show current time
            char time_str[64];
            formatTimeForDisplay(time_str, sizeof(time_str));
            Serial.print("  Current time: ");
            Serial.println(time_str);
        } else {
            Serial.println("WARNING: Time not set!");
            Serial.println("Use SET_DATETIME command to set time");
            Serial.println("Example: SET_DATETIME 2026-01-13 14:30:00 -5");
        }
    } else {
        Serial.println("Storage initialization failed");
    }

    // Initialize serial command handler (conditional)
#if ENABLE_SERIAL_COMMANDS
    serialCommandsInit();
    serialCommandsSetTimeCallback(onTimeSet);
    Serial.println("Serial command handler initialized");
#endif

    // Initialize gesture detection
    if (adxlReady) {
        gesturesInit(adxl);
        Serial.println("Gesture detection initialized");
    }

    // Initialize weight measurement
    if (nauReady) {
        weightInit(nau);
        Serial.println("Weight measurement initialized");
    }

    // Handle timer wake from extended sleep (now that sensors are initialized)
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // Restore extended sleep state from RTC memory
        extendedSleepRestoreFromRTC();

        // Check if still experiencing constant motion
        if (isStillMovingConstantly()) {
            // Continue extended sleep
            Serial.println("Extended sleep: Still moving - re-entering extended sleep");
            enterExtendedDeepSleep();
            // Will not return from deep sleep
        } else {
            // Return to normal mode
            Serial.println("Extended sleep: Motion stopped - returning to normal mode");
            g_in_extended_sleep_mode = false;
            g_continuous_awake_start = millis();  // Reset tracking

            // Set flag to force display update to clear Zzzz indicator
            g_force_display_clear_sleep = true;
            Serial.println("Extended sleep: Will clear Zzzz indicator on first stable update");
        }
    }

    // Initialize calibration state machine
#if ENABLE_STANDALONE_CALIBRATION
    calibrationInit();
    Serial.println("Calibration state machine initialized");

    // Initialize calibration UI
#if defined(BOARD_ADAFRUIT_FEATHER)
    uiCalibrationInit(display);
    Serial.println("Calibration UI initialized");

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
        Serial.println("Drink tracking system initialized");
    } else {
        Serial.println("WARNING: Drink tracking not initialized - time not set");
    }

    // Initialize display state tracking module
#if defined(BOARD_ADAFRUIT_FEATHER)
    displayInit(display);

    // Restore state from RTC memory if waking from deep sleep
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // Attempt to restore display state from RTC memory
        bool display_restored = displayRestoreFromRTC();
        bool drinks_restored = drinksRestoreFromRTC();

        if (display_restored && drinks_restored) {
            Serial.println("State restored from RTC memory (wake from sleep)");
            // Mark display as initialized to prevent unnecessary updates
            displayMarkInitialized();
        } else {
            Serial.println("No valid RTC state (power cycle) - will force display update");
        }
    } else {
        Serial.println("Display state tracking initialized (power on/reset)");
    }

    // Note: Display will update on first stable check to ensure correct values shown
#endif

    // Initialize NVS (required by ESP-IDF BLE)
    Serial.println("Initializing NVS flash...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println("NVS: Erasing and reinitializing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    Serial.println("NVS initialized!");

    // Initialize BLE service (conditional)
#if ENABLE_BLE
    Serial.println("Initializing BLE service...");
    if (bleInit()) {
        Serial.println("BLE service initialized!");

        // Start advertising on motion wake (not timer wake)
        if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
            Serial.println("Motion wake detected - starting BLE advertising");
            bleStartAdvertising();
        } else if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
            Serial.println("Power on/reset - starting BLE advertising");
            bleStartAdvertising();
        } else {
            Serial.println("Timer wake - BLE advertising NOT started (power conservation)");
        }
    } else {
        Serial.println("BLE initialization failed!");
    }
#endif

    Serial.println("Setup complete!");
    Serial.print("Will sleep in ");
    Serial.print(AWAKE_DURATION_MS / 1000);
    Serial.println(" seconds...");
}

void loop() {
    // Check for serial commands (conditional)
#if ENABLE_SERIAL_COMMANDS
    serialCommandsUpdate();
#endif

    // Update BLE service (conditional)
#if ENABLE_BLE
    bleUpdate();
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
    }

    // NOW use snapshot for all logic below
    int32_t current_adc = sensors.adc_reading;
    float current_water_ml = sensors.water_ml;
    GestureType gesture = sensors.gesture;

    // Handle shake-while-inverted gesture (shake to empty)
    if (gesture == GESTURE_SHAKE_WHILE_INVERTED) {
        if (!g_cancel_drink_pending) {
            g_cancel_drink_pending = true;
            Serial.println("Main: Shake gesture detected - cancel drink pending (return bottle upright to confirm)");
        }
    }

    // Handle pending drink cancellation when bottle returns to upright stable
    if (gesture == GESTURE_UPRIGHT_STABLE && g_cancel_drink_pending) {
        g_cancel_drink_pending = false;

        // Verify bottle is actually empty before cancelling
        if (current_water_ml < BOTTLE_EMPTY_THRESHOLD_ML) {
            // Bottle is empty - cancel the last drink
            if (drinksCancelLast()) {
                Serial.println("Main: Last drink cancelled - showing confirmation");
#if defined(BOARD_ADAFRUIT_FEATHER)
                uiShowBottleEmptied(display);
                delay(3000);  // Show confirmation for 3 seconds

                // Get current values for display update
                DailyState daily_state;
                drinksGetState(daily_state);

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

                displayForceUpdate(current_water_ml, daily_state.daily_total_ml,
                                  time_hour, time_minute, battery_pct, false);
#endif
            }
            // If drinksCancelLast() returns false (nothing to cancel), silently ignore
        } else {
            // Bottle still has liquid - ignore shake gesture, process as normal
            Serial.printf("Main: Bottle not empty (%.1fml >= %dml) - ignoring shake gesture\n",
                         current_water_ml, BOTTLE_EMPTY_THRESHOLD_ML);
            // Normal drink detection will happen below via drinksUpdate()
        }
    }

    // Check calibration state (used in display logic even if standalone calibration disabled)
#if ENABLE_STANDALONE_CALIBRATION
    CalibrationState cal_state = calibrationGetState();
#else
    CalibrationState cal_state = CAL_IDLE;  // Always idle when standalone calibration disabled
#endif

#if ENABLE_STANDALONE_CALIBRATION
    // If not in calibration mode, check for calibration trigger
    if (cal_state == CAL_IDLE) {
        // Check for calibration trigger gesture (hold inverted for 5s)
        // But don't re-trigger immediately after an abort - require gesture release first
        if (gesture == GESTURE_INVERTED_HOLD) {
            if (!g_cal_just_cancelled) {
                Serial.println("Main: Calibration triggered!");
                calibrationStart();
                cal_state = calibrationGetState();
            }
        } else {
            // Gesture released - allow new calibration trigger
            g_cal_just_cancelled = false;
        }

        // If calibrated, show water level
        if (g_debug_enabled && g_debug_water_level && g_calibrated && nauReady) {
            float water_ml = calibrationGetWaterWeight(current_adc, g_calibration);
            Serial.print("Water level: ");
            Serial.print(water_ml);
            Serial.println(" ml");
        }
    }

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

                DailyState daily_state;
                drinksGetState(daily_state);

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

                displayForceUpdate(water_ml, daily_state.daily_total_ml,
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

                DailyState daily_state;
                drinksGetState(daily_state);

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

                displayForceUpdate(water_ml, daily_state.daily_total_ml,
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

                DailyState daily_state;
                drinksGetState(daily_state);

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

                displayForceUpdate(water_ml, daily_state.daily_total_ml,
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
                    drinksUpdate(current_adc, g_calibration);
                }

                // Check interval timers for time and battery updates
                bool time_interval_elapsed = (millis() - last_time_check >= DISPLAY_TIME_UPDATE_INTERVAL_MS);
                bool battery_interval_elapsed = (millis() - last_battery_check >= DISPLAY_BATTERY_UPDATE_INTERVAL_MS);

                // Get current daily total
                DailyState daily_state;
                drinksGetState(daily_state);

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

                // Check if display needs update (water, daily intake, time, or battery changed)
                // OR if we need to clear Zzzz indicator after extended sleep
                if (g_force_display_clear_sleep ||
                    displayNeedsUpdate(display_water_ml, daily_state.daily_total_ml,
                                      time_interval_elapsed, battery_interval_elapsed)) {

                    if (g_force_display_clear_sleep) {
                        Serial.println("Extended sleep: Clearing Zzzz indicator");
                        g_force_display_clear_sleep = false;
                    }

                    displayUpdate(display_water_ml, daily_state.daily_total_ml,
                                 time_hour, time_minute, battery_pct, false);

                    // Reset interval timers if they triggered the update
                    if (time_interval_elapsed) last_time_check = millis();
                    if (battery_interval_elapsed) last_battery_check = millis();
                }

                // Update BLE Current State (send notifications if connected - conditional)
#if ENABLE_BLE
                bleUpdateCurrentState(daily_state, current_adc, g_calibration,
                                    battery_pct, g_calibrated, g_time_valid,
                                    gesture == GESTURE_UPRIGHT_STABLE);

                // Update BLE battery level (separate characteristic)
                bleUpdateBatteryLevel(battery_pct);
#endif
            }
#endif
        }
    }

    // Debug output (only print periodically to reduce serial spam)
    if (g_debug_enabled && g_debug_accelerometer) {
        static unsigned long last_debug_print = 0;
        if (adxlReady && (millis() - last_debug_print >= 1000)) {
            last_debug_print = millis();

            float x, y, z;
            gesturesGetAccel(x, y, z);
            Serial.print("Accel X: ");
            Serial.print(x, 2);
            Serial.print("g  Y: ");
            Serial.print(y, 2);
            Serial.print("g  Z: ");
            Serial.print(z, 2);
            Serial.print("g  Gesture: ");

            switch (gesture) {
                case GESTURE_INVERTED_HOLD: Serial.print("INVERTED_HOLD"); break;
                case GESTURE_UPRIGHT_STABLE: Serial.print("UPRIGHT_STABLE"); break;
                case GESTURE_SIDEWAYS_TILT: Serial.print("SIDEWAYS_TILT"); break;
                case GESTURE_SHAKE_WHILE_INVERTED: Serial.print("SHAKE_WHILE_INVERTED"); break;
                default: Serial.print("NONE"); break;
            }

            Serial.print("  Cal State: ");
            Serial.print(calibrationGetStateName(cal_state));

            // Only show sleep countdown if sleep is enabled
            if (g_sleep_timeout_ms > 0) {
                unsigned long elapsed = millis() - wakeTime;
                if (elapsed < g_sleep_timeout_ms) {
                    unsigned long remaining = (g_sleep_timeout_ms - elapsed) / 1000;
                    Serial.print("  (sleep in ");
                    Serial.print(remaining);
                    Serial.print("s)");
                }
            }
            Serial.println();
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
    if (!g_in_extended_sleep_mode
#if ENABLE_STANDALONE_CALIBRATION
        && !calibrationIsActive()
#endif
    ) {
        unsigned long total_awake_time = millis() - g_continuous_awake_start;
        if (total_awake_time >= (g_extended_sleep_threshold_sec * 1000)) {
            // Reset extended sleep timer when bottle is upright and stable (not in backpack)
            if (gesture == GESTURE_UPRIGHT_STABLE) {
                // Bottle is stationary - reset timer, normal sleep will handle this
                g_continuous_awake_start = millis();
            }
            // Block extended sleep when BLE is connected (same as normal sleep - conditional)
#if ENABLE_BLE
            else if (bleIsConnected()) {
                Serial.println("Extended sleep blocked - BLE connected");
                g_continuous_awake_start = millis(); // Reset timer while connected
            }
#endif
            else {
                Serial.print("Extended sleep: Continuous awake threshold exceeded (");
                Serial.print(total_awake_time / 1000);
                Serial.print("s >= ");
                Serial.print(g_extended_sleep_threshold_sec);
                Serial.println("s)");
                Serial.println("Extended sleep: Switching to extended sleep mode");
                g_in_extended_sleep_mode = true;
                enterExtendedDeepSleep();
                // Will not return from deep sleep
            }
        }
    }

    // Check if it's time to sleep (only if sleep enabled)
    if (g_sleep_timeout_ms > 0 && millis() - wakeTime >= g_sleep_timeout_ms) {
        bool sleep_blocked = false;

        // Prevent sleep during active calibration
#if ENABLE_STANDALONE_CALIBRATION
        if (calibrationIsActive()) {
            Serial.println("Sleep blocked - calibration in progress");
            wakeTime = millis(); // Reset timer to prevent immediate sleep after calibration
            sleep_blocked = true;
        }
#endif
        // Prevent sleep when BLE is connected (conditional)
#if ENABLE_BLE
        if (!sleep_blocked && bleIsConnected()) {
            Serial.println("Sleep blocked - BLE connected");
            wakeTime = millis(); // Reset timer while connected
            sleep_blocked = true;
        }
#endif

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
