/*
 * svc_reset_button.c -- GPIO button for user-initiated re-provisioning.
 *
 * When CONFIG_TW_REPROVISION_GPIO is enabled, a GPIO pin (active low with
 * internal pull-up) is monitored.  Holding the button for
 * CONFIG_TW_REPROVISION_GPIO_HOLD_S seconds clears hub provisioning
 * state and reboots to the provisioning partition.
 *
 * No automatic re-provisioning on WiFi failure -- the user must
 * physically press the button.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_types.h"
#include "tw_device.h"
#include "pal_gpio.h"
#include "pal_nvs.h"
#include "pal_os.h"
#include "pal_system.h"
#include "pal_log.h"

#define TAG "reset-btn"

#ifndef CONFIG_TW_REPROVISION_GPIO
#define CONFIG_TW_REPROVISION_GPIO 0
#endif

#ifndef CONFIG_TW_REPROVISION_GPIO_PIN
#define CONFIG_TW_REPROVISION_GPIO_PIN 9
#endif

#ifndef CONFIG_TW_REPROVISION_GPIO_HOLD_S
#define CONFIG_TW_REPROVISION_GPIO_HOLD_S 5
#endif

#if CONFIG_TW_REPROVISION_GPIO

static uint32_t s_hold_start_ms;
static bool     s_holding;

void svc_reset_button_init(void)
{
    pal_gpio_init(CONFIG_TW_REPROVISION_GPIO_PIN, PAL_GPIO_INPUT_PULLUP);
    s_holding = false;
    s_hold_start_ms = 0;
    PAL_LOGI(TAG, "reset button on GPIO %d (hold %ds to re-provision)",
             CONFIG_TW_REPROVISION_GPIO_PIN, CONFIG_TW_REPROVISION_GPIO_HOLD_S);
}

void svc_reset_button_poll(void)
{
    /* Active low: button pressed = level 0. */
    bool level = pal_gpio_read(CONFIG_TW_REPROVISION_GPIO_PIN);

    if (!level) {
        if (!s_holding) {
            s_holding = true;
            s_hold_start_ms = (uint32_t)pal_uptime_ms();
        }

        uint32_t held_ms = (uint32_t)pal_uptime_ms() - s_hold_start_ms;
        uint32_t required_ms = (uint32_t)CONFIG_TW_REPROVISION_GPIO_HOLD_S * 1000;

        if (held_ms >= required_ms) {
            PAL_LOGW(TAG, "reset button held %us -- clearing provisioning",
                     (unsigned)CONFIG_TW_REPROVISION_GPIO_HOLD_S);

            /* Clear hub provisioning state but preserve factory identity. */
            pal_nvs_set_i32("id_hubprov", 0);
            pal_nvs_commit();

            PAL_LOGW(TAG, "rebooting to provisioning partition...");
#ifdef TW_PLATFORM_ESPIDF
            pal_system_reboot_to_factory();
#else
            pal_system_reboot();
#endif
        }
    } else {
        if (s_holding) {
            s_holding = false;
        }
    }
}

#else /* !CONFIG_TW_REPROVISION_GPIO */

void svc_reset_button_init(void) { }
void svc_reset_button_poll(void) { }

#endif
