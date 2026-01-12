/**
 * Aquavate - Configuration Constants
 * Centralized configuration for hardware and power management
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== Power Management ====================

// How long to stay awake after waking from deep sleep (milliseconds)
#define AWAKE_DURATION_MS   15000   // 15 seconds

// ==================== LIS3DH Accelerometer ====================

// GPIO pin for accelerometer interrupt (must be RTC-capable for wake)
#define LIS3DH_INT_PIN      27

// Tilt detection threshold for wake-on-tilt
// This value determines when the bottle is considered "tilted" enough to wake
// Threshold unit: LSB at ±2g scale (each LSB ≈ 16mg)
//
// Calibrated values:
// - Vertical (0°): Z-axis reads ~15808 (~1g)
// - At 80° tilt: Z-axis reads ~4000 (~0.25g)
// - Threshold 0x38 (56 LSB) = wake when tilted >80° from vertical
#define LIS3DH_TILT_THRESHOLD_80_DEG    0x38

// Alternative threshold values (for experimentation):
// #define LIS3DH_TILT_THRESHOLD_70_DEG    0x30  // More sensitive (wakes at 70°)
// #define LIS3DH_TILT_THRESHOLD_85_DEG    0x40  // Less sensitive (wakes at 85°)

// Active tilt threshold (change this to adjust sensitivity)
#define LIS3DH_TILT_WAKE_THRESHOLD   LIS3DH_TILT_THRESHOLD_80_DEG

// ==================== Battery Monitoring ====================

#if defined(BOARD_ADAFRUIT_FEATHER)
    #define VBAT_PIN    A13  // GPIO 35 - battery voltage (divided by 2)
#endif

// LiPo voltage ranges for percentage calculation
#define BATTERY_VOLTAGE_FULL    4.2f  // 100%
#define BATTERY_VOLTAGE_EMPTY   3.2f  // 0%

// ==================== E-Paper Display ====================

#if defined(BOARD_ADAFRUIT_FEATHER)
    // 2.13" Mono E-Paper FeatherWing pin definitions for ESP32 Feather
    #define EPD_DC      33
    #define EPD_CS      15
    #define EPD_BUSY    -1
    #define SRAM_CS     32
    #define EPD_RESET   -1
#endif

#endif // CONFIG_H
