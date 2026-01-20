/**
 * Aquavate - Gesture Detection Module
 * Detects bottle gestures using ADXL343 accelerometer
 */

#ifndef GESTURES_H
#define GESTURES_H

#include <Arduino.h>
#include <Adafruit_ADXL343.h>

// Gesture types
enum GestureType {
    GESTURE_NONE,
    GESTURE_INVERTED_HOLD,        // Z < -0.8g for 5s (calibration trigger)
    GESTURE_UPRIGHT,              // Z > 0.985g, low variance (bottle on table - for display updates)
    GESTURE_UPRIGHT_STABLE,       // Upright + weight stable for 2s (for drink tracking)
    GESTURE_SIDEWAYS_TILT,        // |X| or |Y| > 0.5g (confirmation)
    GESTURE_SHAKE_WHILE_INVERTED, // Shake while inverted for 1.5s (shake to empty)
};

// Gesture detection configuration
struct GestureConfig {
    // Thresholds (in g units)
    float inverted_z_threshold;      // -0.8g for inverted detection
    float upright_z_threshold;       // 0.9g for upright detection
    float sideways_threshold;        // 0.5g for sideways tilt

    // Timing (milliseconds)
    uint32_t inverted_hold_duration; // 5000ms for calibration trigger
    uint32_t stability_duration;     // 1000ms for stable detection

    // Stability threshold (variance in g^2)
    float stability_variance;        // 0.01g^2 max variance for stable

    // Sample window size
    int sample_window_size;          // 10 samples for variance calculation
};

// Initialize gesture detection with default config
void gesturesInit(Adafruit_ADXL343& adxl);

// Initialize with custom config
void gesturesInit(Adafruit_ADXL343& adxl, const GestureConfig& config);

// Update gesture detection (call regularly in loop)
// weight_ml: current weight reading in ml (negative if bottle is in the air)
GestureType gesturesUpdate(float weight_ml = 0.0f);

// Get current gesture config
const GestureConfig& gesturesGetConfig();

// Check if bottle is currently stable (low motion)
bool gesturesIsStable();

// Get current accelerometer readings in g units
void gesturesGetAccel(float& x, float& y, float& z);

// Calculate variance of recent samples
float gesturesGetVariance();

// Reset gesture state (useful after handling a gesture)
void gesturesReset();

#endif // GESTURES_H
