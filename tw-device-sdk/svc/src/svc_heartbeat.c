/*
 * svc_heartbeat.c -- Hub heartbeat client (protobuf-encoded).
 *
 * Periodically sends a HeartbeatPayload protobuf to the hub via the
 * active protocol backend.  The hub responds with a HeartbeatReply
 * containing the count of pending commands it will push.
 *
 * Commands are no longer bundled in the heartbeat response -- they
 * arrive as individual CoAP POST requests to per-command endpoints
 * (see svc_cmd.c).
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_hub.h"
#include "tw_types.h"
#include "tw_device.h"
#include "tw_identity.h"
#include "tw_msg.h"
#include "tw_kvtext.h"
#include "tw_lock.h"
#include "pal_nvs.h"
#include "pal_os.h"
#include "pal_system.h"
#include "pal_log.h"
#include "tw_coap_codes.h"
#include "tw_net_constants.h"

#ifdef CONFIG_TW_USE_PROTOBUF
#include <pb_encode.h>
#include <pb_decode.h>
#include "tw_protocol.pb.h"
#endif

#include <string.h>
#include <stdio.h>

#define TAG "heartbeat"

#define NVS_KEY_HUB_ADDR  "hub_addr"
#define NVS_KEY_HUB_PORT  "hub_port"

/*
 * Hub uses the IANA coaps port so TLS/DTLS expectations match deployment;
 * heartbeats are still sent as cleartext CoAP when using the text stub.
 */
#define HUB_DEFAULT_PORT  TW_COAPS_DEFAULT_PORT
#define HEARTBEAT_PATH    "/hub/heartbeat"

#define TX_BUF_SIZE       512
#define RX_BUF_SIZE       256

#define HEARTBEAT_SEND_TIMEOUT_MS 5000

#define APP_PAYLOAD_BUF_SIZE 128

static tw_lock_t s_lock;
static char     s_hub_host[TW_HOST_BUF_SIZE];
static uint16_t s_hub_port = HUB_DEFAULT_PORT;
static bool     s_configured;

static tw_protocol_t *s_proto;

/* Tracks the last pending count from HeartbeatReply for listen window. */
static uint32_t s_last_pending;

/* ---- Hub address management ---- */

tw_err_t tw_hub_set_address(const char *coap_uri)
{
    if (!coap_uri) return TW_ERR_INVAL;

    const char *host_start = coap_uri;
    if (strncmp(coap_uri, "coap://", TW_COAP_URI_SCHEME_LEN) == 0)
        host_start += TW_COAP_URI_SCHEME_LEN;

    char host[TW_HOST_BUF_SIZE] = {0};
    uint16_t port = HUB_DEFAULT_PORT;

    /*
     * H7 fix: find the last colon to handle IPv6 literals (e.g. [::1]:5684).
     * For bracketed IPv6 the port follows ']:'.  For IPv4 or hostname it's
     * the last ':'.  Use strtoul with range check instead of atoi.
     */
    const char *port_sep = strrchr(host_start, ':');

    /* If it looks like a bare IPv6 (multiple colons, no brackets), ignore colons. */
    if (port_sep && port_sep != strchr(host_start, ':') &&
        host_start[0] != '[') {
        port_sep = NULL;
    }

    if (port_sep) {
        size_t hlen = (size_t)(port_sep - host_start);
        if (hlen >= sizeof(host)) return TW_ERR_OVERFLOW;
        memcpy(host, host_start, hlen);
        char *end = NULL;
        unsigned long p = strtoul(port_sep + 1, &end, 10);
        if (end != port_sep + 1 && p > 0 && p <= 65535)
            port = (uint16_t)p;
    } else {
        strncpy(host, host_start, sizeof(host) - 1);
    }

    strncpy(s_hub_host, host, sizeof(s_hub_host) - 1);
    s_hub_port   = port;
    s_configured = true;

    pal_nvs_set_str(NVS_KEY_HUB_ADDR, s_hub_host);
    pal_nvs_set_i32(NVS_KEY_HUB_PORT, (int32_t)s_hub_port);
    pal_nvs_commit();

    PAL_LOGI(TAG, "hub address set: %s:%u", s_hub_host, s_hub_port);
    return TW_OK;
}

tw_err_t tw_hub_get_address(char *buf, size_t buf_size)
{
    if (!s_configured) return TW_ERR_NOT_READY;
    snprintf(buf, buf_size, "coap://%s:%u", s_hub_host, s_hub_port);
    return TW_OK;
}

