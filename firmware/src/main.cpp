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

Adafruit_NAU7802 nau;
Adafruit_LIS3DH lis = Adafruit_LIS3DH();
bool nauReady = false;
bool lisReady = false;

unsigned long wakeTime = 0;

// Calibration state
CalibrationData g_calibration;
bool g_calibrated = false;
CalibrationState g_last_cal_state = CAL_IDLE;

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

void drawBatteryIcon(int x, int y, int percent) {
    // Battery outline (20x12 pixels)
    display.drawRect(x, y, 20, 12, EPD_BLACK);
    display.fillRect(x + 20, y + 3, 3, 6, EPD_BLACK);  // Positive terminal

    // Fill level (max 16px wide inside)
    int fillWidth = (percent * 16) / 100;
    if (fillWidth > 0) {
        display.fillRect(x + 2, y + 2, fillWidth, 8, EPD_BLACK);
    }
}
#endif

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
void drawMainScreen() {
    Serial.println("Drawing main screen...");
    display.clearBuffer();
    display.setTextColor(EPD_BLACK);

    // Calculate current water level in ml
    float water_ml = 0.0f;
    if (g_calibrated && nauReady) {
        if (nau.available()) {
            int32_t current_adc = nau.read();
            water_ml = calibrationGetWaterWeight(current_adc, g_calibration);

            Serial.print("Main Screen: ADC=");
            Serial.print(current_adc);
            Serial.print(" Water=");
            Serial.print(water_ml, 1);
            Serial.print("ml (Scale factor=");
            Serial.print(g_calibration.scale_factor, 2);
            Serial.println(")");

            // Clamp to 0-830ml range
            if (water_ml < 0) water_ml = 0;
            if (water_ml > 830) water_ml = 830;
        }
    } else {
        // Not calibrated - show 830ml as placeholder
        water_ml = 830.0f;
        Serial.println("Main Screen: Not calibrated - showing 830ml");
    }

    // Draw vertical bottle graphic on left side
    int bottle_x = 10;  // Left side of screen
    int bottle_y = 20;   // Top margin
    int bottle_width = 40;
    int bottle_height = 90;
    int bottle_body_height = 70;  // Main body height
    int neck_height = 10;
    int cap_height = 10;

    // Calculate fill level (0-100%)
    float fill_percent = water_ml / 830.0f;
    int fill_height = (int)(bottle_body_height * fill_percent);

    // Draw bottle body (vertical cylinder)
    display.fillRoundRect(bottle_x, bottle_y + cap_height + neck_height,
                         bottle_width, bottle_body_height, 8, EPD_BLACK);
    display.fillRoundRect(bottle_x + 2, bottle_y + cap_height + neck_height + 2,
                         bottle_width - 4, bottle_body_height - 4, 6, EPD_WHITE);

    // Draw neck (narrower at top)
    int neck_width = bottle_width - 12;
    int neck_x = bottle_x + 6;
    display.fillRect(neck_x, bottle_y + cap_height, neck_width, neck_height, EPD_BLACK);
    display.fillRect(neck_x + 2, bottle_y + cap_height + 2, neck_width - 4, neck_height - 4, EPD_WHITE);

    // Draw cap
    int cap_width = neck_width - 4;
    int cap_x = neck_x + 2;
    display.fillRect(cap_x, bottle_y, cap_width, cap_height, EPD_BLACK);

    // Draw water level inside bottle (fill from bottom)
    if (fill_height > 0) {
        int water_y = bottle_y + cap_height + neck_height + bottle_body_height - fill_height;
        display.fillRoundRect(bottle_x + 4, water_y,
                             bottle_width - 8, fill_height - 2, 4, EPD_BLACK);
    }

    // Draw large text showing milliliters (right side)
    display.setTextSize(3);
    char ml_text[16];
    snprintf(ml_text, sizeof(ml_text), "%dml", (int)water_ml);

    // Position text on right side (x=80, centered vertically)
    display.setCursor(80, 50);
    display.print(ml_text);

    // Draw battery status in top-right corner
    float batteryV = getBatteryVoltage();
    int batteryPct = getBatteryPercent(batteryV);
    drawBatteryIcon(200, 5, batteryPct);

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

    // Draw main screen
    drawMainScreen();

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
    } else {
        Serial.println("Storage initialization failed");
    }

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

    Serial.println("Setup complete!");
    Serial.print("Will sleep in ");
    Serial.print(AWAKE_DURATION_MS / 1000);
    Serial.println(" seconds...");
}

void loop() {
    // Update gesture detection
    GestureType gesture = GESTURE_NONE;
    if (lisReady) {
        gesture = gesturesUpdate();
    }

    // Get current load cell reading
    int32_t current_adc = 0;
    if (nauReady && nau.available()) {
        current_adc = nau.read();
    }

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
        if (g_calibrated && nauReady) {
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
                drawMainScreen();
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
            drawMainScreen();
        }
    }

    // Update main screen when bottle is picked up and placed back down
    // Track gesture state to detect tilt → upright stable transitions
    static GestureType last_gesture = GESTURE_NONE;
    static bool bottle_was_tilted = false;

    if (cal_state == CAL_IDLE && g_calibrated) {
        // Detect when bottle is tilted (picked up)
        if (gesture != GESTURE_UPRIGHT_STABLE && gesture != GESTURE_NONE) {
            bottle_was_tilted = true;
        }

        // Detect when bottle returns to upright stable after being tilted
        if (bottle_was_tilted && gesture == GESTURE_UPRIGHT_STABLE && last_gesture != GESTURE_UPRIGHT_STABLE) {
#if defined(BOARD_ADAFRUIT_FEATHER)
            Serial.println("Main: Bottle placed down - updating water level display");
            drawMainScreen();
#endif
            bottle_was_tilted = false;
        }
    }

    last_gesture = gesture;

    // Debug output (only print periodically to reduce serial spam)
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
