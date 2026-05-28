/*
 * test_wasm_host_api.c -- Unit tests for the WASM host API bindings.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "wasm_host_api.h"
#include "tw_types.h"
#include "mock_pal.h"

void setUp(void)
{
    mock_reset();
    mock_config_reset();
}

void tearDown(void) {}

void test_read_sensor_returns_fake_temp(void)
{
    g_mock_cfg.i2c_read_temp = 235;
    int32_t val = tw_host_read_sensor_sn("temperature", 11);
    TEST_ASSERT_EQUAL(235, val);
}

void test_write_gpio_records_call(void)
{
    tw_host_write_gpio(4, 1);
    TEST_ASSERT_TRUE(mock_count("pal_gpio_write") > 0);
}

void test_read_gpio_returns_mock_value(void)
{
    g_mock_cfg.gpio_read_value = 1;
    int32_t val = tw_host_read_gpio(3);
    TEST_ASSERT_EQUAL(1, val);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_read_sensor_returns_fake_temp);
    RUN_TEST(test_write_gpio_records_call);
    RUN_TEST(test_read_gpio_returns_mock_value);
    return UNITY_END();
}
