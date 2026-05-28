/*
 * test_ota_state.c -- Unit tests for OTA state machine.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tw_ota.h"
#include "tw_types.h"
#include "mock_pal.h"

void setUp(void)
{
    mock_reset();
    mock_config_reset();
}

void tearDown(void) {}

void test_initial_state_is_idle(void)
{
    TEST_ASSERT_EQUAL(TW_OTA_IDLE, tw_ota_state());
}

void test_progress_zero_when_idle(void)
{
    TEST_ASSERT_EQUAL(0, tw_ota_progress_pct());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_is_idle);
    RUN_TEST(test_progress_zero_when_idle);
    return UNITY_END();
}
