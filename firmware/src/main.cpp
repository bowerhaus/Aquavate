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
#include "ui_calibration.h"

// Serial commands for time setting
#include "serial_commands.h"
#include <sys/time.h>
#include <time.h>

// Daily intake tracking
#include "drinks.h"

// Display state tracking
#include "display.h"

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

// Bitmap data and drawing functions moved to display.cpp

// Calibration state
CalibrationData g_calibration;
bool g_calibrated = false;
CalibrationState g_last_cal_state = CAL_IDLE;

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

void configureLIS3DHInterrupt() {
    // Configure LIS3DH to wake when Z-axis < threshold (tilted >80° from vertical)

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
    readAccelReg(LIS3DH_REG_INT1SRC);

    Serial.print("INT pin state: ");
    Serial.println(digitalRead(LIS3DH_INT_PIN));
    Serial.println("LIS3DH configured: wake when Z < ~4000 (tilt >75°)");
}

void enterDeepSleep() {
    Serial.println("Entering deep sleep...");
    Serial.flush();

    // Configure wake-up interrupt from LIS3DH INT1 pin
    esp_sleep_enable_ext0_wakeup((gpio_num_t)LIS3DH_INT_PIN, 1);  // Wake on HIGH

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
#endif

void setup() {
    Serial.begin(115200);
    delay(1000);

    wakeTime = millis();

    // Initialize LED
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    // Print wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    Serial.println("=================================");
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Woke up from tilt/motion!");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Woke up from timer");
            break;
        default:
            Serial.println("Normal boot (power on/reset)");
            break;
    }
    Serial.println("=================================");

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

    // Draw welcome screen on first boot
    drawWelcomeScreen();

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

        if (g_time_valid) {
            Serial.println("Time configuration loaded:");
            Serial.print("  Timezone offset: ");
            Serial.println(g_timezone_offset);

            // Restore time from last boot (approximation until DS3231 added)
            uint32_t last_boot_time = storageLoadLastBootTime();
            if (last_boot_time > 0) {
                struct timeval tv;
                tv.tv_sec = last_boot_time;
                tv.tv_usec = 0;
                settimeofday(&tv, NULL);
                Serial.println("  RTC restored from last boot timestamp");
                Serial.println("  WARNING: Time may be inaccurate (DS3231 RTC recommended)");
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

    // Initialize serial command handler
    serialCommandsInit();
    serialCommandsSetTimeCallback(onTimeSet);
    Serial.println("Serial command handler initialized");

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

    // Initialize calibration state machine
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
    Serial.println("Display state tracking initialized");
#endif

    Serial.println("Setup complete!");
    Serial.print("Will sleep in ");
    Serial.print(AWAKE_DURATION_MS / 1000);
    Serial.println(" seconds...");
}

void loop() {
    // Check for serial commands
    serialCommandsUpdate();

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

    // Read accelerometer and get gesture (ONCE)
    if (lisReady) {
        sensors.gesture = gesturesUpdate(sensors.water_ml);
    }

    // NOW use snapshot for all logic below
    int32_t current_adc = sensors.adc_reading;
    float current_water_ml = sensors.water_ml;
    GestureType gesture = sensors.gesture;

    // Check calibration state
    CalibrationState cal_state = calibrationGetState();

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
                Serial.println("Main: Returning to main screen");
#if defined(BOARD_ADAFRUIT_FEATHER)
                displayForceUpdate();  // Force display refresh after calibration
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
            Serial.println("Main: Returning to main screen after error");
#if defined(BOARD_ADAFRUIT_FEATHER)
            displayForceUpdate();  // Force display refresh after error
#endif
        }
    }

    // Periodically check display state and update if needed (smart tracking)
    static unsigned long last_level_check = 0;
    static unsigned long last_time_check = 0;
    static unsigned long last_battery_check = 0;

    // Check display ONLY when bottle is upright stable (prevents flicker during movement)
    if (cal_state == CAL_IDLE && g_calibrated && gesture == GESTURE_UPRIGHT_STABLE) {

        // Check every DISPLAY_UPDATE_INTERVAL_MS when bottle is upright stable
        if (millis() - last_level_check >= DISPLAY_UPDATE_INTERVAL_MS) {
            last_level_check = millis();

#if defined(BOARD_ADAFRUIT_FEATHER)
            if (nauReady) {
                // FIX Bug #4: Use snapshot data instead of reading sensors again
                float display_water_ml = current_water_ml;

                // Clamp to valid range
                if (display_water_ml < 0) display_water_ml = 0;
                if (display_water_ml > 830) display_water_ml = 830;

                // Process drink tracking (we're already UPRIGHT_STABLE)
                if (g_time_valid) {
                    drinksUpdate(current_adc, g_calibration);
                }

                // Check interval timers for time and battery updates
                bool time_interval_elapsed = (millis() - last_time_check >= DISPLAY_TIME_UPDATE_INTERVAL_MS);
                bool battery_interval_elapsed = (millis() - last_battery_check >= DISPLAY_BATTERY_UPDATE_INTERVAL_MS);

                // Get current daily total
                DailyState daily_state;
                drinksGetState(daily_state);

                // Check if display needs update (water, daily intake, time, or battery changed)
                if (displayNeedsUpdate(display_water_ml, daily_state.daily_total_ml,
                                      time_interval_elapsed, battery_interval_elapsed)) {
                    displayUpdate(display_water_ml, daily_state.daily_total_ml);

                    // Reset interval timers if they triggered the update
                    if (time_interval_elapsed) last_time_check = millis();
                    if (battery_interval_elapsed) last_battery_check = millis();
                }
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

            unsigned long remaining = (AWAKE_DURATION_MS - (millis() - wakeTime)) / 1000;
            Serial.print("  (sleep in ");
            Serial.print(remaining);
            Serial.println("s)");
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

    // Check if it's time to sleep
    // DISABLED FOR CALIBRATION TESTING
    /*
    if (millis() - wakeTime >= AWAKE_DURATION_MS) {
        if (lisReady) {
            // Clear any pending interrupt before sleep
            readAccelReg(LIS3DH_REG_INT1SRC);

            Serial.print("INT pin before sleep: ");
            Serial.println(digitalRead(LIS3DH_INT_PIN));
        }
        enterDeepSleep();
    }
    */

    // Reduced delay for more responsive gesture detection
    delay(200);
}
