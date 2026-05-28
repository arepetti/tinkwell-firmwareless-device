/*
 * tw_safety.h -- Generic safety monitor (threshold-based with callbacks).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_SAFETY_H
#define TW_SAFETY_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Classification of the latest reading relative to configured thresholds. */
typedef enum {
    TW_SAFETY_NORMAL, /**< Reading lies between low and high thresholds (inclusive per implementation). */
    TW_SAFETY_LOW,    /**< Reading fell below @a low_threshold in ::tw_safety_config_t. */
    TW_SAFETY_HIGH,   /**< Reading exceeded @a high_threshold in ::tw_safety_config_t. */
} tw_safety_event_t;

/**
 * @brief Application callback when the monitor classifies a reading relative to thresholds.
 * @param event Current classification (::TW_SAFETY_LOW, ::TW_SAFETY_HIGH, or ::TW_SAFETY_NORMAL).
 * @param ctx Opaque pointer from ::tw_safety_config_t::ctx.
 */
typedef void (*tw_safety_cb_t)(tw_safety_event_t event, void *ctx);

/** @brief Thresholds and callbacks for ::tw_safety_create. */
typedef struct {
    int32_t        low_threshold;  /**< Value at or below which a low condition is recognized. */
    int32_t        high_threshold; /**< Value at or above which a high condition is recognized. */
    tw_safety_cb_t on_low;         /**< Invoked when entering or while in low state (see implementation). */
    tw_safety_cb_t on_high;        /**< Invoked when entering or while in high state (see implementation). */
    void          *ctx;             /**< Opaque pointer passed to callbacks. */
} tw_safety_config_t;

/** @brief Runtime monitor holding configuration copy and last classified state. */
typedef struct {
    tw_safety_config_t cfg; /**< Snapshot of configuration at creation time. */
    tw_safety_event_t  state; /**< Last reported classification after ::tw_safety_check. */
} tw_safety_monitor_t;

/**
 * @brief Initializes a monitor from @a cfg (copies thresholds and callbacks).
 * @param mon Uninitialized monitor storage.
 * @param cfg Non-NULL configuration; must outlive the monitor if pointers inside are used.
 * @retval TW_OK on success.
 * @retval TW_ERR_INVAL if thresholds or @a mon are invalid (e.g. low > high).
 */
tw_err_t tw_safety_create(tw_safety_monitor_t *mon,
                          const tw_safety_config_t *cfg);

/**
 * @brief Feeds a new sensor reading into the monitor and may invoke callbacks.
 * @param mon Initialized monitor.
 * @param reading Current integer sample in the same units as thresholds.
 */
void     tw_safety_check(tw_safety_monitor_t *mon, int32_t reading);

/**
 * @brief Returns whether the monitor is currently in an out-of-band (low or high) condition.
 *
 * Used to let application logic suppress actuation when safety limits are breached until
 * readings return to the normal band.
 *
 * @param mon Initialized monitor.
 * @return True if @a state is not ::TW_SAFETY_NORMAL; false if normal.
 */
bool     tw_safety_is_overriding(const tw_safety_monitor_t *mon);

#ifdef __cplusplus
}
#endif

#endif /* TW_SAFETY_H */
