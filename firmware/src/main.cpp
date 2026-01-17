/**
 * Aquavate - Smart Water Bottle Firmware
 * Main entry point
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include "aquavate.h"
#include "config.h"

// Calibration system includes
#include "gestures.h"
#include "weight.h"
#include "storage.h"
#include "calibration.h"
#if ENABLE_STANDALONE_CALIBRATION
#include "ui_calibration.h"
#endif

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
Adafruit_LIS3DH lis = Adafruit_LIS3DH();
bool nauReady = false;
bool lisReady = false;

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
#endif

// Time state
int8_t g_timezone_offset = 0;  // UTC offset in hours
bool g_time_valid = false;     // Has time been set?
bool g_rtc_ds3231_present = false;  // DS3231 RTC detected (future)

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
    Wire.beginTransmission(0x18);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t readAccelReg(uint8_t reg) {
    Wire.beginTransmission(0x18);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)0x18, (uint8_t)1);
    return Wire.read();
}

void testInterruptState() {
    // Manual test function - call this while bottle is tilted to see if interrupt triggers
    if (!lisReady) {
        Serial.println("LIS3DH not ready");
        return;
    }

    Serial.println("\n=== INTERRUPT STATE TEST ===");

    // Read current accelerometer values
    lis.read();
    float x_g = lis.x / 16000.0f;
    float y_g = lis.y / 16000.0f;
    float z_g = lis.z / 16000.0f;

    Serial.print("Current orientation: X=");
    Serial.print(x_g, 3);
    Serial.print("g, Y=");
    Serial.print(y_g, 3);
    Serial.print("g, Z=");
    Serial.print(z_g, 3);
    Serial.println("g");

    // Read interrupt registers (reading INT1_SRC clears the latch)
    uint8_t int_src = readAccelReg(LIS3DH_REG_INT1SRC);
    delay(10);  // Wait for latch to clear

    // Read again to get current state after clearing
    int_src = readAccelReg(LIS3DH_REG_INT1SRC);
    uint8_t int_cfg = readAccelReg(LIS3DH_REG_INT1CFG);
    uint8_t int_ths = readAccelReg(LIS3DH_REG_INT1THS);
    int pin_state = digitalRead(LIS3DH_INT_PIN);

    Serial.print("INT1_SRC: 0x");
    Serial.print(int_src, HEX);
    Serial.print(" - IA=");
    Serial.print((int_src & 0x40) ? "1" : "0");
    Serial.print(", ZH=");
    Serial.print((int_src & 0x20) ? "1" : "0");
    Serial.print(", ZL=");
    Serial.print((int_src & 0x04) ? "1" : "0");
    Serial.print(", YH=");
    Serial.print((int_src & 0x10) ? "1" : "0");
    Serial.print(", YL=");
    Serial.print((int_src & 0x02) ? "1" : "0");
    Serial.print(", XH=");
    Serial.print((int_src & 0x08) ? "1" : "0");
    Serial.print(", XL=");
    Serial.println((int_src & 0x01) ? "1" : "0");

    Serial.print("INT1_CFG: 0x");
    Serial.print(int_cfg, HEX);
    Serial.print(" (AOI=");
    Serial.print((int_cfg & 0x80) ? "1" : "0");
    Serial.print(", enabled: ");
    if (int_cfg & 0x20) Serial.print("ZHIE ");
    if (int_cfg & 0x04) Serial.print("ZLIE ");
    if (int_cfg & 0x10) Serial.print("YHIE ");
    if (int_cfg & 0x02) Serial.print("YLIE ");
    if (int_cfg & 0x08) Serial.print("XHIE ");
    if (int_cfg & 0x01) Serial.print("XLIE ");
    Serial.println(")");

    Serial.print("Threshold: 0x");
    Serial.print(int_ths, HEX);
    Serial.print(" = ");
    Serial.print(int_ths * 0.016f, 3);
    Serial.println("g");

    Serial.print("INT pin state: ");
    Serial.println(pin_state ? "HIGH (interrupt active)" : "LOW (no interrupt)");

    Serial.print("Expected: Z=");
    Serial.print(z_g, 3);
    Serial.print("g should be ");
    Serial.print((z_g < (int_ths * 0.016f)) ? "BELOW" : "ABOVE");
    Serial.print(" threshold ");
    Serial.print(int_ths * 0.016f, 3);
    Serial.print("g -> ");
    Serial.println((z_g < (int_ths * 0.016f)) ? "SHOULD TRIGGER" : "should NOT trigger");

    Serial.println("=========================\n");
}

void configureLIS3DHInterrupt() {
    // Configure LIS3DH to wake when Z-axis < threshold (tilted >80° from vertical)
    // ORIGINAL CONFIGURATION - reverted from experimental multi-axis interrupt

    Serial.println("Configuring LIS3DH interrupt for wake-on-tilt...");

    // Configure interrupt pin with pulldown (INT is push-pull active high)
    pinMode(LIS3DH_INT_PIN, INPUT_PULLDOWN);

    // CTRL_REG1: 10Hz ODR, normal mode, all axes enabled
    writeAccelReg(LIS3DH_REG_CTRL1, 0x27);

    // CTRL_REG2: No filters
    writeAccelReg(LIS3DH_REG_CTRL2, 0x00);

    // CTRL_REG3: AOI1 interrupt on INT1 pin
    writeAccelReg(LIS3DH_REG_CTRL3, 0x40);

    // CTRL_REG4: ±2g full scale
    writeAccelReg(LIS3DH_REG_CTRL4, 0x00);

    // CTRL_REG5: Latch interrupt on INT1
    writeAccelReg(LIS3DH_REG_CTRL5, 0x08);

    // CTRL_REG6: INT1 active high
    writeAccelReg(LIS3DH_REG_CTRL6, 0x00);

    // INT1_THS: Threshold for tilt detection
    // See config.h for calibration details
    writeAccelReg(LIS3DH_REG_INT1THS, LIS3DH_TILT_WAKE_THRESHOLD);

    // INT1_DURATION: 0 (immediate)
    writeAccelReg(LIS3DH_REG_INT1DUR, 0x00);

    // INT1_CFG: Trigger on Z-low event only (bit 1)
    // 0x02 = ZLIE (Z Low Interrupt Enable)
    writeAccelReg(LIS3DH_REG_INT1CFG, 0x02);

    delay(50);

    // Clear any pending interrupt
    uint8_t int_src = readAccelReg(LIS3DH_REG_INT1SRC);
    delay(10);

    // Read current accelerometer values for diagnostics
    lis.read();
    float x_g = lis.x / 16000.0f;
    float y_g = lis.y / 16000.0f;
    float z_g = lis.z / 16000.0f;

    // Verify interrupt configuration
    uint8_t ctrl1 = readAccelReg(LIS3DH_REG_CTRL1);
    uint8_t ctrl2 = readAccelReg(LIS3DH_REG_CTRL2);
    uint8_t ctrl3 = readAccelReg(LIS3DH_REG_CTRL3);
    uint8_t ctrl4 = readAccelReg(LIS3DH_REG_CTRL4);
    uint8_t ctrl5 = readAccelReg(LIS3DH_REG_CTRL5);
    uint8_t ctrl6 = readAccelReg(LIS3DH_REG_CTRL6);
    uint8_t int_cfg = readAccelReg(LIS3DH_REG_INT1CFG);
    uint8_t int_ths = readAccelReg(LIS3DH_REG_INT1THS);
    uint8_t int_dur = readAccelReg(LIS3DH_REG_INT1DUR);
    int pin_state = digitalRead(LIS3DH_INT_PIN);

    Serial.println("LIS3DH interrupt configuration:");
    Serial.print("  CTRL_REG1: 0x");
    Serial.print(ctrl1, HEX);
    Serial.print(" (ODR=");
    Serial.print((ctrl1 >> 4) & 0x0F);
    Serial.print(", Xen=");
    Serial.print((ctrl1 & 0x01) ? "1" : "0");
    Serial.print(", Yen=");
    Serial.print((ctrl1 & 0x02) ? "1" : "0");
    Serial.print(", Zen=");
    Serial.print((ctrl1 & 0x04) ? "1" : "0");
    Serial.println(")");
    Serial.print("  CTRL_REG2: 0x");
    Serial.print(ctrl2, HEX);
    Serial.println(" (HP filter)");
    Serial.print("  CTRL_REG3: 0x");
    Serial.print(ctrl3, HEX);
    Serial.println(" (INT1 routing)");
    Serial.print("  CTRL_REG4: 0x");
    Serial.print(ctrl4, HEX);
    Serial.println(" (scale/resolution)");
    Serial.print("  CTRL_REG5: 0x");
    Serial.print(ctrl5, HEX);
    Serial.println(" (latch enable)");
    Serial.print("  CTRL_REG6: 0x");
    Serial.print(ctrl6, HEX);
    Serial.println(" (INT polarity)");
    Serial.print("  INT1_CFG: 0x");
    Serial.print(int_cfg, HEX);
    Serial.print(" (ZLIE=");
    Serial.print((int_cfg & 0x02) ? "1" : "0");
    Serial.print(", AOI=");
    Serial.print((int_cfg & 0x80) ? "1" : "0");
    Serial.println(")");
    Serial.print("  INT1_THS: 0x");
    Serial.print(int_ths, HEX);
    Serial.print(" (");
    Serial.print(int_ths);
    Serial.print(" LSB = ");
    Serial.print(int_ths * 0.016f, 3);
    Serial.println("g)");
    Serial.print("  INT1_DUR: 0x");
    Serial.println(int_dur, HEX);
    Serial.print("  INT1_SRC: 0x");
    Serial.print(int_src, HEX);
    Serial.print(" (ZL=");
    Serial.print((int_src & 0x04) ? "1" : "0");
    Serial.print(", IA=");
    Serial.print((int_src & 0x40) ? "1" : "0");
    Serial.println(")");
    Serial.print("  INT pin state: ");
    Serial.println(pin_state ? "HIGH" : "LOW");
    Serial.print("  Current accel: X=");
    Serial.print(x_g, 3);
    Serial.print("g, Y=");
    Serial.print(y_g, 3);
    Serial.print("g, Z=");
    Serial.print(z_g, 3);
    Serial.println("g");
    Serial.println("Wake condition: Z-axis drops below threshold (tilt in any direction)");
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
    if (!lisReady) {
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
        default: Serial.print("NONE"); break;
    }
    Serial.print(", Final gesture: ");
    switch (final_gesture) {
        case GESTURE_INVERTED_HOLD: Serial.print("INVERTED_HOLD"); break;
        case GESTURE_UPRIGHT_STABLE: Serial.print("UPRIGHT_STABLE"); break;
        case GESTURE_SIDEWAYS_TILT: Serial.print("SIDEWAYS_TILT"); break;
        case GESTURE_UPRIGHT: Serial.print("UPRIGHT"); break;
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

    Serial.flush();

    // Configure wake-up interrupt from LIS3DH INT1 pin
    // Wake on HIGH level (interrupt pin goes HIGH when tilted)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)LIS3DH_INT_PIN, 1);

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
            Serial.print(LIS3DH_INT_PIN);
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

    // Initialize LIS3DH accelerometer
    Serial.println("Initializing LIS3DH...");
    if (lis.begin(0x18)) {  // Default I2C address
        Serial.println("LIS3DH found!");
        lis.setRange(LIS3DH_RANGE_2_G);
        lis.setDataRate(LIS3DH_DATARATE_10_HZ);
        lisReady = true;

        // If we woke from EXT0, read current accelerometer state and clear interrupt
        if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
            Serial.println("Checking accelerometer state after wake...");
            lis.read();
            float x = lis.x / 16000.0f;
            float y = lis.y / 16000.0f;
            float z = lis.z / 16000.0f;
            Serial.print("  Current orientation: X=");
            Serial.print(x, 2);
            Serial.print("g Y=");
            Serial.print(y, 2);
            Serial.print("g Z=");
            Serial.print(z, 2);
            Serial.println("g");

            // Read INT1_SRC to clear the interrupt latch
            uint8_t int_src = readAccelReg(LIS3DH_REG_INT1SRC);
            Serial.print("  INT1_SRC: 0x");
            Serial.print(int_src, HEX);
            Serial.println(" (cleared)");
        }

        // Configure interrupt for wake-on-tilt
        configureLIS3DHInterrupt();
    } else {
        Serial.println("LIS3DH not found!");
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
                uint32_t last_boot_time = storageLoadLastBootTime();
                if (last_boot_time > 0) {
                    struct timeval tv;
                    tv.tv_sec = last_boot_time;
                    tv.tv_usec = 0;
                    settimeofday(&tv, NULL);
                    Serial.println("  RTC restored from NVS (cold boot)");
                    Serial.println("  WARNING: Time may be inaccurate (DS3231 RTC recommended)");
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
    if (lisReady) {
        gesturesInit(lis);
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
    if (lisReady) {
        sensors.gesture = gesturesUpdate(sensors.water_ml);
    }

    // NOW use snapshot for all logic below
    int32_t current_adc = sensors.adc_reading;
    float current_water_ml = sensors.water_ml;
    GestureType gesture = sensors.gesture;

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
        if (gesture == GESTURE_INVERTED_HOLD) {
            Serial.println("Main: Calibration triggered!");
            calibrationStart();
            cal_state = calibrationGetState();
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

            g_last_cal_state = new_state;
        }

        // Check if calibration completed
        if (new_state == CAL_COMPLETE && g_last_cal_state != CAL_COMPLETE) {
            CalibrationResult result = calibrationGetResult();
            if (result.success) {
                Serial.println("Main: Calibration completed successfully!");
                g_calibration = result.data;
                g_calibrated = true;

                // Show complete screen for 5 seconds
                delay(5000);

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
        }

        // Check for calibration error
        if (new_state == CAL_ERROR && g_last_cal_state != CAL_ERROR) {
            CalibrationResult result = calibrationGetResult();
            Serial.print("Main: Calibration error: ");
            Serial.println(result.error_message);

            // Show error screen for 5 seconds
            delay(5000);

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
        if (lisReady && (millis() - last_debug_print >= 1000)) {
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
            // Block extended sleep when BLE is connected (same as normal sleep - conditional)
#if ENABLE_BLE
            if (bleIsConnected()) {
                Serial.println("Extended sleep blocked - BLE connected");
                g_continuous_awake_start = millis(); // Reset timer while connected
            } else
#endif
            {
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

            if (lisReady) {
                // Clear any pending interrupt before sleep
                readAccelReg(LIS3DH_REG_INT1SRC);
                Serial.print("INT pin before sleep: ");
                Serial.println(digitalRead(LIS3DH_INT_PIN));
            }
            enterDeepSleep();
        }
    }

    // Reduced delay for more responsive gesture detection
    delay(200);
}
