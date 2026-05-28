/*
 * tw_led.c -- LED pattern driver.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_led.h"
#include "pal_gpio.h"
#include "pal_os.h"

#define LED_BLINK_SLOW_PERIOD_MS 500
#define LED_BLINK_FAST_PERIOD_MS 150
#define LED_PULSE_PERIOD_MS      100

/** Configures GPIO and initial pattern state for software-driven LED patterns. */
tw_err_t tw_led_create(tw_led_t *led, int pin)
{
    led->pin             = pin;
    led->pattern         = TW_LED_OFF;
    led->_next_toggle_ms = 0;
    led->_state          = false;
    return pal_gpio_init(pin, PAL_GPIO_OUTPUT);
}

/** Applies a steady or timed pattern; immediate output for OFF/SOLID, timers reset for others. */
void tw_led_set_pattern(tw_led_t *led, tw_led_pattern_t pattern)
{
    if (led->pattern == pattern) return;
    led->pattern = pattern;
    led->_next_toggle_ms = 0;

    switch (pattern) {
    case TW_LED_OFF:
        led->_state = false;
        pal_gpio_write(led->pin, false);
        break;
    case TW_LED_SOLID:
        led->_state = true;
        pal_gpio_write(led->pin, true);
        break;
    default:
        break;
    }
}

/** Advances blink/pulse timing; call periodically from the main loop or a timer. */
void tw_led_tick(tw_led_t *led)
{
    uint32_t period;
    switch (led->pattern) {
    case TW_LED_BLINK_SLOW: period = LED_BLINK_SLOW_PERIOD_MS; break;
    case TW_LED_BLINK_FAST: period = LED_BLINK_FAST_PERIOD_MS; break;
    case TW_LED_PULSE:      period = LED_PULSE_PERIOD_MS; break;
    default: return;
    }

    uint64_t now = pal_uptime_ms();
    if (now < led->_next_toggle_ms) return;

    led->_next_toggle_ms = now + period;
    led->_state = !led->_state;
    pal_gpio_write(led->pin, led->_state);

    if (led->pattern == TW_LED_PULSE && !led->_state)
        led->pattern = TW_LED_OFF;
}
