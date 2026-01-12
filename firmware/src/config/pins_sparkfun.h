/**
 * Pin definitions for SparkFun ESP32-C6 Qwiic Pocket + Waveshare E-Paper
 *
 * I2C devices (Qwiic connector):
 *   - NAU7802 ADC (load cell): 0x2A
 *   - LIS3DH accelerometer: 0x18 or 0x19
 *   - DS3231 RTC: 0x68
 *
 * Waveshare 1.54" E-Paper connects via SPI (requires manual wiring)
 */

#ifndef PINS_SPARKFUN_H
#define PINS_SPARKFUN_H

// I2C pins (Qwiic connector)
#define PIN_I2C_SDA         6
#define PIN_I2C_SCL         7

// Waveshare 1.54" E-Paper SPI pins (manual wiring required)
#define PIN_EPD_CS          18
#define PIN_EPD_DC          19
#define PIN_EPD_RESET       20
#define PIN_EPD_BUSY        21

// SPI pins
#define PIN_SPI_MOSI        23
#define PIN_SPI_MISO        22  // Not used by E-Paper
#define PIN_SPI_SCK         4

// LIS3DH interrupt pin for wake-on-tilt
#define PIN_ACCEL_INT       5

// Battery monitoring (via external divider if needed)
#define PIN_VBAT            A0

// Onboard LED (standard blue LED, not WS2812)
#define PIN_LED             23

// E-Paper display dimensions (Waveshare 1.54")
#define EPD_WIDTH           200
#define EPD_HEIGHT          200

#endif // PINS_SPARKFUN_H
