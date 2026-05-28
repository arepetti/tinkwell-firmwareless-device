/*
 * tw_button.c -- Debounced push-button.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_button.h"
#include "pal_gpio.h"
#include "pal_os.h"

#define DEBOUNCE_MS 50

/** Configures a pull-up input and callback for active-low press detection. */
tw_err_t tw_button_create(tw_button_t *btn, int pin,
                          tw_button_cb_t on_press, void *ctx)
{
    btn->pin        = pin;
    btn->on_press   = on_press;
    btn->ctx        = ctx;
    btn->_last_ms   = 0;
    btn->_last_level = false;

    return pal_gpio_init(pin, PAL_GPIO_INPUT_PULLUP);
}

/**
 * Debounced poll: invokes the press callback once per physical release-to-press edge;
 * readings exactly at the debounce boundary reuse the prior level so chatter does not double-trigger.
 */
void tw_button_poll(tw_button_t *btn)
{
    bool level = pal_gpio_read(btn->pin);
    uint64_t now = pal_uptime_ms();

    /* Active-low: button pressed when level is false. */
    if (!level && btn->_last_level && (now - btn->_last_ms) > DEBOUNCE_MS) {
        btn->_last_ms = now;
        if (btn->on_press)
            btn->on_press(btn->ctx);
    }
    btn->_last_level = level;
}
