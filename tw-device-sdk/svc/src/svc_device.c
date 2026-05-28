/*
 * svc_device.c -- tw_device_run() implementation: the SDK main loop.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_device.h"
#include "tw_config.h"
#include "tw_coap_codes.h"
#include "tw_identity.h"
#include "tw_cmd.h"
#include "tw_msg.h"
#include "pal_log.h"
#include "pal_nvs.h"
#include "pal_os.h"
#include "pal_net.h"
#include "pal_power.h"
#include "pal_system.h"

#include <string.h>

#define TAG "device"

/* Internal service entry points (not in public headers). */
extern void     svc_sensor_init(void);
extern void     svc_sensor_poll(void);
extern void     svc_ota_init(void);
extern tw_err_t svc_heartbeat_init(tw_protocol_t *proto);
extern tw_err_t svc_heartbeat_send(const tw_device_config_t *cfg);
extern uint32_t svc_heartbeat_last_pending(void);
extern bool     svc_provision_is_needed(void);
extern tw_err_t svc_provision_run(const tw_device_config_t *cfg);
extern void     svc_reset_button_init(void);
extern void     svc_reset_button_poll(void);
extern void     svc_telemetry_init(tw_protocol_t *proto);
extern tw_err_t svc_telemetry_send(void);
extern bool     svc_telemetry_is_due(void);
extern void     svc_telemetry_schedule_next(void);
extern void     svc_power_init(void);
extern bool     svc_power_is_sleep_enabled(void);
extern void     svc_power_listen_window(const tw_device_config_t *cfg,
                                         tw_protocol_t *proto,
                                         uint32_t pending_commands);
extern void     svc_power_enter_sleep(const tw_device_config_t *cfg);
extern void     svc_power_notify_cmd_received(void);

static volatile bool s_shutdown;

#ifndef CONFIG_TW_COAP_PORT
#define CONFIG_TW_COAP_PORT TW_COAP_DEFAULT_PORT
#endif

/*
 * Bound protocol poll sleep: a non-zero minimum avoids a tight spin when the next tick
 * is overdue; the maximum caps latency so inbound CoAP work stays responsive vs CPU use.
 */
#define POLL_MIN_MS 1
#define POLL_MAX_MS 100

#ifndef CONFIG_TW_HEARTBEAT_INTERVAL_S
#define CONFIG_TW_HEARTBEAT_INTERVAL_S 60
#endif

#ifndef CONFIG_TW_HEARTBEAT_ON_BOOT
#define CONFIG_TW_HEARTBEAT_ON_BOOT 1
#endif

#ifndef CONFIG_TW_COAP_MAX_RESOURCES
#define CONFIG_TW_COAP_MAX_RESOURCES 16
#endif

/*
 * SDK-internal resources prepended/appended to the application table:
 *   GET  /tw/info            -- device identity summary
 *   GET  /tw/identity/pubkey -- Ed25519 public key export
 *   POST /tw/reboot          -- hub-pushed reboot
 *   POST /tw/set-config      -- hub-pushed config update
 *   POST /tw/ota-available   -- hub-pushed OTA notification
 *   POST /tw/app             -- hub-pushed application command
 */
#define SDK_RESOURCE_STATIC  2   /* info + pubkey */
#define SDK_RESOURCE_CMD     4   /* reboot + set-config + ota-available + app */
#define SDK_RESOURCE_TOTAL   (SDK_RESOURCE_STATIC + SDK_RESOURCE_CMD)

static tw_msg_resource_t s_merged_resources[CONFIG_TW_COAP_MAX_RESOURCES + SDK_RESOURCE_TOTAL + 1];

/**
 * Builds the merged resource table: SDK info endpoints, then hub-pushed
 * command endpoints, then the application-supplied table.
 */
static void build_resource_table(tw_msg_resource_t *app_resources)
{
    int i = 0;

    /* SDK GET endpoints. */
    s_merged_resources[i++] = (tw_msg_resource_t){
        .path    = "/tw/info",
        .methods = TW_MSG_GET,
        .handler = tw_identity_msg_info,
    };

    s_merged_resources[i++] = (tw_msg_resource_t){
        .path    = "/tw/identity/pubkey",
        .methods = TW_MSG_GET,
        .handler = tw_identity_msg_pubkey,
    };

    /* Hub-pushed command endpoints from svc_cmd.c. */
    for (tw_msg_resource_t *r = svc_cmd_resources; r->path; r++) {
        if (i >= CONFIG_TW_COAP_MAX_RESOURCES + SDK_RESOURCE_TOTAL)
            break;
        s_merged_resources[i++] = *r;
    }

    /* Application-defined resources. */
    if (app_resources) {
        for (tw_msg_resource_t *r = app_resources; r->path; r++) {
            if (i >= CONFIG_TW_COAP_MAX_RESOURCES + SDK_RESOURCE_TOTAL)
                break;
            s_merged_resources[i++] = *r;
        }
    }

    memset(&s_merged_resources[i], 0, sizeof(tw_msg_resource_t));
}

/** Sets the cooperative shutdown flag so the always-on main loop exits cleanly. */
void tw_device_request_shutdown(void)
{
    s_shutdown = true;
}

