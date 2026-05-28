/*
 * pal_power_esp.c -- Deep sleep via ESP-IDF.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_power.h"
#include "pal_log.h"
#include "esp_sleep.h"

#define TAG "power"

tw_err_t pal_power_deep_sleep(uint32_t duration_ms)
{
    PAL_LOGI(TAG, "entering deep sleep for %lu ms",
             (unsigned long)duration_ms);

    esp_sleep_enable_timer_wakeup((uint64_t)duration_ms * 1000ULL);
    esp_deep_sleep_start();

    /* Never reached. */
    return TW_OK;
}

pal_wake_reason_t pal_power_wake_reason(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:     return PAL_WAKE_TIMER;
    case ESP_SLEEP_WAKEUP_GPIO:
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:      return PAL_WAKE_GPIO;
    default:                         return PAL_WAKE_UNKNOWN;
    }
}

tw_err_t pal_power_set_wake_gpio(int pin, bool level)
{
#if SOC_PM_SUPPORT_EXT0_WAKEUP
    esp_sleep_enable_ext0_wakeup((gpio_num_t)pin, level ? 1 : 0);
    return TW_OK;
#else
    TW_UNUSED(pin);
    TW_UNUSED(level);
    PAL_LOGW(TAG, "ext0 wakeup not supported on this chip");
    return TW_ERR_NOT_READY;
#endif
}
