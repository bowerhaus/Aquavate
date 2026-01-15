/**
 * Aquavate - Weight Measurement Module
 * Implementation
 */

#include "weight.h"
#include "config.h"
#include <math.h>

// Static variables
static Adafruit_NAU7802* g_nau = nullptr;
static bool g_initialized = false;

void weightInit(Adafruit_NAU7802& nau) {
    g_nau = &nau;
    g_initialized = true;
}

WeightConfig weightGetDefaultConfig() {
    WeightConfig config;
    config.duration_seconds = WEIGHT_MEASUREMENT_DURATION;
    config.variance_threshold = WEIGHT_VARIANCE_THRESHOLD;
    config.min_samples = WEIGHT_MIN_SAMPLES;
    config.outlier_std_devs = WEIGHT_OUTLIER_STD_DEVS;
    return config;
}

int32_t weightReadRaw() {
    if (!g_initialized || !g_nau || !g_nau->available()) {
        return 0;
    }
    return g_nau->read();
}

bool weightIsReady() {
    return g_initialized && g_nau && g_nau->available();
}

// Calculate mean of samples
static int32_t calculateMean(const int32_t* samples, int count) {
    if (count == 0) return 0;

    int64_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += samples[i];
    }
    return (int32_t)(sum / count);
}

// Calculate standard deviation
static float calculateStdDev(const int32_t* samples, int count, int32_t mean) {
    if (count < 2) return 0.0f;

    float sum_sq_diff = 0.0f;
    for (int i = 0; i < count; i++) {
        float diff = (float)(samples[i] - mean);
        sum_sq_diff += diff * diff;
    }

    return sqrt(sum_sq_diff / count);
}

// Calculate variance
static float calculateVariance(const int32_t* samples, int count, int32_t mean) {
    if (count < 2) return 0.0f;

    float sum_sq_diff = 0.0f;
    for (int i = 0; i < count; i++) {
        float diff = (float)(samples[i] - mean);
        sum_sq_diff += diff * diff;
    }

    return sum_sq_diff / count;
}

// Remove outliers from samples (±std_devs standard deviations from mean)
// Returns new count after outlier removal
static int removeOutliers(int32_t* samples, int count, float std_devs, int32_t& new_mean) {
    if (count < 3) {
        new_mean = calculateMean(samples, count);
        return count; // Not enough samples to remove outliers
    }

    // Calculate initial mean and std dev
    int32_t mean = calculateMean(samples, count);
    float std_dev = calculateStdDev(samples, count, mean);

    // Create filtered array
    int filtered_count = 0;
    int32_t* filtered = new int32_t[count];

    float threshold = std_dev * std_devs;

    for (int i = 0; i < count; i++) {
        float diff = fabs((float)(samples[i] - mean));
        if (diff <= threshold) {
            filtered[filtered_count++] = samples[i];
        }
    }

    // Copy filtered samples back
    for (int i = 0; i < filtered_count; i++) {
        samples[i] = filtered[i];
    }

    // Calculate new mean
    new_mean = calculateMean(samples, filtered_count);

    delete[] filtered;
    return filtered_count;
}

WeightMeasurement weightMeasureStable() {
    return weightMeasureStable(weightGetDefaultConfig());
}

WeightMeasurement weightMeasureStable(const WeightConfig& config) {
    WeightMeasurement result;
    result.valid = false;
    result.stable = false;
    result.raw_adc = 0;
    result.variance = 0.0f;
    result.sample_count = 0;

    if (!g_initialized || !g_nau) {
        Serial.println("Weight: Not initialized");
        return result;
    }

    // Calculate expected sample count (10 SPS * duration)
    int expected_samples = config.duration_seconds * 10;
    int32_t* samples = new int32_t[expected_samples];
    int sample_count = 0;

    Serial.print("Weight: Starting measurement (");
    Serial.print(config.duration_seconds);
    Serial.println("s)...");

    unsigned long start_time = millis();
    unsigned long duration_ms = config.duration_seconds * 1000;

    // Collect samples for specified duration
    while (millis() - start_time < duration_ms) {
        if (g_nau->available()) {
            int32_t reading = g_nau->read();
            if (sample_count < expected_samples) {
                samples[sample_count++] = reading;
            }
        }
        delay(10); // Wait for next sample (~10 SPS)
    }

    Serial.print("Weight: Collected ");
    Serial.print(sample_count);
    Serial.println(" samples");

    if (sample_count < config.min_samples) {
        Serial.println("Weight: Not enough samples");
        delete[] samples;
        return result;
    }

    // Remove outliers
    int32_t mean_after_outliers;
    int filtered_count = removeOutliers(samples, sample_count, config.outlier_std_devs, mean_after_outliers);

    Serial.print("Weight: After outlier removal: ");
    Serial.print(filtered_count);
    Serial.println(" samples");

    if (filtered_count < config.min_samples) {
        Serial.println("Weight: Not enough samples after outlier removal");
        delete[] samples;
        return result;
    }

    // Calculate final statistics
    result.raw_adc = mean_after_outliers;
    result.variance = calculateVariance(samples, filtered_count, mean_after_outliers);
    result.sample_count = filtered_count;
    result.valid = true;
    result.stable = (result.variance < config.variance_threshold);

    Serial.print("Weight: Mean ADC = ");
    Serial.print(result.raw_adc);
    Serial.print(", Variance = ");
    Serial.print(result.variance);
    Serial.print(", Stable = ");
    Serial.println(result.stable ? "YES" : "NO");

    delete[] samples;
    return result;
}
