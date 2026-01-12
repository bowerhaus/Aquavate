/**
 * Aquavate - Gesture Detection Module
 * Implementation
 */

#include "gestures.h"
#include "config.h"
#include <math.h>

// Static variables
static Adafruit_LIS3DH* g_lis = nullptr;
static GestureConfig g_config;
static bool g_initialized = false;

// Sample history for variance calculation
static float g_x_samples[GESTURE_SAMPLE_WINDOW_SIZE];
static float g_y_samples[GESTURE_SAMPLE_WINDOW_SIZE];
static float g_z_samples[GESTURE_SAMPLE_WINDOW_SIZE];
static int g_sample_index = 0;
static int g_sample_count = 0;

// Current accelerometer readings
static float g_current_x = 0.0f;
static float g_current_y = 0.0f;
static float g_current_z = 0.0f;

// Gesture timing
static uint32_t g_inverted_start_time = 0;
static bool g_inverted_active = false;
static bool g_inverted_triggered = false;  // Latch to prevent re-triggering

// Default configuration
static GestureConfig getDefaultConfig() {
    GestureConfig config;
    config.inverted_z_threshold = GESTURE_INVERTED_Z_THRESHOLD;
    config.upright_z_threshold = GESTURE_UPRIGHT_Z_THRESHOLD;
    config.sideways_threshold = GESTURE_SIDEWAYS_THRESHOLD;
    config.inverted_hold_duration = GESTURE_INVERTED_HOLD_DURATION;
    config.stability_duration = GESTURE_STABILITY_DURATION;
    config.stability_variance = GESTURE_STABILITY_VARIANCE;
    config.sample_window_size = GESTURE_SAMPLE_WINDOW_SIZE;
    return config;
}

void gesturesInit(Adafruit_LIS3DH& lis) {
    gesturesInit(lis, getDefaultConfig());
}

void gesturesInit(Adafruit_LIS3DH& lis, const GestureConfig& config) {
    g_lis = &lis;
    g_config = config;
    g_initialized = true;
    g_sample_count = 0;
    g_sample_index = 0;
    g_inverted_active = false;
    g_inverted_triggered = false;
    g_inverted_start_time = 0;
}

// Calculate mean of samples
static float calculateMean(const float* samples, int count) {
    if (count == 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += samples[i];
    }
    return sum / count;
}

// Calculate variance of samples
static float calculateVariance(const float* samples, int count) {
    if (count < 2) return 0.0f;

    float mean = calculateMean(samples, count);
    float sum_sq_diff = 0.0f;

    for (int i = 0; i < count; i++) {
        float diff = samples[i] - mean;
        sum_sq_diff += diff * diff;
    }

    return sum_sq_diff / count;
}

// Add current reading to sample history
static void addSample(float x, float y, float z) {
    g_x_samples[g_sample_index] = x;
    g_y_samples[g_sample_index] = y;
    g_z_samples[g_sample_index] = z;

    g_sample_index = (g_sample_index + 1) % g_config.sample_window_size;

    if (g_sample_count < g_config.sample_window_size) {
        g_sample_count++;
    }
}

// Convert raw accelerometer reading to g units
static float rawToGs(int16_t raw) {
    // LIS3DH at ±2g range: 16-bit value, ~16000 = 1g
    return raw / 16000.0f;
}

GestureType gesturesUpdate() {
    if (!g_initialized || !g_lis) {
        return GESTURE_NONE;
    }

    // Read accelerometer
    g_lis->read();
    g_current_x = rawToGs(g_lis->x);
    g_current_y = rawToGs(g_lis->y);
    g_current_z = rawToGs(g_lis->z);

    // Add to sample history
    addSample(g_current_x, g_current_y, g_current_z);

    // Check for inverted hold gesture (highest priority - triggers calibration)
    if (g_current_z < g_config.inverted_z_threshold) {
        if (!g_inverted_active) {
            g_inverted_active = true;
            g_inverted_triggered = false;
            g_inverted_start_time = millis();
            Serial.print("Gestures: Inverted detected! Z=");
            Serial.print(g_current_z);
            Serial.println("g - hold for 5 seconds...");
        } else if (!g_inverted_triggered) {
            // Check if held long enough
            uint32_t held_duration = millis() - g_inverted_start_time;
            if (held_duration >= g_config.inverted_hold_duration) {
                Serial.println("Gestures: INVERTED_HOLD gesture triggered!");
                g_inverted_triggered = true;  // Latch to prevent re-triggering
                return GESTURE_INVERTED_HOLD;
            }
            // Debug: show progress every second
            if (held_duration % 1000 < 500) {
                Serial.print("Gestures: Holding inverted... ");
                Serial.print(held_duration / 1000);
                Serial.println("s");
            }
        } else {
            // Already triggered - keep returning the gesture until bottle is no longer inverted
            return GESTURE_INVERTED_HOLD;
        }
    } else {
        // Reset inverted state if bottle no longer inverted
        if (g_inverted_active) {
            Serial.println("Gestures: Bottle returned to normal position");
        }
        g_inverted_active = false;
        g_inverted_triggered = false;
    }

    // Check for sideways tilt (confirmation gesture)
    if (fabs(g_current_x) > g_config.sideways_threshold ||
        fabs(g_current_y) > g_config.sideways_threshold) {
        return GESTURE_SIDEWAYS_TILT;
    }

    // Check for upright stable (bottle placement)
    if (g_current_z > g_config.upright_z_threshold) {
        if (gesturesIsStable()) {
            return GESTURE_UPRIGHT_STABLE;
        }
    }

    return GESTURE_NONE;
}

const GestureConfig& gesturesGetConfig() {
    return g_config;
}

bool gesturesIsStable() {
    if (g_sample_count < g_config.sample_window_size) {
        return false; // Not enough samples yet
    }

    // Calculate variance for all axes
    float x_var = calculateVariance(g_x_samples, g_sample_count);
    float y_var = calculateVariance(g_y_samples, g_sample_count);
    float z_var = calculateVariance(g_z_samples, g_sample_count);

    // Total variance (sum of all axes)
    float total_var = x_var + y_var + z_var;

    return total_var < g_config.stability_variance;
}

void gesturesGetAccel(float& x, float& y, float& z) {
    x = g_current_x;
    y = g_current_y;
    z = g_current_z;
}

float gesturesGetVariance() {
    if (g_sample_count < 2) {
        return 999.0f; // Very high variance if not enough samples
    }

    float x_var = calculateVariance(g_x_samples, g_sample_count);
    float y_var = calculateVariance(g_y_samples, g_sample_count);
    float z_var = calculateVariance(g_z_samples, g_sample_count);

    return x_var + y_var + z_var;
}

void gesturesReset() {
    g_sample_count = 0;
    g_sample_index = 0;
    g_inverted_active = false;
    g_inverted_triggered = false;
    g_inverted_start_time = 0;
}
