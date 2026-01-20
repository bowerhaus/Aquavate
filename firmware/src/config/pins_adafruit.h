/**
 * Pin definitions for Adafruit ESP32 Feather V2 + E-Paper FeatherWing
 *
 * I2C devices (STEMMA QT / Qwiic):
 *   - NAU7802 ADC (load cell): 0x2A
 *   - LIS3DH accelerometer: 0x18 or 0x19
 *   - DS3231 RTC: 0x68
 *
 * E-Paper FeatherWing connects via SPI (directly stacked)
 */

#ifndef PINS_ADAFRUIT_H
#define PINS_ADAFRUIT_H

// I2C pins (STEMMA QT connector)
#define PIN_I2C_SDA         22
#define PIN_I2C_SCL         20

// E-Paper FeatherWing SPI pins (directly stacked)
#define PIN_EPD_CS          9
#define PIN_EPD_DC          10
#define PIN_EPD_RESET       -1  // Not connected on FeatherWing
#define PIN_EPD_BUSY        5

// SPI pins (shared with E-Paper)
#define PIN_SPI_MOSI        35
#define PIN_SPI_MISO        37
#define PIN_SPI_SCK         36

// ADXL343 interrupt pin for wake-on-tilt
// NOTE: GPIO 33 conflicts with E-Paper FeatherWing DC pin
// Using GPIO 27 (A10 on Feather silkscreen) - physically wire INT1 to this pin
#define PIN_ACCEL_INT       27

// Battery monitoring
#define PIN_VBAT            A13  // Battery voltage divider
#define PIN_VBUS            A12  // USB voltage detection

// Onboard LED
#define PIN_LED             13

// E-Paper display dimensions
#define EPD_WIDTH           250
#define EPD_HEIGHT          122

#endif // PINS_ADAFRUIT_H
