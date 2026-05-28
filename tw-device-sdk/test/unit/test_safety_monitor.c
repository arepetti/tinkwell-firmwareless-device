/*
 * test_safety_monitor.c -- Unit tests for the safety threshold monitor.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tw_safety.h"
#include "tw_types.h"
#include "mock_pal.h"

static int s_low_count;
static int s_high_count;

static void on_low(tw_safety_event_t ev, void *ctx)
{
    (void)ev; (void)ctx;
    s_low_count++;
}

static void on_high(tw_safety_event_t ev, void *ctx)
{
    (void)ev; (void)ctx;
    s_high_count++;
}

static tw_safety_monitor_t mon;
static tw_safety_config_t  cfg;

void setUp(void)
{
    mock_reset();
    s_low_count  = 0;
    s_high_count = 0;

    cfg = (tw_safety_config_t){
        .low_threshold  = 30,
        .high_threshold = 400,
        .on_low         = on_low,
        .on_high        = on_high,
    };
    tw_safety_create(&mon, &cfg);
}

void tearDown(void) {}

void test_normal_range_no_callback(void)
{
    tw_safety_check(&mon, 200);
    TEST_ASSERT_EQUAL(0, s_low_count);
    TEST_ASSERT_EQUAL(0, s_high_count);
}

void test_below_freeze_triggers_low(void)
{
    tw_safety_check(&mon, 25);
    TEST_ASSERT_EQUAL(1, s_low_count);
    TEST_ASSERT_EQUAL(0, s_high_count);
}

void test_above_max_triggers_high(void)
{
    tw_safety_check(&mon, 410);
    TEST_ASSERT_EQUAL(0, s_low_count);
    TEST_ASSERT_EQUAL(1, s_high_count);
}

void test_exact_threshold_no_trigger(void)
{
    tw_safety_check(&mon, 30);
    TEST_ASSERT_EQUAL(0, s_low_count);
    tw_safety_check(&mon, 400);
    TEST_ASSERT_EQUAL(0, s_high_count);
}

void test_safety_override_flag(void)
{
    TEST_ASSERT_FALSE(tw_safety_is_overriding(&mon));
    tw_safety_check(&mon, 20);
    TEST_ASSERT_TRUE(tw_safety_is_overriding(&mon));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_normal_range_no_callback);
    RUN_TEST(test_below_freeze_triggers_low);
    RUN_TEST(test_above_max_triggers_high);
    RUN_TEST(test_exact_threshold_no_trigger);
    RUN_TEST(test_safety_override_flag);
    return UNITY_END();
}
