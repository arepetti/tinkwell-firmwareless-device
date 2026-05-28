/*
 * mock_pal_gpio.c
 * SPDX-License-Identifier: MIT
 */

#include "pal_gpio.h"
#include "mock_pal.h"
#include <stdio.h>

static bool pin_state[64];

tw_err_t pal_gpio_init(int pin, pal_gpio_mode_t mode)
{
    char args[64];
    snprintf(args, sizeof(args), "pin=%d mode=%d", pin, (int)mode);
    mock_record("pal_gpio_init", args);
    return g_mock_cfg.next_error ? g_mock_cfg.next_error : TW_OK;
}

tw_err_t pal_gpio_write(int pin, bool level)
{
    char args[64];
    snprintf(args, sizeof(args), "pin=%d level=%d", pin, level);
    mock_record("pal_gpio_write", args);
    if (pin >= 0 && pin < 64) pin_state[pin] = level;
    return TW_OK;
}

bool pal_gpio_read(int pin)
{
    mock_record("pal_gpio_read", NULL);
    return g_mock_cfg.gpio_read_value != 0;
}

tw_err_t pal_gpio_set_interrupt(int pin, pal_gpio_edge_t edge,
                                pal_gpio_isr_t handler, void *ctx)
{
    mock_record("pal_gpio_set_interrupt", NULL);
    (void)pin; (void)edge; (void)handler; (void)ctx;
    return TW_OK;
}
