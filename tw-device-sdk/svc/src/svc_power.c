/*
 * svc_power.c -- Sleep/wake orchestration.
 *
 * When deep sleep is enabled (CONFIG_TW_POWER_DEEP_SLEEP), the
 * device follows this cycle:
 *
 *   1. Wake (timer or GPIO)
 *   2. Call on_wake callback
 *   3. Read sensors, send heartbeat, process mailbox
 *   4. Stay awake for listen_window_s (hub can push commands)
 *   5. Call on_sleep callback
 *   6. Enter deep sleep for sleep_interval_s
 *
 * In always-on mode this service is a no-op.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_types.h"
#include "tw_device.h"
#include "tw_msg.h"
#include "pal_power.h"
#include "pal_os.h"
#include "pal_log.h"

#define TAG "power"

/** Default sleep quantum when the app leaves tick_interval_ms at zero (avoids a zero-duration loop). */
#define FALLBACK_TICK_MS 1000

#ifndef CONFIG_TW_POWER_DEEP_SLEEP
#define CONFIG_TW_POWER_DEEP_SLEEP 0
#endif

#ifndef CONFIG_TW_SLEEP_INTERVAL_S
#define CONFIG_TW_SLEEP_INTERVAL_S 300
#endif

#ifndef CONFIG_TW_SLEEP_LISTEN_WINDOW_S
#define CONFIG_TW_SLEEP_LISTEN_WINDOW_S 30
#endif

#ifndef CONFIG_TW_SLEEP_LISTEN_WINDOW_IDLE_S
#define CONFIG_TW_SLEEP_LISTEN_WINDOW_IDLE_S 5
#endif

#ifndef CONFIG_TW_SLEEP_LISTEN_EXTEND_ON_CMD
#define CONFIG_TW_SLEEP_LISTEN_EXTEND_ON_CMD 0
#endif

static bool s_sleep_enabled;

/** Records whether deep sleep cycling is compiled in and logs the configured intervals. */
void svc_power_init(void)
{
#if CONFIG_TW_POWER_DEEP_SLEEP
    s_sleep_enabled = true;
    PAL_LOGI(TAG, "deep sleep enabled: interval=%ds, listen=%ds",
             CONFIG_TW_SLEEP_INTERVAL_S,
             CONFIG_TW_SLEEP_LISTEN_WINDOW_S);
#else
    s_sleep_enabled = false;
    PAL_LOGI(TAG, "always-on mode");
#endif
}

/** True when the one-shot wake/sleep path should run instead of the always-on loop. */
bool svc_power_is_sleep_enabled(void)
{
    return s_sleep_enabled;
}

/** Forwards the platform wake reason (timer vs GPIO) for application logging. */
pal_wake_reason_t svc_power_wake_reason(void)
{
    return pal_power_wake_reason();
}

/*
 * Extend-on-command: set by the CoAP dispatch path (proto_coap.c or
 * the protocol backend) whenever a request is processed.  Reset by the
 * listen window loop after extending.
 */
#if CONFIG_TW_SLEEP_LISTEN_EXTEND_ON_CMD
static volatile bool s_cmd_received;

void svc_power_notify_cmd_received(void)
{
    s_cmd_received = true;
}
#else
void svc_power_notify_cmd_received(void) { }
#endif

/**
 * Keeps the device awake after heartbeat, polling the CoAP server so
 * inbound hub requests (commands, OTA blocks) are processed.
 *
 * Window selection:
 *   - pending_commands == 0: use CONFIG_TW_SLEEP_LISTEN_WINDOW_IDLE_S
 *   - pending_commands > 0:  use CONFIG_TW_SLEEP_LISTEN_WINDOW_S
 *
 * If CONFIG_TW_SLEEP_LISTEN_EXTEND_ON_CMD is enabled, receiving a
 * request resets the window timer to the full window duration.
 */
void svc_power_listen_window(const tw_device_config_t *cfg,
                             tw_protocol_t *proto,
                             uint32_t pending_commands)
{
    if (!s_sleep_enabled) return;

    uint32_t window_s = (pending_commands > 0)
                        ? (uint32_t)CONFIG_TW_SLEEP_LISTEN_WINDOW_S
                        : (uint32_t)CONFIG_TW_SLEEP_LISTEN_WINDOW_IDLE_S;

    if (window_s == 0) {
        PAL_LOGI(TAG, "listen window: 0s (sleeping immediately)");
        return;
    }

    PAL_LOGI(TAG, "listen window: %u seconds (pending=%u)",
             (unsigned)window_s, (unsigned)pending_commands);

    uint32_t start    = (uint32_t)pal_uptime_ms();
    uint32_t window_ms = window_s * 1000;
    int poll_ms = (int)(cfg->tick_interval_ms ? cfg->tick_interval_ms : FALLBACK_TICK_MS);
    if (poll_ms > 100) poll_ms = 100;

    while ((uint32_t)pal_uptime_ms() - start < window_ms) {
        if (cfg->on_tick)
            cfg->on_tick(cfg);

        proto->poll(proto, poll_ms);

#if CONFIG_TW_SLEEP_LISTEN_EXTEND_ON_CMD
        if (s_cmd_received) {
            s_cmd_received = false;
            start = (uint32_t)pal_uptime_ms();
            PAL_LOGD(TAG, "listen window extended (command received)");
        }
#endif
    }

    PAL_LOGI(TAG, "listen window expired");
}

/** Application hook then platform deep sleep for the configured interval (may not return on MCU). */
void svc_power_enter_sleep(const tw_device_config_t *cfg)
{
    if (!s_sleep_enabled) return;

    if (cfg->on_sleep)
        cfg->on_sleep(cfg);

    uint32_t sleep_ms = (uint32_t)CONFIG_TW_SLEEP_INTERVAL_S * 1000;
    PAL_LOGI(TAG, "entering deep sleep for %lu ms", (unsigned long)sleep_ms);

    pal_power_deep_sleep(sleep_ms);
    /* Does not return on real hardware.  On POSIX, simulates via sleep. */
}