/**
 * SDK entry: provisions identity, starts PAL and services, runs one-shot deep-sleep
 * or the continuous tick/heartbeat loop until shutdown, then persists NVS and stops the stack.
 */
tw_err_t tw_device_run(const tw_device_config_t *cfg)
{
    s_shutdown = false;

    PAL_LOGI(TAG, "--- TW Device SDK ---");
    PAL_LOGI(TAG, "device : %s", cfg->name);
    PAL_LOGI(TAG, "fw     : %s", cfg->fw_version);
    PAL_LOGI(TAG, "chip   : %s", pal_system_chip_info());
    PAL_LOGI(TAG, "boot   : %d", pal_system_boot_reason());

    /* Initialise platform services. */
    pal_nvs_init();

    /* Identity (loads NVS overrides on top of compile-time defaults). */
    tw_identity_init(cfg);

    /* Provisioning check.
     *
     * When running on ESP-IDF with partition-based provisioning, the main
     * app does not contain provisioning code.  If not provisioned, reboot
     * to the factory partition which handles BLE/SoftAP/LAN provisioning.
     *
     * On POSIX builds (no partitions), fall back to inline provisioning.
     */
    if (svc_provision_is_needed()) {
#ifdef TW_PLATFORM_ESPIDF
        PAL_LOGI(TAG, "not provisioned -- rebooting to provisioning partition");
        pal_system_reboot_to_factory();
        return TW_ERR_NOT_READY;
#else
        PAL_LOGI(TAG, "device not provisioned -- entering provisioning mode");
        tw_err_t perr = svc_provision_run(cfg);
        if (perr != TW_OK)
            PAL_LOGW(TAG, "provisioning returned %d", perr);
        tw_identity_init(cfg);
#endif
    }

    pal_net_init();

    /* Initialise subsystems. */
    svc_cmd_init(cfg);
    svc_sensor_init();
    svc_ota_init();
    svc_power_init();
    svc_reset_button_init();

    /* Build merged resource table (SDK + application). */
    build_resource_table(cfg->resources);

    /* Instantiate the compile-time selected protocol backend. */
    tw_protocol_t *proto = tw_protocol_create();
    tw_err_t err = proto->init(proto, s_merged_resources, CONFIG_TW_COAP_PORT);
    if (!tw_ok(err)) {
        PAL_LOGE(TAG, "protocol init failed: %d", err);
        return err;
    }

    /* Heartbeat service (receives protocol handle for outbound messages). */
    svc_heartbeat_init(proto);
    svc_telemetry_init(proto);

    /* Wake callback (deep sleep: device just woke). */
    if (svc_power_is_sleep_enabled() && cfg->on_wake)
        cfg->on_wake(cfg);

    /* Boot heartbeat. */
#if CONFIG_TW_HEARTBEAT_ON_BOOT
    svc_heartbeat_send(cfg);
#endif

    /* Application init callback. */
    if (cfg->on_init) {
        err = cfg->on_init(cfg);
        if (!tw_ok(err)) {
            PAL_LOGE(TAG, "on_init failed: %d", err);
            return err;
        }
    }

    /*
     * Deep sleep path: one cycle then sleep.
     * Always-on path:  loop until shutdown requested.
     */
    if (svc_power_is_sleep_enabled()) {
        PAL_LOGI(TAG, "deep sleep mode: single cycle");

        svc_sensor_poll();
        if (cfg->on_tick)
            cfg->on_tick(cfg);

        svc_heartbeat_send(cfg);
        svc_telemetry_send();

        svc_power_listen_window(cfg, proto, svc_heartbeat_last_pending());

        proto->deinit(proto);
        pal_net_deinit();

        svc_power_enter_sleep(cfg);
        return TW_OK;
    }

    /* Always-on main loop. */
    PAL_LOGI(TAG, "entering main loop (tick every %u ms)",
             cfg->tick_interval_ms);

    uint64_t next_tick = pal_uptime_ms();
    uint64_t next_hb   = pal_uptime_ms() +
                          (uint64_t)CONFIG_TW_HEARTBEAT_INTERVAL_S * 1000;

    while (!s_shutdown) {
        uint64_t now = pal_uptime_ms();

        if (now >= next_tick) {
            svc_sensor_poll();
            svc_reset_button_poll();

            if (cfg->on_tick) {
                err = cfg->on_tick(cfg);
                if (!tw_ok(err))
                    PAL_LOGW(TAG, "on_tick error: %d", err);
            }

            next_tick = now + cfg->tick_interval_ms;
        }

        if (now >= next_hb) {
            svc_heartbeat_send(cfg);
            next_hb = now + (uint64_t)CONFIG_TW_HEARTBEAT_INTERVAL_S * 1000;
        }

        if (svc_telemetry_is_due()) {
            svc_telemetry_send();
            svc_telemetry_schedule_next();
        }

        int wait = (int)(next_tick - pal_uptime_ms());
        if (wait < POLL_MIN_MS) wait = POLL_MIN_MS;
        if (wait > POLL_MAX_MS) wait = POLL_MAX_MS;
        proto->poll(proto, wait);
    }

    PAL_LOGI(TAG, "shutdown requested, cleaning up...");
    proto->deinit(proto);
    pal_net_deinit();
    pal_nvs_commit();
    return TW_OK;
}
