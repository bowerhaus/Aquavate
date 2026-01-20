/**
 * Aquavate - Smart Water Bottle Firmware
 * Common definitions and configuration
 */

#ifndef AQUAVATE_H
#define AQUAVATE_H

#include <Arduino.h>

// Version info
#define AQUAVATE_VERSION_MAJOR  0
#define AQUAVATE_VERSION_MINOR  1
#define AQUAVATE_VERSION_PATCH  0
#define AQUAVATE_VERSION        "0.1.0"

// Include board-specific pin definitions
#if defined(BOARD_ADAFRUIT_FEATHER)
    #include "config/pins_adafruit.h"
#elif defined(BOARD_SPARKFUN_QWIIC)
    #include "config/pins_sparkfun.h"
#else
    #error "No board defined! Use -DBOARD_ADAFRUIT_FEATHER or -DBOARD_SPARKFUN_QWIIC"
#endif

// I2C device addresses
#define I2C_ADDR_NAU7802    0x2A
#define I2C_ADDR_ADXL343    0x53  // Default with SDO=LOW
#define I2C_ADDR_DS3231     0x68

// BLE configuration
#define BLE_DEVICE_NAME     "Aquavate"
#define BLE_SERVICE_UUID    "12345678-1234-5678-1234-56789abcdef0"

// Measurement settings
#define WEIGHT_SAMPLES      10      // Number of samples to average
#define TARE_SAMPLES        50      // Samples for tare calibration
#define MIN_DRINK_ML        10      // Minimum detectable drink (ml)

// Power management
#define DEEP_SLEEP_US       (60 * 1000000ULL)  // Wake every 60 seconds
#define DISPLAY_TIMEOUT_MS  10000              // Display off after 10 seconds

#endif // AQUAVATE_H
