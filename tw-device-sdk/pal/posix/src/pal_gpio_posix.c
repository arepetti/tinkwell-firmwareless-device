/*
 * pal_gpio_posix.c -- GPIO simulation via in-memory state.
 *
 * On POSIX there is no real GPIO.  Pins are tracked in an array so the
 * rest of the SDK works.  Integration tests can inject button presses
 * by calling pal_gpio_sim_set() (declared here, not in the public header).
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_gpio.h"
#include "pal_log.h"

#define TAG "gpio"
#define MAX_PINS 32

static struct {
    bool            configured;
    pal_gpio_mode_t mode;
    bool            level;
    pal_gpio_isr_t  isr;
    void           *isr_ctx;
} pins[MAX_PINS];

tw_err_t pal_gpio_init(int pin, pal_gpio_mode_t mode)
{
    if (pin < 0 || pin >= MAX_PINS) return TW_ERR_INVAL;
    pins[pin].configured = true;
    pins[pin].mode  = mode;
    pins[pin].level = false;
    PAL_LOGD(TAG, "pin %d configured mode=%d", pin, mode);
    return TW_OK;
}

tw_err_t pal_gpio_write(int pin, bool level)
{
    if (pin < 0 || pin >= MAX_PINS || !pins[pin].configured)
        return TW_ERR_INVAL;
    pins[pin].level = level;
    return TW_OK;
}

bool pal_gpio_read(int pin)
{
    if (pin < 0 || pin >= MAX_PINS) return false;
    return pins[pin].level;
}

tw_err_t pal_gpio_set_interrupt(int pin, pal_gpio_edge_t edge,
                                pal_gpio_isr_t handler, void *ctx)
{
    TW_UNUSED(edge);
    if (pin < 0 || pin >= MAX_PINS) return TW_ERR_INVAL;
    pins[pin].isr     = handler;
    pins[pin].isr_ctx = ctx;
    return TW_OK;
}

/* Test helper: simulate an external pin level change. */
void pal_gpio_sim_set(int pin, bool level)
{
    if (pin < 0 || pin >= MAX_PINS) return;
    bool prev = pins[pin].level;
    pins[pin].level = level;
    if (pins[pin].isr && prev != level)
        pins[pin].isr(pin, pins[pin].isr_ctx);
}
