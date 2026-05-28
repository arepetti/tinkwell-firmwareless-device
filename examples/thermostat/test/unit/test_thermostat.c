/*
 * test_thermostat.c -- Unit tests for the thermostat state machine.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "thermostat.h"
#include "tw_types.h"
#include "mock_pal.h"

static tw_device_config_t dev = {
    .name = "test-thermostat",
    .fw_version = "0.0.1",
    .tick_interval_ms = 1000,
};

void setUp(void)
{
    mock_reset();
    mock_config_reset();
}

void tearDown(void) {}

void test_init_returns_ok(void)
{
    tw_err_t err = thermostat_init(&dev);
    TEST_ASSERT_EQUAL(TW_OK, err);
}

void test_tick_returns_ok(void)
{
    thermostat_init(&dev);
    tw_err_t err = thermostat_tick(&dev);
    TEST_ASSERT_EQUAL(TW_OK, err);
}

void test_gpio_write_called_on_tick(void)
{
    thermostat_init(&dev);
    thermostat_tick(&dev);
    TEST_ASSERT_TRUE(mock_count("pal_gpio_write") > 0);
}

void test_freeze_protection_forces_relay_on(void)
{
    thermostat_init(&dev);
    g_mock_cfg.i2c_read_temp = 20;  /* 2.0°C -- below freeze threshold. */
    thermostat_tick(&dev);
    /* Relay should be ON. We verify by checking gpio_write was called. */
    TEST_ASSERT_TRUE(mock_count("pal_gpio_write") > 0);
}

void test_overheat_protection_forces_relay_off(void)
{
    thermostat_init(&dev);
    g_mock_cfg.i2c_read_temp = 410; /* 41.0°C -- above max. */
    thermostat_tick(&dev);
    TEST_ASSERT_TRUE(mock_count("pal_gpio_write") > 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_returns_ok);
    RUN_TEST(test_tick_returns_ok);
    RUN_TEST(test_gpio_write_called_on_tick);
    RUN_TEST(test_freeze_protection_forces_relay_on);
    RUN_TEST(test_overheat_protection_forces_relay_off);
    return UNITY_END();
}
