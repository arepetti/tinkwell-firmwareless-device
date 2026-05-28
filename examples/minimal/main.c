/*
 * Minimal TW Device SDK example.
 *
 * One sensor (temperature), one LED, one CoAP resource.
 * This is the absolute bare minimum to get a device running.
 *
 * SPDX-License-Identifier: MIT
 */

#include <tw_device.h>
#include <tw_sensor.h>
#include <tw_led.h>
#include <pal_gpio.h>
#include <stdlib.h>

#define PIN_LED 1

static tw_led_t led;

static tw_err_t fake_read_temp(int32_t *out, void *ctx)
{
    (void)ctx;
    const char *env = getenv("TW_FAKE_TEMP");
    *out = env ? (int32_t)atoi(env) : 215;
    return TW_OK;
}

static tw_err_t on_get_temperature(tw_msg_request_t *req,
                                   tw_msg_response_t *resp)
{
    (void)req;
    int32_t val = 0;
    tw_sensor_read_int("temperature", &val);
    return tw_msg_respond_i32(resp, val);
}

static tw_err_t my_init(const tw_device_config_t *dev)
{
    (void)dev;
    tw_led_create(&led, PIN_LED);
    tw_sensor_register(&(tw_sensor_config_t){
        .name = "temperature", .read = fake_read_temp,
    });
    return TW_OK;
}

static tw_err_t my_tick(const tw_device_config_t *dev)
{
    (void)dev;
    tw_led_set_pattern(&led, TW_LED_BLINK_SLOW);
    tw_led_tick(&led);
    return TW_OK;
}

static tw_msg_resource_t resources[] = {
    { "/tw/sensor/temperature", TW_MSG_GET, on_get_temperature },
    TW_MSG_RESOURCE_END
};

static const tw_device_config_t config = {
    .name                 = "minimal",
    .fw_version           = "0.1.0",
    .vendor_id            = 0x0001,
    .product_id           = 0x0001,
    .vendor_display_name  = "Tinkwell",
    .product_display_name = "Minimal Device",
    .variant              = 0,
    .resources            = resources,
    .on_init              = my_init,
    .on_tick              = my_tick,
    .tick_interval_ms     = 1000,
};

TW_DEVICE_MAIN(&config)
