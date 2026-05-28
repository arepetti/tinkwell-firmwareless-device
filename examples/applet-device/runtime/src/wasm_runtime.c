/*
 * wasm_runtime.c -- WAMR integration (stub).
 *
 * This file will eventually initialise WAMR, load a WASM module from
 * flash (or receive one pushed by the hub), and call the applet's
 * exported functions on each tick.
 *
 * For now it's a skeleton that demonstrates the lifecycle without
 * depending on WAMR.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wasm_runtime.h"
#include "pal_log.h"
#include "pal_flash.h"
#include "tw_config.h"

#include <string.h>
#include <stdio.h>

#define TAG "wasm"
#define APPLET_PARTITION "applet"

static bool applet_loaded = false;

tw_err_t wasm_runtime_init(const tw_device_config_t *dev)
{
    TW_UNUSED(dev);

    pal_flash_init(APPLET_PARTITION);

    /* Check if an applet is stored in flash. */
    size_t size = pal_flash_size(APPLET_PARTITION);
    if (size > 0) {
        PAL_LOGI(TAG, "found stored applet (%zu bytes), loading...", size);
        /*
         * TODO: Initialise WAMR, mmap the flash partition, instantiate
         * the module, register host bindings, call applet_init().
         */
        applet_loaded = true;
        PAL_LOGI(TAG, "applet loaded (stub -- WAMR not yet integrated)");
    } else {
        PAL_LOGI(TAG, "no applet stored -- waiting for hub to push one");
    }

    return TW_OK;
}

tw_err_t wasm_runtime_tick(const tw_device_config_t *dev)
{
    TW_UNUSED(dev);

    if (!applet_loaded)
        return TW_OK;

    /*
     * TODO: Call applet_tick() in the WASM module.
     * The applet reads sensors and controls GPIOs through host bindings.
     */

    return TW_OK;
}

tw_err_t wasm_runtime_on_command(const tw_device_config_t *dev,
                                 const char *command,
                                 const uint8_t *payload, size_t payload_len)
{
    TW_UNUSED(dev);

    if (strcmp(command, "app") == 0) {
        /*
         * TODO: Forward to the applet's on_command() export via WAMR.
         */
        PAL_LOGD(TAG, "app command received (%zu bytes)", payload_len);
        TW_UNUSED(payload);
    }

    return TW_OK;
}

tw_err_t wasm_runtime_heartbeat(uint8_t *buf, size_t buf_size,
                                size_t *out_len)
{
    const char *ver = applet_loaded ? "stub-1.0" : "";
    int n = snprintf((char *)buf, buf_size, "applet_version=%s", ver);
    *out_len = (size_t)n;
    return TW_OK;
}

tw_err_t wasm_on_coap(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    TW_UNUSED(req);
    /*
     * TODO: Forward GET requests to applet_on_coap() export if present.
     */
    return tw_msg_respond_text(resp, "applet messaging not yet implemented");
}

tw_err_t wasm_on_applet_status(tw_msg_request_t *req,
                               tw_msg_response_t *resp)
{
    TW_UNUSED(req);
    const char *state = applet_loaded ? "running" : "none";
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "state=%s", state);
    return tw_msg_respond_buf(resp, buf, (size_t)n);
}
