/**
 * Aquavate - Gesture Detection Module
 * Implementation
 */

#include "gestures.h"
#include "config.h"
#include <math.h>

// Runtime debug control (managed in main.cpp)
extern bool g_debug_enabled;
extern bool g_debug_accelerometer;
extern bool g_debug_calibration;

// Static variables
static Adafruit_ADXL343* g_adxl = nullptr;
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
static uint32_t g_inverted_cooldown_end = 0;  // FIX Bug #3: Cooldown timer to prevent double-trigger

// Weight stability tracking for UPRIGHT_STABLE
static float g_last_stable_weight = 0.0f;
static uint32_t g_upright_start_time = 0;
static bool g_upright_active = false;

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

void gesturesInit(Adafruit_ADXL343& adxl) {
    gesturesInit(adxl, getDefaultConfig());
}

void gesturesInit(Adafruit_ADXL343& adxl, const GestureConfig& config) {
    g_adxl = &adxl;
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
    // ADXL343 at ±2g: 13-bit resolution, 256 LSB/g (4 mg/LSB)
    return raw / 256.0f;
}

GestureType gesturesUpdate(float weight_ml) {
    if (!g_initialized || !g_adxl) {
        return GESTURE_NONE;
    }

    // Read accelerometer
    int16_t x, y, z;
    g_adxl->getXYZ(x, y, z);
    g_current_x = rawToGs(x);
    g_current_y = rawToGs(y);
    g_current_z = rawToGs(z);

    // Add to sample history
    addSample(g_current_x, g_current_y, g_current_z);

    // Check for inverted hold gesture (highest priority - triggers calibration)
    // Y-axis points up (Y=-1.0g when vertical/upright)
    // When inverted: Y rises toward +1.0g
    if (g_current_y > -g_config.inverted_z_threshold) {  // Y > +0.7g indicates inverted
        uint32_t now = millis();

        // FIX Bug #3: Only start new detection if cooldown expired
        if (!g_inverted_active && now >= g_inverted_cooldown_end) {
            g_inverted_active = true;
            g_inverted_triggered = false;
            g_inverted_start_time = now;
            Serial.print("Gestures: Inverted detected! Y=");
            Serial.print(g_current_y);
            Serial.println("g - hold for 5 seconds...");
        } else if (!g_inverted_triggered) {
            // Check if held long enough
            uint32_t held_duration = now - g_inverted_start_time;
            if (held_duration >= g_config.inverted_hold_duration) {
                Serial.println("Gestures: INVERTED_HOLD gesture triggered!");
                g_inverted_triggered = true;  // Latch to prevent re-triggering
                g_inverted_cooldown_end = now + 2000;  // FIX Bug #3: 2 second cooldown
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
    // Y-axis points up, so sideways tilt is when X or Z is large
    // When vertical: Y=-1.0g, X≈0, Z≈0
    // When sideways: Y≈0, X or Z ≈±1.0g
    if (fabs(g_current_x) > g_config.sideways_threshold ||
        fabs(g_current_z) > g_config.sideways_threshold) {
        return GESTURE_SIDEWAYS_TILT;
    }

    // Check for upright (bottle placement on table)
    // Y-axis points up (Y=-1.0g when vertical)
    // Requirements for UPRIGHT:
    // 1. Y-axis close to -1g (within ~25° of vertical: cos(25°) ≈ 0.906)
    // 2. X and Z axes close to 0 (bottle not tilted sideways)
    // 3. Weight is positive (bottle is on table, not in the air)
    // Additional requirement for UPRIGHT_STABLE:
    // 4. Low variance (bottle not moving - accelerometer stable)
    // 5. Weight stable for 2+ seconds
    const float UPRIGHT_Y_MAX = -0.90f;  // Y should be < -0.90g (more negative) when vertical
    const float TILT_XZ_MAX = 0.174f;    // sin(10°) for minimal sideways tilt

    bool y_ok = g_current_y <= UPRIGHT_Y_MAX;  // Y is negative when vertical
    bool x_ok = fabs(g_current_x) <= TILT_XZ_MAX;
    bool z_ok = fabs(g_current_z) <= TILT_XZ_MAX;
    bool weight_ok = weight_ml >= -50.0f;
    bool stable = gesturesIsStable();
    float variance = gesturesGetVariance();

    if (g_debug_enabled && g_debug_calibration) {
        // Debug: show why UPRIGHT_STABLE is or isn't detected
        static unsigned long last_debug = 0;
        if (millis() - last_debug >= 1000) {
            last_debug = millis();
            if (z_ok && x_ok && y_ok && weight_ok) {
                if (!stable) {
                    Serial.print("Gestures: UPRIGHT (not stable) - variance=");
                    Serial.print(variance, 4);
                    Serial.print(" (need <");
                    Serial.print(g_config.stability_variance, 4);
                    Serial.println(" for STABLE)");
                } else {
                    Serial.print("Gestures: UPRIGHT and stable - variance=");
                    Serial.print(variance, 4);
                    Serial.println();
                }
            } else {
                Serial.print("Gestures: Conditions check - Y:");
                Serial.print(y_ok ? "✓" : "✗");
                Serial.print(" X:");
                Serial.print(x_ok ? "✓" : "✗");
                Serial.print(" Z:");
                Serial.print(z_ok ? "✓" : "✗");
                Serial.print(" Weight:");
                Serial.println(weight_ok ? "✓" : "✗");
            }
        }
    }

    // Check if bottle is upright (orientation check only - stability not required)
    if (y_ok && x_ok && z_ok && weight_ok) {
        // For UPRIGHT_STABLE, we need both accelerometer stability AND weight stability
        // For just UPRIGHT, orientation is enough (allows detection during table bangs, vibrations)

        if (stable) {
            // Accelerometer is stable - track weight stability for UPRIGHT_STABLE detection
            if (!g_upright_active) {
                g_upright_active = true;
                g_upright_start_time = millis();
                g_last_stable_weight = weight_ml;
                if (g_debug_enabled && g_debug_calibration) {
                    Serial.println("Gestures: UPRIGHT stable detected - tracking weight stability");
                }
            }

            // Check if weight is stable (not fluctuating)
            // Allow ~6ml variance (weight_ml is in ml, so direct comparison)
            float weight_delta = fabs(weight_ml - g_last_stable_weight);
            bool weight_stable = (weight_delta < 6.0f);

            // If weight is stable, update the baseline (tracks gradual changes)
            if (weight_stable) {
                g_last_stable_weight = weight_ml;
            } else {
                // Weight changed significantly - reset timer
                g_upright_start_time = millis();
                g_last_stable_weight = weight_ml;
            }

            // Check if held stable long enough
            uint32_t upright_duration = millis() - g_upright_start_time;

            if (g_debug_enabled && g_debug_calibration) {
                static unsigned long last_weight_debug = 0;
                if (millis() - last_weight_debug >= 1000) {
                    last_weight_debug = millis();
                    if (!weight_stable) {
                        Serial.print("Gestures: UPRIGHT stable but weight NOT stable - delta=");
                        Serial.print(weight_delta, 1);
                        Serial.println("ml (need <6ml)");
                    } else if (upright_duration < 2000) {
                        Serial.print("Gestures: UPRIGHT stable and weight stable - duration=");
                        Serial.print(upright_duration);
                        Serial.println("ms (need 2000ms)");
                    }
                }
            }

            // Return UPRIGHT_STABLE if both accelerometer stable AND weight stable for 2+ seconds
            if (weight_stable && upright_duration >= 2000) {
                return GESTURE_UPRIGHT_STABLE;
            }
        } else {
            // Not accelerometer stable (e.g., table bang, vibration) - reset tracking
            if (g_upright_active) {
                if (g_debug_enabled && g_debug_calibration) {
                    Serial.println("Gestures: Accelerometer unstable - resetting UPRIGHT_STABLE tracking");
                }
            }
            g_upright_active = false;
            g_upright_start_time = 0;
            g_last_stable_weight = 0.0f;
        }

        // Return UPRIGHT (orientation correct, regardless of stability)
        return GESTURE_UPRIGHT;
    } else {
        // Reset upright tracking if conditions no longer met
        if (g_upright_active) {
            if (g_debug_enabled && g_debug_calibration) {
                Serial.println("Gestures: UPRIGHT ended");
            }
        }
        g_upright_active = false;
        g_upright_start_time = 0;
        g_last_stable_weight = 0.0f;

        // Debug: why returning NONE instead of UPRIGHT
        if (g_debug_enabled && g_debug_calibration) {
            Serial.print("Gestures: Returning NONE - weight_ml=");
            Serial.print(weight_ml, 1);
            Serial.print(" Y=");
            Serial.print(g_current_y, 3);
            Serial.print(" X=");
            Serial.print(g_current_x, 3);
            Serial.print(" Z=");
            Serial.println(g_current_z, 3);
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
    g_upright_active = false;
    g_upright_start_time = 0;
    g_last_stable_weight = 0.0f;
}
