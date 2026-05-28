/*
 * tw_button.h -- Debounced push-button utility.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_BUTTON_H
#define TW_BUTTON_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Invoked once per debounced press transition (see ::tw_button_poll).
 * @param ctx Opaque pointer from ::tw_button_t::ctx.
 */
typedef void (*tw_button_cb_t)(void *ctx);

/** @brief Debounced button state: wiring, callback, and edge timing. */
typedef struct {
    int            pin;          /**< GPIO number; active level defined by PAL (typically low when pressed). */
    tw_button_cb_t on_press;     /**< Called on a debounced press event. */
    void          *ctx;          /**< Opaque argument to @a on_press. */
    uint64_t       _last_ms;     /**< Last sample time for debounce (SDK internal). */
    bool           _last_level;  /**< Previous raw GPIO level (SDK internal). */
} tw_button_t;

/**
 * @brief Initializes a debounced button on @a pin with a press callback.
 * @param btn Uninitialized button state.
 * @param pin GPIO index for the switch input.
 * @param on_press Callback on debounced activation (may be NULL).
 * @param ctx Opaque pointer for @a on_press.
 * @retval TW_OK if GPIO was configured.
 * @retval TW_ERR_IO or ::TW_ERR_INVAL on failure.
 */
tw_err_t tw_button_create(tw_button_t *btn, int pin,
                          tw_button_cb_t on_press, void *ctx);

/**
 * @brief Samples the GPIO and applies debouncing; may invoke @a on_press once per transition.
 *
 * Call from the periodic tick at a fixed rate; debounce interval is derived from
 * successive call spacing and internal timestamps so mechanical bounce is filtered
 * before @a on_press runs.
 *
 * @param btn Initialized button.
 */
void     tw_button_poll(tw_button_t *btn);

#ifdef __cplusplus
}
#endif

#endif /* TW_BUTTON_H */
