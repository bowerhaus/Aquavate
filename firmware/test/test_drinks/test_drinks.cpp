// test_drinks.cpp — unit tests for timezone reset logic and drink detection
//
// Covers the two highest-risk areas:
//   getSecondsUntilRollover() — timezone boundary math (Issue #120 regression)
//   drinksUpdate()            — drink / refill / drift classification
//   recalculateDailyTotals()  — reset boundary filtering via drinksInit()
//
// Uses setTestTimeProvider() to inject a fixed UTC timestamp so tests are
// deterministic regardless of when they run.

#include <unity.h>
#include <time.h>
#include "drinks.h"
#include "calibration.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------------
// Known base timestamp: 2024-01-15 00:00:00 UTC (Monday midnight)
// Verified: date -u -r 1705276800 → Mon Jan 15 00:00:00 UTC 2024
// ---------------------------------------------------------------------------
static const uint32_t T_2024_01_15_MIDNIGHT_UTC = 1705276800u;

static uint32_t g_fake_utc = 0;
static uint32_t fake_time() { return g_fake_utc; }

// Calibration: 500 ADC counts per gram, empty bottle at ADC 0
// → ADC 250000 = 500 ml, ADC 150000 = 300 ml
static CalibrationData make_test_cal() {
    CalibrationData cal;
    cal.scale_factor        = 500.0f;
    cal.empty_bottle_adc    = 0;
    cal.full_bottle_adc     = 415000;
    cal.calibration_valid   = 1;
    cal.calibration_timestamp = 0;
    return cal;
}

