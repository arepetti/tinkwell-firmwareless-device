/*
 * test_applet_lifecycle.c -- Unit tests for applet push/load.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tw_applet.h"
#include "tw_types.h"
#include "mock_pal.h"

void setUp(void)
{
    mock_reset();
    mock_config_reset();
}

void tearDown(void) {}

void test_initial_state_is_none(void)
{
    TEST_ASSERT_EQUAL(TW_APPLET_NONE, tw_applet_state());
}

void test_initial_version_is_null(void)
{
    TEST_ASSERT_NULL(tw_applet_version());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_is_none);
    RUN_TEST(test_initial_version_is_null);
    return UNITY_END();
}
