/*
 * wasm_runtime.h -- WAMR-based applet runtime for applet-driven devices.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef WASM_RUNTIME_H
#define WASM_RUNTIME_H

#include "tw_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Lifecycle callbacks wired into tw_device_config_t. */
tw_err_t wasm_runtime_init(const tw_device_config_t *dev);
tw_err_t wasm_runtime_tick(const tw_device_config_t *dev);

/* Hub-pushed command handler (receives "app" commands). */
tw_err_t wasm_runtime_on_command(const tw_device_config_t *dev,
                                 const char *command,
                                 const uint8_t *payload, size_t payload_len);

/* Heartbeat payload -- appends applet version info. */
tw_err_t wasm_runtime_heartbeat(uint8_t *buf, size_t buf_size,
                                size_t *out_len);

/* Message handlers. */
tw_err_t wasm_on_coap(tw_msg_request_t *req, tw_msg_response_t *resp);
tw_err_t wasm_on_applet_status(tw_msg_request_t *req,
                               tw_msg_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* WASM_RUNTIME_H */