void setUp(void) {
    // Make mktime() behave as timegm() — matches ESP32 (UTC-only) behaviour
    setenv("TZ", "UTC", 1);
    tzset();

    g_timezone_offset = 0;
    g_time_valid      = true;
    g_fake_utc        = T_2024_01_15_MIDNIGHT_UTC + 3600u;  // 01:00 UTC default

    setTestTimeProvider(fake_time);
    testClearStorage();
    testResetDrinkState();
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// getSecondsUntilRollover — timezone boundary tests
// These would have caught Issue #120 (UTC midnight fired instead of local midnight)
// ---------------------------------------------------------------------------

void test_rollover_utc_noon_is_12h_away() {
    // UTC+0, 12:00 → next midnight is exactly 12 h away
    g_fake_utc        = T_2024_01_15_MIDNIGHT_UTC + 12u * 3600u;
    g_timezone_offset = 0;
    uint32_t secs     = getSecondsUntilRollover();
    TEST_ASSERT_UINT32_WITHIN(1, 12u * 3600u, secs);
}

void test_rollover_returns_zero_when_time_invalid() {
    g_time_valid = false;
    TEST_ASSERT_EQUAL(0u, getSecondsUntilRollover());
}

void test_rollover_bst_11pm_local_is_1h_away() {
    // UTC+1 (BST): 22:00 UTC = 23:00 local → next local midnight is 1h away
    g_fake_utc        = T_2024_01_15_MIDNIGHT_UTC + 22u * 3600u;
    g_timezone_offset = 1;
    uint32_t secs     = getSecondsUntilRollover();
    TEST_ASSERT_UINT32_WITHIN(1, 3600u, secs);
}

void test_rollover_bst_midnight_utc_is_23h_away() {
    // UTC+1: 00:00 UTC = 01:00 local → local midnight was 1h ago
    // → next local midnight is 23h from now
    g_fake_utc        = T_2024_01_15_MIDNIGHT_UTC;  // exactly midnight UTC
    g_timezone_offset = 1;
    uint32_t secs     = getSecondsUntilRollover();
    TEST_ASSERT_UINT32_WITHIN(1, 23u * 3600u, secs);
}

void test_rollover_est_noon_utc_is_12h_away() {
    // UTC-5 (EST): 12:00 UTC = 07:00 local → next midnight local = 17h from now (05:00 UTC next day)
    g_fake_utc        = T_2024_01_15_MIDNIGHT_UTC + 12u * 3600u;
    g_timezone_offset = -5;
    uint32_t secs     = getSecondsUntilRollover();
    TEST_ASSERT_UINT32_WITHIN(1, 17u * 3600u, secs);
}

void test_rollover_one_minute_before_midnight() {
    // UTC+0, 23:59 → 60 seconds until midnight
    g_fake_utc        = T_2024_01_15_MIDNIGHT_UTC + 24u * 3600u - 60u;
    g_timezone_offset = 0;
    uint32_t secs     = getSecondsUntilRollover();
    TEST_ASSERT_UINT32_WITHIN(1, 60u, secs);
}

// ---------------------------------------------------------------------------
// drinksUpdate — drink / refill / drift classification
// ---------------------------------------------------------------------------

void test_drink_first_call_establishes_baseline() {
    CalibrationData cal = make_test_cal();
    drinksInit();  // sets g_drinks_initialized = true
    // First call → baseline established, no drink recorded
    bool result = drinksUpdate(250000, cal);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(0, drinksGetDailyTotal());
}

void test_drink_second_call_detects_drink() {
    CalibrationData cal = make_test_cal();
    drinksInit();
    drinksUpdate(250000, cal);          // establish baseline at 500 ml

    bool result = drinksUpdate(150000, cal);  // 300 ml now → delta 200 ml
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(200, drinksGetDailyTotal());
}

void test_refill_not_recorded_in_daily_total() {
    CalibrationData cal = make_test_cal();
    drinksInit();
    drinksUpdate(150000, cal);          // baseline at 300 ml
    bool result = drinksUpdate(400000, cal);  // 800 ml → +500 ml refill
    TEST_ASSERT_FALSE(result);          // refill returns false
    TEST_ASSERT_EQUAL(0, drinksGetDailyTotal());
}

void test_small_delta_below_threshold_not_recorded() {
    CalibrationData cal = make_test_cal();
    drinksInit();
    drinksUpdate(250000, cal);          // baseline at 500 ml
    // 10 ml decrease — below DRINK_MIN_THRESHOLD_ML (30 ml), not a drink
    bool result = drinksUpdate(245000, cal);  // 490 ml
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(0, drinksGetDailyTotal());
}

// ---------------------------------------------------------------------------
// recalculateDailyTotals — reset boundary filtering
// ---------------------------------------------------------------------------

void test_daily_total_only_counts_todays_drinks() {
    // Seed in-memory storage: one record from yesterday, one from today
    uint32_t yesterday = T_2024_01_15_MIDNIGHT_UTC - 3600u;  // 23:00 UTC Jan 14
    uint32_t today     = T_2024_01_15_MIDNIGHT_UTC + 1800u;  // 00:30 UTC Jan 15

    testAddDrinkRecord(yesterday, 150, 0);   // should NOT count
    testAddDrinkRecord(today,     200, 0);   // should count

    g_fake_utc        = T_2024_01_15_MIDNIGHT_UTC + 3600u;  // 01:00 UTC
    g_timezone_offset = 0;
    drinksInit();  // recalculateDailyTotals() runs here

    TEST_ASSERT_EQUAL(200, drinksGetDailyTotal());
    TEST_ASSERT_EQUAL(1,   drinksGetDrinkCount());
}

void test_daily_total_excludes_deleted_records() {
    uint32_t today = T_2024_01_15_MIDNIGHT_UTC + 3600u;
    testAddDrinkRecord(today, 150, 0x04);   // flags=0x04 → deleted

    g_fake_utc        = today + 1800u;
    g_timezone_offset = 0;
    drinksInit();

    TEST_ASSERT_EQUAL(0, drinksGetDailyTotal());
    TEST_ASSERT_EQUAL(0, drinksGetDrinkCount());
}

void test_daily_total_accumulates_multiple_drinks() {
    uint32_t base = T_2024_01_15_MIDNIGHT_UTC + 3600u;
    testAddDrinkRecord(base,          150, 0);
    testAddDrinkRecord(base + 1800u,  200, 0);
    testAddDrinkRecord(base + 3600u,  100, 0);

    g_fake_utc        = base + 7200u;
    g_timezone_offset = 0;
    drinksInit();

    TEST_ASSERT_EQUAL(450, drinksGetDailyTotal());
    TEST_ASSERT_EQUAL(3,   drinksGetDrinkCount());
}

void test_daily_total_bst_counts_correct_day() {
    // UTC+1 (BST): reset at 23:00 UTC (= local midnight)
    // Record at 22:00 UTC (= 23:00 BST yesterday) should NOT count
    // Record at 23:30 UTC (= 00:30 BST today) should count
    uint32_t before_bst_midnight = T_2024_01_15_MIDNIGHT_UTC - 2u * 3600u; // 22:00 UTC Jan 14
    uint32_t after_bst_midnight  = T_2024_01_15_MIDNIGHT_UTC - 30u * 60u;  // 23:30 UTC Jan 14

    testAddDrinkRecord(before_bst_midnight, 120, 0);
    testAddDrinkRecord(after_bst_midnight,  180, 0);

    // Current time: 01:00 UTC Jan 15 = 02:00 BST Jan 15
    g_fake_utc        = T_2024_01_15_MIDNIGHT_UTC + 3600u;
    g_timezone_offset = 1;
    drinksInit();

    // Local midnight BST = 23:00 UTC Jan 14 → 23:30 UTC is after it
    TEST_ASSERT_EQUAL(180, drinksGetDailyTotal());
    TEST_ASSERT_EQUAL(1,   drinksGetDrinkCount());
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    // Rollover / timezone boundary tests
    RUN_TEST(test_rollover_utc_noon_is_12h_away);
    RUN_TEST(test_rollover_returns_zero_when_time_invalid);
    RUN_TEST(test_rollover_bst_11pm_local_is_1h_away);
    RUN_TEST(test_rollover_bst_midnight_utc_is_23h_away);
    RUN_TEST(test_rollover_est_noon_utc_is_12h_away);
    RUN_TEST(test_rollover_one_minute_before_midnight);

    // Drink detection
    RUN_TEST(test_drink_first_call_establishes_baseline);
    RUN_TEST(test_drink_second_call_detects_drink);
    RUN_TEST(test_refill_not_recorded_in_daily_total);
    RUN_TEST(test_small_delta_below_threshold_not_recorded);

    // Daily total / reset boundary
    RUN_TEST(test_daily_total_only_counts_todays_drinks);
    RUN_TEST(test_daily_total_excludes_deleted_records);
    RUN_TEST(test_daily_total_accumulates_multiple_drinks);
    RUN_TEST(test_daily_total_bst_counts_correct_day);

    return UNITY_END();
}