/** Restore hub host/port from NVS if previously configured. */
static void load_hub_address(void)
{
    if (pal_nvs_get_str(NVS_KEY_HUB_ADDR, s_hub_host, sizeof(s_hub_host)) != TW_OK)
        return;

    int32_t port = HUB_DEFAULT_PORT;
    pal_nvs_get_i32(NVS_KEY_HUB_PORT, &port);
    s_hub_port   = (uint16_t)port;
    s_configured = true;

    PAL_LOGI(TAG, "hub address loaded: %s:%u", s_hub_host, s_hub_port);
}

/* ---- Heartbeat payload ---- */

#ifdef CONFIG_TW_USE_PROTOBUF

/* Forward declaration from svc_sensor.c for sensor push. */
#if defined(CONFIG_TW_SENSOR_PUSH) && defined(CONFIG_TW_SENSOR_PUSH_IN_HEARTBEAT)
extern int svc_sensor_count(void);
extern tw_err_t svc_sensor_get_by_index(int idx, const char **name,
                                         int32_t *value);
#endif

/** Encode HeartbeatPayload protobuf into buf, return bytes written. */
static tw_err_t build_heartbeat_payload(const tw_device_config_t *cfg,
                                        uint8_t *buf, size_t buf_size,
                                        size_t *out_len)
{
    const tw_device_identity_t *id = tw_identity_get();

    tw_HeartbeatPayload msg = tw_HeartbeatPayload_init_zero;

    memcpy(msg.id.bytes, id->uuid, TW_UUID_SIZE);
    msg.id.size       = TW_UUID_SIZE;
    msg.vendor_id     = id->vendor_id;
    msg.product_id    = id->product_id;
    msg.serial_number = id->serial_number;
    if (cfg->fw_version)
        strncpy(msg.fw_version, cfg->fw_version, sizeof(msg.fw_version) - 1);
    msg.uptime_ms     = pal_uptime_ms();
    msg.free_heap     = pal_system_free_heap();
    msg.boot_reason   = (int32_t)pal_system_boot_reason();

    /* Optional application-defined heartbeat blob. */
    if (cfg->heartbeat_payload) {
        uint8_t app_buf[APP_PAYLOAD_BUF_SIZE];
        size_t  app_len = 0;
        if (cfg->heartbeat_payload(app_buf, sizeof(app_buf), &app_len) == TW_OK &&
            app_len > 0 && app_len <= sizeof(msg.app_data.bytes)) {
            memcpy(msg.app_data.bytes, app_buf, app_len);
            msg.app_data.size = (pb_size_t)app_len;
        }
    }

    /* Optional sensor readings in heartbeat. */
#if defined(CONFIG_TW_SENSOR_PUSH) && defined(CONFIG_TW_SENSOR_PUSH_IN_HEARTBEAT)
    {
        int count = svc_sensor_count();
        int limit = count < (int)tw_HeartbeatPayload_sensors_max_count
                    ? count : (int)tw_HeartbeatPayload_sensors_max_count;
        for (int i = 0; i < limit; i++) {
            const char *sname = NULL;
            int32_t sval = 0;
            if (svc_sensor_get_by_index(i, &sname, &sval) == TW_OK && sname) {
                tw_SensorReading *sr = &msg.sensors[msg.sensors_count];
                strncpy(sr->name, sname, sizeof(sr->name) - 1);
                sr->value = (double)sval;
                sr->timestamp_ms = pal_uptime_ms();
                msg.sensors_count++;
            }
        }
    }
#endif

    pb_ostream_t stream = pb_ostream_from_buffer(buf, buf_size);
    if (!pb_encode(&stream, tw_HeartbeatPayload_fields, &msg)) {
        PAL_LOGE(TAG, "HeartbeatPayload encode failed: %s", PB_GET_ERROR(&stream));
        *out_len = 0;
        return TW_ERR_OVERFLOW;
    }

    *out_len = stream.bytes_written;
    return TW_OK;
}

/** Decode HeartbeatReply protobuf, return pending command count. */
static uint32_t decode_heartbeat_reply(const uint8_t *data, size_t len)
{
    tw_HeartbeatReply reply = tw_HeartbeatReply_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(data, len);
    if (!pb_decode(&stream, tw_HeartbeatReply_fields, &reply)) {
        PAL_LOGW(TAG, "HeartbeatReply decode failed: %s", PB_GET_ERROR(&stream));
        return 0;
    }
    return reply.pending;
}

