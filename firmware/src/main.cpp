/**
 * Aquavate - Smart Water Bottle Firmware
 * Main entry point
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NAU7802.h>
#include <Adafruit_LIS3DH.h>
#include "aquavate.h"
#include "config.h"

Adafruit_NAU7802 nau;
Adafruit_LIS3DH lis = Adafruit_LIS3DH();
bool nauReady = false;
bool lisReady = false;

unsigned long wakeTime = 0;

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
    display.clearBuffer();
    display.setTextColor(EPD_BLACK);
    display.setTextSize(2);

    // Centre "Hello Sally" (11 chars * 12px = 132px, display width 250px)
    display.setCursor((250 - 132) / 2, 30);
    display.print("Hello Sally");

    // Centre "Your bottle is full" (19 chars * 12px = 228px)
    display.setCursor((250 - 228) / 2, 60);
    display.print("Your bottle is full");

    // Draw bottle on its side (centered below text)
    int bx = 85;   // bottle start x
    int by = 90;   // bottle y position

    // Bottle body (horizontal rectangle)
    display.fillRoundRect(bx, by, 60, 24, 4, EPD_BLACK);
    display.fillRoundRect(bx + 2, by + 2, 56, 20, 3, EPD_WHITE);

    // Bottle neck (smaller rectangle on the right)
    display.fillRect(bx + 60, by + 6, 15, 12, EPD_BLACK);
    display.fillRect(bx + 62, by + 8, 11, 8, EPD_WHITE);

    // Cap
    display.fillRect(bx + 75, by + 4, 6, 16, EPD_BLACK);

    // Water inside (filled portion)
    display.fillRoundRect(bx + 4, by + 4, 50, 16, 2, EPD_BLACK);

    // Draw battery status in top-right corner
    float batteryV = getBatteryVoltage();
    int batteryPct = getBatteryPercent(batteryV);
    drawBatteryIcon(200, 5, batteryPct);

    Serial.print("Battery: ");
    Serial.print(batteryV);
    Serial.print("V (");
    Serial.print(batteryPct);
    Serial.println("%)");

    display.display();
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

    Serial.println("Setup complete!");
    Serial.print("Will sleep in ");
    Serial.print(AWAKE_DURATION_MS / 1000);
    Serial.println(" seconds...");
}

void loop() {
    // Check if it's time to sleep
    if (millis() - wakeTime >= AWAKE_DURATION_MS) {
        if (lisReady) {
            // Clear any pending interrupt before sleep
            readAccelReg(LIS3DH_REG_INT1SRC);

            Serial.print("INT pin before sleep: ");
            Serial.println(digitalRead(LIS3DH_INT_PIN));
        }
        enterDeepSleep();
    }

    // Read and print load cell value
    if (nauReady && nau.available()) {
        int32_t reading = nau.read();
        Serial.print("Load cell: ");
        Serial.println(reading);
    }

    // Read and print accelerometer values
    if (lisReady) {
        lis.read();
        Serial.print("Accel X: ");
        Serial.print(lis.x);
        Serial.print("  Y: ");
        Serial.print(lis.y);
        Serial.print("  Z: ");
        Serial.print(lis.z);

        // Show time remaining and INT pin state
        unsigned long remaining = (AWAKE_DURATION_MS - (millis() - wakeTime)) / 1000;
        Serial.print("  INT:");
        Serial.print(digitalRead(LIS3DH_INT_PIN));
        Serial.print(" (sleep in ");
        Serial.print(remaining);
        Serial.println("s)");
    }

    delay(500);
}
