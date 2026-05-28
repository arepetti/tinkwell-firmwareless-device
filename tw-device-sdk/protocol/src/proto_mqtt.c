/*
 * proto_mqtt.c -- MQTT protocol backend for tw_protocol_t (STUB).
 *
 * This file exists to define the compile-time guard and provide a
 * roadmap of what needs implementing when MQTT support is added.
 *
 * When CONFIG_TW_PROTOCOL_MQTT is enabled, this file generates a
 * compile-time error.  The #ifdef guards throughout the SDK show
 * exactly which code paths need an MQTT implementation.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_msg.h"

#ifdef CONFIG_TW_PROTOCOL_MQTT

#error "MQTT protocol backend is not yet implemented. \
To add MQTT support: \
1. Implement tw_protocol_t vtable (init, poll, deinit, send) using an MQTT client library. \
2. Implement tw_msg_respond_* helpers that serialise responses as MQTT publishes. \
3. Map tw_msg_resource_t subscriptions to MQTT topic filters. \
4. Update svc_heartbeat.c to publish heartbeat via MQTT instead of CoAP POST. \
5. Update svc_device.c to use the MQTT vtable when selected. \
6. Add MQTT-specific Kconfig options (broker URI, client ID, QoS, TLS). \
7. Test with Mosquitto or similar broker."

/*
 * Implementation outline (to be filled in):
 *
 * static tw_err_t mqtt_init(tw_protocol_t *self,
 *                           tw_msg_resource_t *resources, uint16_t port)
 * {
 *     // Connect to MQTT broker (host:port from Kconfig/NVS)
 *     // Subscribe to resource paths as MQTT topics
 *     // Register message callback that dispatches to resource handlers
 * }
 *
 * static void mqtt_poll(tw_protocol_t *self, int timeout_ms)
 * {
 *     // Call mqtt_client_poll() / process incoming messages
 *     // Dispatch received messages to matching resource handlers
 * }
 *
 * static void mqtt_deinit(tw_protocol_t *self)
 * {
 *     // Unsubscribe, disconnect, clean up
 * }
 *
 * static tw_err_t mqtt_send(tw_protocol_t *self,
 *                           const char *host, uint16_t port,
 *                           const char *path,
 *                           const uint8_t *payload, size_t payload_len,
 *                           uint8_t *resp_buf, size_t resp_buf_size,
 *                           size_t *resp_len, int timeout_ms)
 * {
 *     // Publish payload to topic = path
 *     // Optionally subscribe to response topic and wait for reply
 * }
 *
 * static tw_protocol_t s_mqtt_protocol = {
 *     .init   = mqtt_init,
 *     .poll   = mqtt_poll,
 *     .deinit = mqtt_deinit,
 *     .send   = mqtt_send,
 * };
 *
 * tw_protocol_t *tw_protocol_create(void) { return &s_mqtt_protocol; }
 */

#endif /* CONFIG_TW_PROTOCOL_MQTT */
