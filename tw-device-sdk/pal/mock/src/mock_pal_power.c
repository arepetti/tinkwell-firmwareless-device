/*
 * mock_pal_power.c
 * SPDX-License-Identifier: MIT
 */

#include "pal_power.h"
#include "mock_pal.h"

static pal_wake_reason_t s_wake = PAL_WAKE_UNKNOWN;

tw_err_t pal_power_deep_sleep(uint32_t duration_ms)
{
    char args[64];
    snprintf(args, sizeof(args), "ms=%lu", (unsigned long)duration_ms);
    mock_record("pal_power_deep_sleep", args);
    return TW_OK;
}

pal_wake_reason_t pal_power_wake_reason(void) { return s_wake; }

tw_err_t pal_power_set_wake_gpio(int pin, bool level)
{
    mock_record("pal_power_set_wake_gpio", NULL);
    (void)pin; (void)level;
    return TW_OK;
}
