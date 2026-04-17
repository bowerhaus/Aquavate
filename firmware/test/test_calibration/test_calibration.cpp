// test_calibration.cpp — unit tests for scale-factor math in calibration.cpp
//
// Tests the two pure functions that are always compiled (outside ENABLE_STANDALONE_CALIBRATION):
//   calibrationCalculateScaleFactor()  — two-point linear calibration
//   calibrationGetWaterWeight()        — ADC → ml conversion
//
// No hardware, no NVS, no BLE required.

#include <unity.h>
#include "calibration.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// calibrationCalculateScaleFactor
// ---------------------------------------------------------------------------

void test_scale_factor_zero_to_full() {
    // 0..830000 ADC for 830 ml → exactly 1000.0 ADC/g
    float sf = calibrationCalculateScaleFactor(0, 830000, 830.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, sf);
}

void test_scale_factor_typical_values() {
    // Typical NAU7802 range: empty≈1000000, full≈1415000 for 830 ml → ≈500 ADC/g
    float sf = calibrationCalculateScaleFactor(1000000, 1415000, 830.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 500.0f, sf);
}

void test_scale_factor_returns_zero_when_full_le_empty() {
    // full_adc ≤ empty_adc is physically impossible — must return 0 (error sentinel)
    float sf = calibrationCalculateScaleFactor(1000000, 500000, 830.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sf);
}

void test_scale_factor_returns_zero_when_equal() {
    float sf = calibrationCalculateScaleFactor(500000, 500000, 830.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sf);
}

// ---------------------------------------------------------------------------
// calibrationGetWaterWeight
// ---------------------------------------------------------------------------

static CalibrationData make_cal(float scale_factor, int32_t empty_adc) {
    CalibrationData cal;
    cal.scale_factor       = scale_factor;
    cal.empty_bottle_adc   = empty_adc;
    cal.full_bottle_adc    = empty_adc + (int32_t)(scale_factor * 830.0f);
    cal.calibration_valid  = 1;
    cal.calibration_timestamp = 0;
    return cal;
}

void test_water_weight_at_empty_adc_is_zero() {
    CalibrationData cal = make_cal(500.0f, 1000000);
    float ml = calibrationGetWaterWeight(1000000, cal);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ml);
}

void test_water_weight_at_full_adc_is_830ml() {
    CalibrationData cal = make_cal(500.0f, 1000000);
    // full_adc = 1000000 + 500 * 830 = 1415000
    float ml = calibrationGetWaterWeight(1415000, cal);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 830.0f, ml);
}

void test_water_weight_midpoint() {
    CalibrationData cal = make_cal(500.0f, 1000000);
    // halfway: ADC = 1207500 → 415ml
    float ml = calibrationGetWaterWeight(1207500, cal);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 415.0f, ml);
}

void test_water_weight_returns_zero_for_invalid_calibration() {
    CalibrationData cal = make_cal(500.0f, 1000000);
    cal.calibration_valid = 0;
    float ml = calibrationGetWaterWeight(1200000, cal);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ml);
}

void test_water_weight_returns_zero_for_zero_scale_factor() {
    CalibrationData cal = make_cal(0.0f, 1000000);
    cal.calibration_valid = 1;
    float ml = calibrationGetWaterWeight(1200000, cal);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ml);
}

void test_water_weight_negative_when_below_empty() {
    // ADC below empty bottle is physically possible (tare error) — should return negative
    CalibrationData cal = make_cal(500.0f, 1000000);
    float ml = calibrationGetWaterWeight(900000, cal);
    TEST_ASSERT_TRUE(ml < 0.0f);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_scale_factor_zero_to_full);
    RUN_TEST(test_scale_factor_typical_values);
    RUN_TEST(test_scale_factor_returns_zero_when_full_le_empty);
    RUN_TEST(test_scale_factor_returns_zero_when_equal);

    RUN_TEST(test_water_weight_at_empty_adc_is_zero);
    RUN_TEST(test_water_weight_at_full_adc_is_830ml);
    RUN_TEST(test_water_weight_midpoint);
    RUN_TEST(test_water_weight_returns_zero_for_invalid_calibration);
    RUN_TEST(test_water_weight_returns_zero_for_zero_scale_factor);
    RUN_TEST(test_water_weight_negative_when_below_empty);

    return UNITY_END();
}
