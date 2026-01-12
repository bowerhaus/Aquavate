/**
 * Aquavate - Weight Measurement Module
 * Stable weight readings with outlier removal and variance checking
 */

#ifndef WEIGHT_H
#define WEIGHT_H

#include <Arduino.h>
#include <Adafruit_NAU7802.h>

// Weight measurement result
struct WeightMeasurement {
    int32_t raw_adc;       // Raw ADC reading (mean after outlier removal)
    float variance;        // Sample variance
    bool stable;           // Stability flag (variance < threshold)
    int sample_count;      // Number of samples used (after outlier removal)
    bool valid;            // Measurement valid (enough samples, no errors)
};

// Weight measurement configuration
struct WeightConfig {
    int duration_seconds;         // Measurement duration (default: 10s)
    float variance_threshold;     // Stable if variance < this (default: 100.0)
    int min_samples;              // Minimum samples required (default: 8)
    float outlier_std_devs;       // Outlier threshold in std devs (default: 2.0)
};

// Initialize weight measurement module
void weightInit(Adafruit_NAU7802& nau);

// Take a stable weight reading with default config (10s duration)
WeightMeasurement weightMeasureStable();

// Take a stable weight reading with custom config
WeightMeasurement weightMeasureStable(const WeightConfig& config);

// Take a single ADC reading (non-blocking, returns immediately)
int32_t weightReadRaw();

// Check if NAU7802 is ready for reading
bool weightIsReady();

// Get default weight measurement config
WeightConfig weightGetDefaultConfig();

#endif // WEIGHT_H
