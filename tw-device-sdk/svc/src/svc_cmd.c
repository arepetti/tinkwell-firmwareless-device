/*
 * svc_cmd.c -- Hub-pushed command endpoint handlers.
 *
 * Each handler decodes a protobuf payload (nanopb) and dispatches to the
 * appropriate SDK service or application callback.  The hub sends these
 * as individual CoAP POST requests after the device heartbeat.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_cmd.h"
#include "tw_device.h"
#include "tw_config.h"
#include "tw_lock.h"
#include "pal_log.h"
#include "pal_nvs.h"
#include "pal_system.h"

#include <string.h>
#include <stdint.h>

#ifdef CONFIG_TW_USE_PROTOBUF
#include <pb_decode.h>
#include "tw_protocol.pb.h"
#endif

#define TAG "cmd"

static const tw_device_config_t *s_cfg;

void svc_cmd_init(const struct tw_device_config *cfg)
{
    s_cfg = cfg;
}

/* ── POST /tw/reboot ───────────────────────────────────────────────────── */

static tw_err_t svc_cmd_on_reboot(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    (void)req;
    PAL_LOGW(TAG, "hub requested reboot");
    tw_msg_respond_empty(resp, TW_MSG_204_CHANGED);
    pal_system_reboot();
    return TW_OK;
}

/* ── POST /tw/set-config ───────────────────────────────────────────────── */

static tw_err_t svc_cmd_on_set_config(tw_msg_request_t *req, tw_msg_response_t *resp)
{
#ifdef CONFIG_TW_USE_PROTOBUF
    if (!req->payload || req->payload_len == 0) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "empty payload");
    }

    tw_SetConfigCmd msg = tw_SetConfigCmd_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(req->payload, req->payload_len);

    if (!pb_decode(&stream, tw_SetConfigCmd_fields, &msg)) {
        PAL_LOGW(TAG, "set-config decode failed: %s", PB_GET_ERROR(&stream));
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "decode error");
    }

    for (pb_size_t i = 0; i < msg.entries_count; i++) {
        const tw_ConfigEntry *e = &msg.entries[i];
        PAL_LOGI(TAG, "set-config: %s=%s", e->key, e->value);
        tw_config_set_str(e->key, e->value);
    }

    return tw_msg_respond_empty(resp, TW_MSG_204_CHANGED);
#else
    if (s_cfg && s_cfg->on_command) {
        s_cfg->on_command(s_cfg, "set-config", req->payload, req->payload_len);
    }
    return tw_msg_respond_empty(resp, TW_MSG_204_CHANGED);
#endif
}

/* ── POST /tw/ota-available ────────────────────────────────────────────── */

static tw_err_t svc_cmd_on_ota_available(tw_msg_request_t *req, tw_msg_response_t *resp)
{
#ifdef CONFIG_TW_USE_PROTOBUF
    if (!req->payload || req->payload_len == 0) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "empty payload");
    }

    tw_OtaAvailableCmd msg = tw_OtaAvailableCmd_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(req->payload, req->payload_len);

    if (!pb_decode(&stream, tw_OtaAvailableCmd_fields, &msg)) {
        PAL_LOGW(TAG, "ota-available decode failed: %s", PB_GET_ERROR(&stream));
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "decode error");
    }

    /* H6 fix: validate size fits int32 before storing, since NVS uses i32. */
    if (msg.size_bytes > (uint32_t)INT32_MAX) {
        PAL_LOGW(TAG, "ota-available: size %u exceeds INT32_MAX",
                 (unsigned)msg.size_bytes);
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "size too large");
    }

    PAL_LOGI(TAG, "ota-available: url=%s size=%u", msg.url, (unsigned)msg.size_bytes);

    tw_config_set_str("ota_url", msg.url);
    tw_config_set_i32("ota_size", (int32_t)msg.size_bytes);
    if (msg.sha256.size == 32) {
        pal_nvs_set_blob("ota_sha256", msg.sha256.bytes, 32);
        pal_nvs_commit();
    }
#else
    (void)req;
    PAL_LOGI(TAG, "ota-available (raw, no protobuf)");
#endif

    if (s_cfg && s_cfg->on_command) {
        s_cfg->on_command(s_cfg, "ota-available", req->payload, req->payload_len);
    }
    return tw_msg_respond_empty(resp, TW_MSG_204_CHANGED);
}

/* ── POST /tw/app ──────────────────────────────────────────────────────── */

static tw_err_t svc_cmd_on_app(tw_msg_request_t *req, tw_msg_response_t *resp)
{
#ifdef CONFIG_TW_USE_PROTOBUF
    tw_AppCmd msg = tw_AppCmd_init_zero;

    if (req->payload && req->payload_len > 0) {
        pb_istream_t stream = pb_istream_from_buffer(req->payload, req->payload_len);
        if (!pb_decode(&stream, tw_AppCmd_fields, &msg)) {
            PAL_LOGW(TAG, "app cmd decode failed: %s", PB_GET_ERROR(&stream));
            return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "decode error");
        }
    }

    if (s_cfg && s_cfg->on_command) {
        s_cfg->on_command(s_cfg, "app", msg.payload.bytes, msg.payload.size);
    }
#else
    if (s_cfg && s_cfg->on_command) {
        s_cfg->on_command(s_cfg, "app", req->payload, req->payload_len);
    }
#endif

    return tw_msg_respond_empty(resp, TW_MSG_204_CHANGED);
}

/* ── Resource table ────────────────────────────────────────────────────── */

tw_msg_resource_t svc_cmd_resources[] = {
    { "/tw/reboot",        TW_MSG_POST, svc_cmd_on_reboot },
    { "/tw/set-config",    TW_MSG_POST, svc_cmd_on_set_config },
    { "/tw/ota-available", TW_MSG_POST, svc_cmd_on_ota_available },
    { "/tw/app",           TW_MSG_POST, svc_cmd_on_app },
    TW_MSG_RESOURCE_END
};
