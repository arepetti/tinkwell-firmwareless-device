/*
 * pal_power_posix.c -- Power management stub (log-only, no real sleep).
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_power.h"
#include "pal_log.h"
#include "pal_os.h"

#define TAG "power"

tw_err_t pal_power_deep_sleep(uint32_t duration_ms)
{
    PAL_LOGI(TAG, "deep sleep requested for %u ms (simulating with delay)", duration_ms);
    pal_sleep_ms(duration_ms);
    return TW_OK;
}

pal_wake_reason_t pal_power_wake_reason(void)
{
    return PAL_WAKE_UNKNOWN;
}

tw_err_t pal_power_set_wake_gpio(int pin, bool level)
{
    PAL_LOGD(TAG, "wake GPIO pin=%d level=%d (no-op on POSIX)", pin, level);
    return TW_OK;
}