#else /* !CONFIG_TW_USE_PROTOBUF */

/** kvtext fallback heartbeat payload builder. */
static tw_err_t build_heartbeat_payload(const tw_device_config_t *cfg,
                                        uint8_t *buf, size_t buf_size,
                                        size_t *out_len)
{
    const tw_device_identity_t *id = tw_identity_get();

    char *cbuf = (char *)buf;
    size_t pos = 0;

    tw_kvtext_write_hex(cbuf, buf_size, &pos, "uuid",
                        id->uuid, TW_UUID_SIZE);
    tw_kvtext_write_i32(cbuf, buf_size, &pos, "vendor-id", id->vendor_id);
    tw_kvtext_write_i32(cbuf, buf_size, &pos, "product-id", id->product_id);
    tw_kvtext_write_i32(cbuf, buf_size, &pos, "serial-number",
                        id->serial_number);
    tw_kvtext_write_u8(cbuf, buf_size, &pos, "variant", id->variant);
    tw_kvtext_write_str(cbuf, buf_size, &pos, "fw-version", cfg->fw_version);

    char uptime[24];
    snprintf(uptime, sizeof(uptime), "%llu",
             (unsigned long long)pal_uptime_ms());
    tw_kvtext_write_str(cbuf, buf_size, &pos, "uptime-ms", uptime);

    char heap[16];
    snprintf(heap, sizeof(heap), "%lu",
             (unsigned long)pal_system_free_heap());
    tw_kvtext_write_str(cbuf, buf_size, &pos, "free-heap", heap);

    tw_kvtext_write_i32(cbuf, buf_size, &pos, "boot-reason",
                        (int32_t)pal_system_boot_reason());

    if (cfg->heartbeat_payload) {
        uint8_t app_buf[APP_PAYLOAD_BUF_SIZE];
        size_t  app_len = 0;
        if (cfg->heartbeat_payload(app_buf, sizeof(app_buf), &app_len) == TW_OK &&
            app_len > 0 && app_len < buf_size - pos) {
            memcpy(cbuf + pos, app_buf, app_len);
            pos += app_len;
        }
    }

    *out_len = pos;
    return TW_OK;
}

#endif /* CONFIG_TW_USE_PROTOBUF */

/* ---- Public: called by svc_device.c ---- */

tw_err_t svc_heartbeat_init(tw_protocol_t *proto)
{
    tw_lock_init(&s_lock);
    s_proto = proto;
    s_last_pending = 0;
    load_hub_address();
    return TW_OK;
}

uint32_t svc_heartbeat_last_pending(void)
{
    tw_lock_acquire(s_lock);
    uint32_t p = s_last_pending;
    tw_lock_release(s_lock);
    return p;
}

tw_err_t svc_heartbeat_send(const tw_device_config_t *cfg)
{
    if (!s_configured) {
        PAL_LOGD(TAG, "no hub configured, skipping heartbeat");
        return TW_ERR_NOT_READY;
    }

    uint8_t tx_buf[TX_BUF_SIZE];
    size_t  tx_len = 0;

    tw_err_t err = build_heartbeat_payload(cfg, tx_buf, sizeof(tx_buf), &tx_len);
    if (err != TW_OK) return err;

    uint8_t rx_buf[RX_BUF_SIZE];
    size_t  rx_len = 0;

    if (s_proto && s_proto->send) {
        err = s_proto->send(s_proto, s_hub_host, s_hub_port,
                            HEARTBEAT_PATH,
                            tx_buf, tx_len,
                            rx_buf, sizeof(rx_buf), &rx_len,
                            HEARTBEAT_SEND_TIMEOUT_MS);
    } else {
        PAL_LOGW(TAG, "no protocol backend available for heartbeat");
        return TW_ERR_NOT_READY;
    }

    if (err != TW_OK) {
        PAL_LOGW(TAG, "heartbeat send failed: %d", err);
        return err;
    }

    PAL_LOGD(TAG, "heartbeat sent (%zu bytes)", tx_len);

    if (rx_len > 0) {
#ifdef CONFIG_TW_USE_PROTOBUF
        s_last_pending = decode_heartbeat_reply(rx_buf, rx_len);
        PAL_LOGD(TAG, "hub response: %zu bytes, pending=%u",
                 rx_len, (unsigned)s_last_pending);
#else
        PAL_LOGD(TAG, "hub response: %zu bytes", rx_len);
        s_last_pending = 0;
#endif
    } else {
        s_last_pending = 0;
    }

    return TW_OK;
}
