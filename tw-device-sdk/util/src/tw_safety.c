/*
 * tw_safety.c -- Generic safety monitor.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_safety.h"

/** Binds thresholds and callbacks; starts in TW_SAFETY_NORMAL. */
tw_err_t tw_safety_create(tw_safety_monitor_t *mon,
                          const tw_safety_config_t *cfg)
{
    if (!mon || !cfg) return TW_ERR_INVAL;
    mon->cfg   = *cfg;
    mon->state = TW_SAFETY_NORMAL;
    return TW_OK;
}

/**
 * Classifies the sample against thresholds. Callbacks run only on *entry* to LOW/HIGH
 * (not on every sample while latched), so sustained out-of-range readings do not
 * flood handlers; returning to the band clears state to NORMAL without notification.
 */
void tw_safety_check(tw_safety_monitor_t *mon, int32_t reading)
{
    tw_safety_event_t prev = mon->state;

    if (reading <= mon->cfg.low_threshold) {
        mon->state = TW_SAFETY_LOW;
        if (prev != TW_SAFETY_LOW && mon->cfg.on_low)
            mon->cfg.on_low(TW_SAFETY_LOW, mon->cfg.ctx);
    } else if (reading >= mon->cfg.high_threshold) {
        mon->state = TW_SAFETY_HIGH;
        if (prev != TW_SAFETY_HIGH && mon->cfg.on_high)
            mon->cfg.on_high(TW_SAFETY_HIGH, mon->cfg.ctx);
    } else {
        mon->state = TW_SAFETY_NORMAL;
    }
}

/** True while the last classification was not NORMAL (sticky until the next in-range sample). */
bool tw_safety_is_overriding(const tw_safety_monitor_t *mon)
{
    return mon->state != TW_SAFETY_NORMAL;
}
