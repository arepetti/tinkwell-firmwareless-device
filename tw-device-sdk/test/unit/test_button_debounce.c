/*
 * test_button_debounce.c -- Unit tests for button debounce utility.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tw_button.h"
#include "tw_types.h"
#include "mock_pal.h"

static int s_press_count;

static void on_press(void *ctx)
{
    (void)ctx;
    s_press_count++;
}

static tw_button_t btn;

void setUp(void)
{
    mock_reset();
    mock_config_reset();
    s_press_count = 0;
}

void tearDown(void) {}

void test_button_create(void)
{
    tw_err_t err = tw_button_create(&btn, 5, on_press, NULL);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_TRUE(mock_count("pal_gpio_init") > 0);
}

void test_debounce_ignores_noise(void)
{
    tw_button_create(&btn, 5, on_press, NULL);

    /* Simulate rapid bouncing -- multiple polls within debounce window. */
    g_mock_cfg.gpio_read_value = 1;
    tw_button_poll(&btn);
    g_mock_cfg.gpio_read_value = 0;
    tw_button_poll(&btn);
    g_mock_cfg.gpio_read_value = 1;
    tw_button_poll(&btn);

    /* Only one press should register at most. */
    TEST_ASSERT_LESS_OR_EQUAL(1, s_press_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_button_create);
    RUN_TEST(test_debounce_ignores_noise);
    return UNITY_END();
}
