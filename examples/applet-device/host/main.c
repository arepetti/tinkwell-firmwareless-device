/*
 * main.c -- Native POSIX entry point for the applet-device example.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_device.h"
#include "wasm_runtime.h"

static tw_msg_resource_t resources[] = {
    { "/tw/sensor/temperature", TW_MSG_GET,  wasm_on_coap },
    { "/tw/applet/status",      TW_MSG_GET,  wasm_on_applet_status },
    TW_MSG_RESOURCE_END
};

static const tw_device_config_t config = {
    .name                 = "applet-device",
    .fw_version           = "0.1.0",
    .vendor_id            = 0x0001,
    .product_id           = 0x2001,
    .vendor_display_name  = "Tinkwell",
    .product_display_name = "Applet Device",
    .variant              = 0,
    .resources            = resources,
    .on_init              = wasm_runtime_init,
    .on_tick              = wasm_runtime_tick,
    .on_command            = wasm_runtime_on_command,
    .heartbeat_payload    = wasm_runtime_heartbeat,
    .tick_interval_ms     = 1000,
};

TW_DEVICE_MAIN(&config)
