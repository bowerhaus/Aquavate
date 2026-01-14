/**
 * Aquavate - Configuration Constants
 * Centralized configuration for hardware and power management
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== Debug Configuration ====================

// Enable/disable debug output (set to 0 to disable all verbose output)
#define DEBUG_ENABLED                   1   // 0 = quiet mode, 1 = verbose debug output
#define DEBUG_WATER_LEVEL               1   // 0 = disable water level messages
#define DEBUG_ACCELEROMETER             1   // 0 = disable accelerometer debug output
#define DEBUG_DISPLAY_UPDATES           1   // 0 = disable display update messages

// ==================== Power Management ====================

// How long to stay awake after waking from deep sleep (milliseconds)
#define AWAKE_DURATION_MS   30000   // 30 seconds

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

// ==================== Calibration ====================

// Gesture detection thresholds (in g units)
// When bottle is upright: Z ≈ +1.0g
// When bottle is inverted (upside down): Z ≈ -1.0g
// When bottle is horizontal (on its side): Z ≈ 0.0g
#define GESTURE_INVERTED_Z_THRESHOLD    -0.7f   // Z-axis threshold for inverted detection (less strict)
#define GESTURE_UPRIGHT_Z_THRESHOLD      0.996f // Z-axis threshold for upright detection (within 5° vertical)
#define GESTURE_SIDEWAYS_THRESHOLD       0.5f   // X/Y axis threshold for sideways tilt
// NOTE: UPRIGHT_STABLE now uses stricter hardcoded thresholds in gestures.cpp:
//       - Z >= 0.996g (cos(5°) for vertical alignment)
//       - |X|, |Y| <= 0.087g (sin(5°) for minimal tilt)
//       - Weight >= -50ml (bottle on table, not in air)

// Gesture timing (milliseconds)
#define GESTURE_INVERTED_HOLD_DURATION  5000    // 5 seconds to trigger calibration
#define GESTURE_STABILITY_DURATION      1000    // 1 second for stable detection

// Gesture stability
#define GESTURE_STABILITY_VARIANCE      0.02f   // Max variance (g^2) for stable detection (relaxed from 0.01)
#define GESTURE_SAMPLE_WINDOW_SIZE      10      // Number of samples for variance calculation

// Weight measurement
#define WEIGHT_MEASUREMENT_DURATION     10      // Measurement duration in seconds
#define WEIGHT_VARIANCE_THRESHOLD       6000.0f // Stable if variance < this (ADC units squared)
#define WEIGHT_MIN_SAMPLES              8       // Minimum samples required for valid measurement
#define WEIGHT_OUTLIER_STD_DEVS         2.0f    // Outlier threshold in standard deviations

// Calibration parameters
#define CALIBRATION_BOTTLE_VOLUME_ML    830.0f  // Full bottle volume (ml)
#define CALIBRATION_WATER_DENSITY       1.0f    // Water density (g/ml)

// Display update parameters
#define DISPLAY_UPDATE_INTERVAL_MS      5000    // Check for water level changes every 5 seconds
#define DISPLAY_UPDATE_THRESHOLD_ML     5.0f    // Only refresh display if water level changed by >5ml

// Daily intake display mode: 0=human figure, 1=tumbler grid
#define DAILY_INTAKE_DISPLAY_MODE       0       // 0=human figure (continuous fill), 1=tumbler grid (10 glasses)

// Drink detection parameters
#define DRINK_MIN_THRESHOLD_ML          30      // Minimum ml decrease to detect a drink
#define DRINK_REFILL_THRESHOLD_ML       100     // Minimum ml increase to detect a refill
#define DRINK_AGGREGATION_WINDOW_SEC    300     // Aggregate drinks within 5-minute window
#define DRINK_DAILY_RESET_HOUR          4       // Reset daily counter at 4am local time
#define DRINK_DISPLAY_UPDATE_THRESHOLD_ML 50    // Only refresh display if daily total changed by ≥50ml
#define DRINK_MAX_RECORDS               200     // Circular buffer capacity (7+ days at 20 drinks/day)
#define DRINK_DAILY_GOAL_ML             2500    // Hardcoded daily goal for MVP

// NVS Storage
#define NVS_NAMESPACE                   "aquavate"  // NVS namespace for calibration data

#endif // CONFIG_H
