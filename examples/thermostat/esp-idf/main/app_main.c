/*
 * app_main.c -- ESP-IDF entry point for the thermostat example.
 *
 * Wires the thermostat application logic into tw_device_run().
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_device.h"
#include "thermostat.h"

static tw_msg_resource_t resources[] = {
    { "/tw/sensor/temperature", TW_MSG_GET,               on_get_temperature },
    { "/tw/sensor/humidity",    TW_MSG_GET,               on_get_humidity },
    { "/tw/mode",               TW_MSG_GET | TW_MSG_PUT,  on_mode },
    { "/tw/relay",              TW_MSG_GET | TW_MSG_PUT,  on_relay },
    { "/tw/status",             TW_MSG_GET,               on_get_status },
    TW_MSG_RESOURCE_END
};

static const tw_device_config_t config = {
    .name                 = "thermostat",
    .fw_version           = "0.1.0",
    .vendor_id            = 0x0001,
    .product_id           = 0x1001,
    .vendor_display_name  = "Tinkwell",
    .product_display_name = "Smart Thermostat",
    .variant              = 0,
    .resources            = resources,
    .on_init              = thermostat_init,
    .on_tick              = thermostat_tick,
    .tick_interval_ms     = 1000,
};

void app_main(void)
{
    tw_device_run(&config);
}
