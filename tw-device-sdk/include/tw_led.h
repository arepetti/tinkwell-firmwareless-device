/*
 * tw_led.h -- LED pattern utility.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_LED_H
#define TW_LED_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Visual pattern driven by ::tw_led_tick timing. */
typedef enum {
    TW_LED_OFF,         /**< GPIO held in the inactive (off) state. */
    TW_LED_SOLID,       /**< GPIO held in the active (on) state. */
    TW_LED_BLINK_SLOW,  /**< On/off blink with ~500 ms period (depends on ::tw_led_tick rate). */
    TW_LED_BLINK_FAST,  /**< On/off blink with ~150 ms period (depends on ::tw_led_tick rate). */
    TW_LED_PULSE,       /**< Brief on pulse then extended off (attention blink). */
} tw_led_pattern_t;

/** @brief State for one GPIO-driven LED and its timing bookkeeping. */
typedef struct {
    int              pin;             /**< Platform GPIO number passed to the PAL. */
    tw_led_pattern_t pattern;         /**< Active pattern; updated via ::tw_led_set_pattern. */
    uint64_t         _next_toggle_ms; /**< Next scheduled transition time (SDK internal). */
    bool             _state;          /**< Current on/off phase for patterns (SDK internal). */
} tw_led_t;

/**
 * @brief Binds an LED instance to a GPIO pin and default pattern.
 * @param led Uninitialized LED state.
 * @param pin Platform-specific GPIO index for the LED anode/cathode drive.
 * @retval TW_OK if the pin was configured.
 * @retval TW_ERR_IO or ::TW_ERR_INVAL on PAL failure.
 */
tw_err_t tw_led_create(tw_led_t *led, int pin);

/**
 * @brief Changes the active pattern; resets internal timing for the new pattern.
 * @param led Initialized LED.
 * @param pattern Desired ::tw_led_pattern_t.
 */
void     tw_led_set_pattern(tw_led_t *led, tw_led_pattern_t pattern);

/**
 * @brief Advances time-based patterns; must be called periodically from the main tick loop.
 * @param led Initialized LED.
 */
void     tw_led_tick(tw_led_t *led);

#ifdef __cplusplus
}
#endif

#endif /* TW_LED_H */
