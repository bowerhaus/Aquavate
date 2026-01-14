/**
 * Aquavate - Gesture Detection Module
 * Detects bottle gestures using LIS3DH accelerometer
 */

#ifndef GESTURES_H
#define GESTURES_H

#include <Arduino.h>
#include <Adafruit_LIS3DH.h>

// Gesture types
enum GestureType {
    GESTURE_NONE,
    GESTURE_INVERTED_HOLD,    // Z < -0.8g for 5s (calibration trigger)
    GESTURE_UPRIGHT,          // Z > 0.985g, low variance (bottle on table - for display updates)
    GESTURE_UPRIGHT_STABLE,   // Upright + weight stable for 2s (for drink tracking)
    GESTURE_SIDEWAYS_TILT,    // |X| or |Y| > 0.5g (confirmation)
};

// Gesture state (returned by gesturesUpdate)
struct GestureState {
    GestureType type;             // Current gesture detected
    bool needs_acknowledgment;    // True if gesture needs to be acknowledged to prevent re-trigger
    uint32_t timestamp;           // When gesture was first detected (milliseconds)
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
void gesturesInit(Adafruit_LIS3DH& lis);

// Initialize with custom config
void gesturesInit(Adafruit_LIS3DH& lis, const GestureConfig& config);

// Update gesture detection (call regularly in loop)
// weight_ml: current weight reading in ml (negative if bottle is in the air)
// Returns GestureState with detected gesture and acknowledgment flag
GestureState gesturesUpdate(float weight_ml = 0.0f);

// Acknowledge a gesture to prevent re-triggering
// Call this after handling GESTURE_INVERTED_HOLD to allow next detection
void gestureAcknowledge(GestureType type);

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
