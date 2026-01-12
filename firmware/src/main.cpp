/**
 * Aquavate - Smart Water Bottle Firmware
 * Main entry point
 */

#include <Arduino.h>
#include "aquavate.h"

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

    // Initialize LED
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

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

    // Blink LED for 10 seconds on startup
    blinkLED(10);

    Serial.println("Initializing...");

    // TODO: Initialize I2C
    // TODO: Initialize sensors (NAU7802, LIS3DH, DS3231)
    // TODO: Initialize E-Paper display
    // TODO: Initialize BLE
    // TODO: Load calibration from NVS

    Serial.println("Setup complete!");
}

void loop() {
    // TODO: Check for accelerometer interrupt (wake-on-tilt)
    // TODO: Read weight sensor
    // TODO: Calculate water consumption
    // TODO: Update display if needed
    // TODO: Handle BLE communication
    // TODO: Enter deep sleep when idle

    delay(1000);  // Placeholder - remove when implementing
}
