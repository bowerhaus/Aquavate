// test_weight.cpp — unit tests for outlier removal in weight.cpp
//
// Tests removeOutliers() (exposed via removeOutliersTest() under UNIT_TEST)
// which strips samples more than N standard deviations from the mean.
//
// No hardware, no NVS required.

#include <unity.h>
#include "weight.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// removeOutliers
// ---------------------------------------------------------------------------

void test_outlier_removes_single_spike() {
    int32_t samples[] = {100, 101, 99, 100, 500, 101, 100, 99, 100, 101};
    int32_t new_mean;
    int count = removeOutliersTest(samples, 10, 2.0f, new_mean);

    // The 500 spike is > 2 std-devs from the rest — must be removed
    TEST_ASSERT_LESS_THAN(10, count);
    // Mean of the clean data should be close to 100
    TEST_ASSERT_INT_WITHIN(2, 100, new_mean);
}

void test_outlier_removes_negative_spike() {
    int32_t samples[] = {100, 101, 99, 100, -400, 101, 100, 99, 100, 101};
    int32_t new_mean;
    int count = removeOutliersTest(samples, 10, 2.0f, new_mean);

    TEST_ASSERT_LESS_THAN(10, count);
    TEST_ASSERT_INT_WITHIN(2, 100, new_mean);
}

void test_outlier_keeps_uniform_data() {
    // Symmetric around 1000 so integer mean == 1000 exactly.
    // Max diff = 1, std_dev ≈ 0.63, threshold ≈ 1.26 → all samples kept.
    int32_t samples[] = {1000, 1000, 1001, 999, 1000, 1001, 999, 1000, 1000, 1000};
    int32_t new_mean;
    int count = removeOutliersTest(samples, 10, 2.0f, new_mean);

    TEST_ASSERT_EQUAL(10, count);
    TEST_ASSERT_INT_WITHIN(1, 1000, new_mean);
}

void test_outlier_skips_removal_for_two_samples() {
    // Need at least 3 samples to apply outlier removal
    int32_t samples[] = {100, 500};
    int32_t new_mean;
    int count = removeOutliersTest(samples, 2, 2.0f, new_mean);

    TEST_ASSERT_EQUAL(2, count);
}

void test_outlier_skips_removal_for_one_sample() {
    int32_t samples[] = {999};
    int32_t new_mean;
    int count = removeOutliersTest(samples, 1, 2.0f, new_mean);

    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(999, new_mean);
}

void test_outlier_tighter_threshold_removes_more() {
    // With a very tight threshold (0.5 std-devs) even moderate variation is removed
    int32_t samples[] = {100, 105, 100, 95, 100, 104, 100, 96, 100, 103};
    int32_t new_mean_tight, new_mean_loose;
    int32_t s_tight[10], s_loose[10];
    for (int i = 0; i < 10; i++) { s_tight[i] = s_loose[i] = samples[i]; }

    int count_tight = removeOutliersTest(s_tight, 10, 0.5f, new_mean_tight);
    int count_loose = removeOutliersTest(s_loose, 10, 3.0f, new_mean_loose);

    // Tighter threshold keeps fewer samples
    TEST_ASSERT_TRUE(count_tight < count_loose);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_outlier_removes_single_spike);
    RUN_TEST(test_outlier_removes_negative_spike);
    RUN_TEST(test_outlier_keeps_uniform_data);
    RUN_TEST(test_outlier_skips_removal_for_two_samples);
    RUN_TEST(test_outlier_skips_removal_for_one_sample);
    RUN_TEST(test_outlier_tighter_threshold_removes_more);

    return UNITY_END();
}
