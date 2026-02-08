/**
 * Aquavate - Configuration Constants
 * Centralized configuration for hardware and power management
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>  // For uint8_t, uint32_t types

// ==================== Feature Flags ====================

// Master Mode Selection
// Set IOS_MODE to control build configuration:
//   IOS_MODE=1: iOS App Mode (Production - default)
//     - BLE enabled for iOS app communication
//     - Serial commands disabled (saves ~3.7KB IRAM)
//     - Standalone calibration disabled (saves ~2.4KB IRAM)
//     - IRAM usage: ~125KB / 131KB (95.3%)
//     - Headroom: ~6.1KB
//
//   IOS_MODE=0: Standalone USB Mode (Development/Configuration)
//     - BLE disabled (saves ~45.5KB IRAM)
//     - Serial commands enabled for USB configuration
//     - Standalone calibration enabled (inverted-hold trigger + UI)
//     - IRAM usage: ~82KB / 131KB (62.4%)
//     - Headroom: ~49.2KB

#define IOS_MODE    1   // Production: BLE enabled, serial commands disabled

// Auto-configure feature flags based on IOS_MODE
#if IOS_MODE
    #define ENABLE_BLE                      1
    #define ENABLE_SERIAL_COMMANDS          0
    #define ENABLE_STANDALONE_CALIBRATION   1   // Bottle-driven calibration (iOS mirrors state)
#else
    #define ENABLE_BLE                      0
    #define ENABLE_SERIAL_COMMANDS          1
    #define ENABLE_STANDALONE_CALIBRATION   1   // Keep for USB mode
#endif

// Sanity check: Verify mutual exclusivity
#if ENABLE_BLE && ENABLE_SERIAL_COMMANDS
#error "Cannot enable both ENABLE_BLE and ENABLE_SERIAL_COMMANDS - IRAM overflow! Check IOS_MODE configuration."
#endif

// ==================== Debug Configuration ====================

// Debug levels (runtime control via serial commands '0'-'4', '9')
// Level 0: All debug output OFF (quiet mode)
// Level 1: Events (drink tracking, bottle refills, display updates)
// Level 2: + Gestures (gesture detection, state changes, calibration)
// Level 3: + Weight readings (load cell ADC values, water levels)
// Level 4: + Accelerometer raw data (periodic accelerometer readings)
// Level 9: All debug ON (all categories enabled, room for future expansion)

// Default debug flags (can be overridden at runtime via serial commands)
#define DEBUG_ENABLED                   1   // 0 = quiet mode, 1 = verbose debug output
#define DEBUG_WATER_LEVEL               1   // 0 = disable water level messages
#define DEBUG_ACCELEROMETER             1   // 0 = disable accelerometer debug output
#define DEBUG_DISPLAY_UPDATES           1   // 0 = disable display update messages
#define DEBUG_DRINK_TRACKING            1   // 0 = disable drink detection debug
#define DEBUG_CALIBRATION               1   // 0 = disable calibration debug
#define DEBUG_BLE                       1   // 0 = disable BLE debug, 1 = enable BLE debug

// Runtime debug control - these extern declarations allow runtime debug control
// Use these macros in your code instead of #if DEBUG_* for runtime control
#ifndef CONFIG_H_GLOBALS_ONLY
extern bool g_debug_enabled;
extern bool g_debug_water_level;
extern bool g_debug_accelerometer;
extern bool g_debug_display;
extern bool g_debug_drink_tracking;
extern bool g_debug_calibration;
extern bool g_debug_ble;

// Runtime display mode control
extern uint8_t g_daily_intake_display_mode;

// Helper macros for conditional debug output (runtime control)
#define DEBUG_PRINT(category, ...) \
    do { \
        if (g_debug_enabled && category) { \
            Serial.print(__VA_ARGS__); \
        } \
    } while(0)

#define DEBUG_PRINTLN(category, ...) \
    do { \
        if (g_debug_enabled && category) { \
            Serial.println(__VA_ARGS__); \
        } \
    } while(0)

#define DEBUG_PRINTF(category, ...) \
    do { \
        if (g_debug_enabled && category) { \
            Serial.printf(__VA_ARGS__); \
        } \
    } while(0)
#endif

// ==================== Power Management ====================
//
// Plan 034: Two-Timer Sleep Model
// ================================
// The firmware uses two simple timers to manage sleep behavior:
//
// Timer 1: Activity Timeout (ACTIVITY_TIMEOUT_MS)
//   Purpose: Enter sleep when device is idle
//   Resets on: gesture change, BLE data activity (sync, commands)
//   Does NOT reset on: BLE connect/disconnect alone
//   When expires: Stop advertising, enter normal deep sleep (motion wake)
//
// Timer 2: Time Since Stable (TIME_SINCE_STABLE_THRESHOLD_SEC)
//   Purpose: Detect "backpack mode" - constant motion prevents normal operation
//   Resets when: bottle becomes UPRIGHT_STABLE (placed on flat surface)
//   Counts when: bottle is in motion / not stable
//   When expires: Enter extended deep sleep (timer wake every 60s)
//
// Normal use: Timer 2 keeps resetting when bottle is placed down
// Backpack: Constant motion, timer 2 never resets, extended sleep after 3 min
//
// BLE advertising: Tied to awake state - advertising stops when device sleeps

// Timer 1: Activity timeout - how long to stay awake after last activity
// Resets on: gesture change, BLE data activity (sync, commands)
#define ACTIVITY_TIMEOUT_MS             30000   // 30 seconds idle → enter sleep
#define ACTIVITY_TIMEOUT_EXTENDED_MS   240000   // 4 minutes when unsynced records exist (background sync)

// Timer 2: Extended deep sleep configuration (backpack mode)
// When bottle hasn't been stable (UPRIGHT_STABLE) for threshold duration,
// switch to tap-based wake instead of motion wake to conserve battery
#define TIME_SINCE_STABLE_THRESHOLD_SEC 180     // 3 minutes without stability triggers backpack mode

// Health-check wake: periodic timer wake in all deep sleep modes
// Ensures device auto-recovers after battery depletion + recharge
// Battery impact: ~1mAh/day (negligible vs 400-1000mAh LiPo)
#define HEALTH_CHECK_WAKE_INTERVAL_SEC  7200    // 2 hours

// Display "Zzzz" indicator before entering deep sleep
// 0 = No display update before sleep (saves battery, no flash)
// 1 = Show "Zzzz" indicator before sleep (visual feedback)
#define DISPLAY_SLEEP_INDICATOR     0
#define EXTENDED_SLEEP_INDICATOR    1           // Show "Zzzz" before extended sleep (1=enabled)

// Activity detection for normal sleep wake (ADXL343 AC-coupled activity interrupt)
#define ACTIVITY_WAKE_THRESHOLD     0x08    // 0.5g threshold (8 x 62.5mg/LSB) - detects tilt to pour

// Tap detection for backpack mode wake (ADXL343 double-tap interrupt)
#define TAP_WAKE_THRESHOLD          0x30    // 3.0g threshold (48 x 62.5mg/LSB) - firm tap required
#define TAP_WAKE_DURATION           0x10    // 10ms max duration (16 x 625us/LSB) - short sharp tap
#define TAP_WAKE_LATENT             0x50    // 100ms latency (80 x 1.25ms/LSB) - between taps
#define TAP_WAKE_WINDOW             0xF0    // 300ms window (240 x 1.25ms/LSB) - for second tap

// ==================== ADXL343 Accelerometer ====================

// Note: PIN_ACCEL_INT is defined in board-specific pins_*.h files
// (pins_adafruit.h: pin 33, pins_sparkfun.h: pin 5)

// Tilt detection threshold for wake-on-tilt
// ADXL343 at ±2g: 13-bit resolution, 256 LSB/g (4 mg/LSB)
// Threshold scale: 62.5 mg/LSB (different from data read resolution!)
//
// Physical orientation: Y-axis points up (Y ≈ -1.0g when upright)
// Using inactivity interrupt: triggers when |Y| drops below threshold (bottle tilts)
//
// Threshold calculation: register_value × 0.0625g = threshold in g
// Threshold = 0.80g = 800 mg → 800 / 62.5 = 12.8 ≈ 13 (0x0D)
//
// When upright: Y ≈ -1.0g (|Y| = 1.0g) → no interrupt
// When tilted >12°: |Y| < 0.81g → interrupt triggers
#define ADXL343_TILT_WAKE_THRESHOLD   0x0D  // 13 × 62.5mg = 0.8125g

// Alternative threshold values (for experimentation):
// #define ADXL343_TILT_THRESHOLD_0_75G    0x0C  // 12 × 62.5mg = 0.75g (more sensitive)
// #define ADXL343_TILT_THRESHOLD_0_875G   0x0E  // 14 × 62.5mg = 0.875g (less sensitive)

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

// Shake-while-inverted gesture (shake to empty / bottle emptied)
#define GESTURE_SHAKE_INVERTED_Y_THRESHOLD  -0.3f   // Y > -0.3g for ~70° tilt (inverted)
#define GESTURE_SHAKE_VARIANCE_THRESHOLD    0.08f   // Variance > 0.08g² indicates shaking
#define GESTURE_SHAKE_DURATION_MS           1500    // 1.5 seconds of shaking required
#define BOTTLE_EMPTY_THRESHOLD_ML           50      // Bottle considered empty if <50ml remaining

// Weight measurement
#define WEIGHT_MEASUREMENT_DURATION     5       // Measurement duration in seconds
#define WEIGHT_VARIANCE_THRESHOLD       6000.0f // Stable if variance < this (ADC units squared)
#define WEIGHT_MIN_SAMPLES              8       // Minimum samples required for valid measurement
#define WEIGHT_OUTLIER_STD_DEVS         2.0f    // Outlier threshold in standard deviations

// Calibration parameters
#define CALIBRATION_BOTTLE_VOLUME_ML    830.0f  // Full bottle volume (ml)
#define CALIBRATION_WATER_DENSITY       1.0f    // Water density (g/ml)

// Calibration scale factor bounds (ADC counts per gram)
// Based on NAU7802 at 128 gain with typical load cells:
// Expected range: ~200-600 ADC/g, with margin for variation
// Values outside this range indicate corrupt calibration
#define CALIBRATION_SCALE_FACTOR_MIN    100.0f  // Minimum valid scale factor
#define CALIBRATION_SCALE_FACTOR_MAX    800.0f  // Maximum valid scale factor

// Calibration UI timeouts (milliseconds)
#define CAL_STARTED_DISPLAY_DURATION    3000    // 3 seconds - "Calibration Started" screen
#define CAL_WAIT_EMPTY_TIMEOUT          60000   // 60 seconds - Empty bottle prompt timeout
#define CAL_WAIT_FULL_TIMEOUT           120000  // 120 seconds - Full bottle prompt timeout

// Display update parameters
#define DISPLAY_UPDATE_INTERVAL_MS      5000    // Check for water level changes every 5 seconds
#define DISPLAY_UPDATE_THRESHOLD_ML     5.0f    // Only refresh display if water level changed by >5ml

// Daily intake display mode: 0=human figure, 1=tumbler grid
#define DAILY_INTAKE_DISPLAY_MODE       0       // 0=human figure (continuous fill), 1=tumbler grid (10 glasses)

// Drink detection parameters
#define DRINK_MIN_THRESHOLD_ML          30      // Minimum ml decrease to detect a drink
#define DRINK_REFILL_THRESHOLD_ML       100     // Minimum ml increase to detect a refill
#define DRINK_DAILY_RESET_HOUR          0       // Reset daily counter at midnight (aligns with HealthKit)
#define DRINK_DISPLAY_UPDATE_THRESHOLD_ML 50    // Only refresh display if daily total changed by ≥50ml
#define DRINK_MAX_RECORDS               600     // Circular buffer capacity (30 days at 20 drinks/day)
#define DRINK_DAILY_GOAL_MIN_ML         1000    // Minimum configurable goal
#define DRINK_DAILY_GOAL_MAX_ML         4000    // Maximum configurable goal
#define DRINK_DAILY_GOAL_DEFAULT_ML     2500    // Default daily goal (persisted to NVS)
#define DRINK_DRIFT_THRESHOLD_ML        15      // Max delta for drift compensation (avoids baseline contamination)

// Drink type classification
#define DRINK_TYPE_GULP                 0       // Small drink (<100ml)
#define DRINK_TYPE_POUR                 1       // Large drink (≥100ml)
#define DRINK_GULP_THRESHOLD_ML         100     // Threshold for gulp vs pour classification

// Display update parameters
#define DISPLAY_TIME_UPDATE_INTERVAL_MS     900000  // Check time every 15 minutes (900000ms)
#define DISPLAY_TIME_UPDATE_THRESHOLD_MIN   15      // Update display if time changed by ≥15 minutes
#define DISPLAY_BATTERY_UPDATE_INTERVAL_MS  900000  // Check battery every 15 minutes
#define DISPLAY_BATTERY_UPDATE_THRESHOLD    20      // Update display if battery changed by ≥20%

// NVS Storage
#define NVS_NAMESPACE                   "aquavate"  // NVS namespace for calibration data

#endif // CONFIG_H
