/*
 * svc_telemetry.c -- Dedicated sensor telemetry push to the hub.
 *
 * When CONFIG_TW_SENSOR_PUSH_DEDICATED is enabled, this service
 * periodically sends a TelemetryPush protobuf to the hub on its
 * own schedule, independent of the heartbeat interval.
 *
 * The hub can respond with TelemetryReply.next_interval_s to
 * dynamically adjust the push rate (0 = keep current).
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_types.h"
#include "tw_device.h"
#include "tw_msg.h"
#include "tw_hub.h"
#include "pal_os.h"
#include "pal_log.h"

#ifdef CONFIG_TW_USE_PROTOBUF
#include <pb_encode.h>
#include <pb_decode.h>
#include "tw_protocol.pb.h"
#endif

#include <string.h>
#include <stdlib.h>

#define TAG "telemetry"

#ifndef CONFIG_TW_SENSOR_PUSH_DEDICATED
#define CONFIG_TW_SENSOR_PUSH_DEDICATED 0
#endif

#ifndef CONFIG_TW_SENSOR_PUSH_INTERVAL_S
#define CONFIG_TW_SENSOR_PUSH_INTERVAL_S 10
#endif

#ifndef CONFIG_TW_SENSOR_PUSH_PATH
#define CONFIG_TW_SENSOR_PUSH_PATH "/hub/telemetry"
#endif

#define TELEMETRY_TIMEOUT_MS 5000
#define TX_BUF_SIZE 512
#define RX_BUF_SIZE 64

#if CONFIG_TW_SENSOR_PUSH_DEDICATED

/* Forward declarations from svc_sensor.c. */
extern int svc_sensor_count(void);
extern tw_err_t svc_sensor_get_by_index(int idx, const char **name,
                                         int32_t *value);

static tw_protocol_t *s_proto;
static uint32_t s_interval_s;
static uint64_t s_next_push_ms;
static bool     s_initialized;

void svc_telemetry_init(tw_protocol_t *proto)
{
    s_proto = proto;
    s_interval_s = CONFIG_TW_SENSOR_PUSH_INTERVAL_S;
    s_next_push_ms = pal_uptime_ms() + (uint64_t)s_interval_s * 1000;
    s_initialized = true;
    PAL_LOGI(TAG, "dedicated telemetry push enabled (interval=%us, path=%s)",
             (unsigned)s_interval_s, CONFIG_TW_SENSOR_PUSH_PATH);
}

tw_err_t svc_telemetry_send(void)
{
    if (!s_initialized || !s_proto) return TW_ERR_NOT_READY;

#ifdef CONFIG_TW_USE_PROTOBUF
    tw_TelemetryPush msg = tw_TelemetryPush_init_zero;

    int count = svc_sensor_count();
    int limit = count < (int)tw_TelemetryPush_readings_max_count
                ? count : (int)tw_TelemetryPush_readings_max_count;

    for (int i = 0; i < limit; i++) {
        const char *name = NULL;
        int32_t val = 0;
        if (svc_sensor_get_by_index(i, &name, &val) == TW_OK && name) {
            tw_SensorReading *r = &msg.readings[msg.readings_count];
            strncpy(r->name, name, sizeof(r->name) - 1);
            r->value = (double)val;
            r->timestamp_ms = pal_uptime_ms();
            msg.readings_count++;
        }
    }

    if (msg.readings_count == 0) {
        PAL_LOGD(TAG, "no sensor readings to push");
        return TW_OK;
    }

    uint8_t tx_buf[TX_BUF_SIZE];
    pb_ostream_t stream = pb_ostream_from_buffer(tx_buf, sizeof(tx_buf));
    if (!pb_encode(&stream, tw_TelemetryPush_fields, &msg)) {
        PAL_LOGW(TAG, "TelemetryPush encode failed: %s", PB_GET_ERROR(&stream));
        return TW_ERR_OVERFLOW;
    }

    char hub_addr[80];
    if (tw_hub_get_address(hub_addr, sizeof(hub_addr)) != TW_OK) {
        return TW_ERR_NOT_READY;
    }

    /* H7 fix: same improved URI parsing as heartbeat. */
    const char *host_start = hub_addr;
    if (strncmp(hub_addr, "coap://", 7) == 0) host_start += 7;
    char host[64] = {0};
    uint16_t port = 5684;

    const char *port_sep = strrchr(host_start, ':');
    if (port_sep && port_sep != strchr(host_start, ':') &&
        host_start[0] != '[') {
        port_sep = NULL;
    }
    if (port_sep) {
        size_t hlen = (size_t)(port_sep - host_start);
        if (hlen < sizeof(host)) memcpy(host, host_start, hlen);
        char *end = NULL;
        unsigned long p = strtoul(port_sep + 1, &end, 10);
        if (end != port_sep + 1 && p > 0 && p <= 65535)
            port = (uint16_t)p;
    } else {
        strncpy(host, host_start, sizeof(host) - 1);
    }

    uint8_t rx_buf[RX_BUF_SIZE];
    size_t rx_len = 0;

    tw_err_t err = s_proto->send(s_proto, host, port,
                                  CONFIG_TW_SENSOR_PUSH_PATH,
                                  tx_buf, stream.bytes_written,
                                  rx_buf, sizeof(rx_buf), &rx_len,
                                  TELEMETRY_TIMEOUT_MS);
    if (err != TW_OK) {
        PAL_LOGW(TAG, "telemetry send failed: %d", err);
        return err;
    }

    PAL_LOGD(TAG, "telemetry pushed %d readings", (int)msg.readings_count);

    /* Decode TelemetryReply for adaptive interval. */
    if (rx_len > 0) {
        tw_TelemetryReply reply = tw_TelemetryReply_init_zero;
        pb_istream_t rstream = pb_istream_from_buffer(rx_buf, rx_len);
        if (!pb_decode(&rstream, tw_TelemetryReply_fields, &reply)) {
            /* L8 fix: log decode failure instead of silently ignoring. */
            PAL_LOGW(TAG, "TelemetryReply decode failed: %s",
                     PB_GET_ERROR(&rstream));
        } else if (reply.next_interval_s > 0) {
            /* M8 fix: clamp hub-advised interval to [1, 3600] seconds. */
            uint32_t clamped = reply.next_interval_s;
            if (clamped > 3600) clamped = 3600;
            s_interval_s = clamped;
            PAL_LOGI(TAG, "hub adjusted telemetry interval to %us",
                     (unsigned)s_interval_s);
        }
    }

    return TW_OK;
#else
    PAL_LOGD(TAG, "telemetry push requires protobuf");
    return TW_ERR_NOT_READY;
#endif
}

bool svc_telemetry_is_due(void)
{
    return s_initialized && pal_uptime_ms() >= s_next_push_ms;
}

void svc_telemetry_schedule_next(void)
{
    s_next_push_ms = pal_uptime_ms() + (uint64_t)s_interval_s * 1000;
}

#else /* !CONFIG_TW_SENSOR_PUSH_DEDICATED */

void svc_telemetry_init(tw_protocol_t *proto) { (void)proto; }
tw_err_t svc_telemetry_send(void) { return TW_OK; }
bool svc_telemetry_is_due(void) { return false; }
void svc_telemetry_schedule_next(void) { }

#endif
