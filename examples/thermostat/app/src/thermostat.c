/*
 * thermostat.c -- Thermostat state machine and message resource handlers.
 *
 * Modes:
 *   OFF  -- relay off (unless freeze safety override)
 *   ON   -- relay on  (unless overheat safety override)
 *   AUTO -- relay controlled by hub via PUT /tw/relay
 *
 * Safety:
 *   temp <= FREEZE_THRESHOLD  ->  force relay ON
 *   temp >= OVERHEAT_THRESHOLD -> force relay OFF
 *
 * SPDX-License-Identifier: MIT
 */

#include "thermostat.h"
#include "thermostat_pins.h"

#include "tw_button.h"
#include "tw_led.h"
#include "tw_safety.h"
#include "tw_sensor.h"
#include "tw_config.h"
#include "pal_gpio.h"
#include "pal_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TAG "thermo"

/* Kconfig defaults (overridden by real Kconfig on ESP-IDF). */
#ifndef CONFIG_SAFETY_FREEZE_TEMP_C
#define CONFIG_SAFETY_FREEZE_TEMP_C 30   /* 3.0 C */
#endif
#ifndef CONFIG_SAFETY_MAX_TEMP_C
#define CONFIG_SAFETY_MAX_TEMP_C    400  /* 40.0 C */
#endif

/* ---------------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------------*/

static thermostat_mode_t mode = MODE_AUTO;
static bool relay_state     = false;
static bool relay_requested = false;   /* from hub, used in AUTO mode */

static tw_button_t         button;
static tw_led_t            led_mode;
static tw_led_t            led_relay;
static tw_safety_monitor_t safety;

/* Fake sensor driver for POSIX -- reads TW_FAKE_TEMP env var. */
static tw_err_t read_temperature(int32_t *out, void *ctx)
{
    TW_UNUSED(ctx);
    const char *env = getenv("TW_FAKE_TEMP");
    *out = env ? (int32_t)atoi(env) : 215;
    return TW_OK;
}

static tw_err_t read_humidity(int32_t *out, void *ctx)
{
    TW_UNUSED(ctx);
    const char *env = getenv("TW_FAKE_HUMID");
    *out = env ? (int32_t)atoi(env) : 450;
    return TW_OK;
}

/* ---------------------------------------------------------------------------
 * Callbacks
 * -------------------------------------------------------------------------*/

static void on_button_press(void *ctx)
{
    TW_UNUSED(ctx);
    mode = (thermostat_mode_t)((mode + 1) % 3);
    tw_config_set_i32("mode", (int32_t)mode);
    PAL_LOGI(TAG, "button -> mode=%d", mode);
}

static void on_freeze(tw_safety_event_t event, void *ctx)
{
    TW_UNUSED(event); TW_UNUSED(ctx);
    PAL_LOGW(TAG, "FREEZE override: forcing relay ON");
    relay_state = true;
}

static void on_overheat(tw_safety_event_t event, void *ctx)
{
    TW_UNUSED(event); TW_UNUSED(ctx);
    PAL_LOGW(TAG, "OVERHEAT override: forcing relay OFF");
    relay_state = false;
}

/* ---------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------*/

tw_err_t thermostat_init(const tw_device_config_t *dev)
{
    TW_UNUSED(dev);

    mode = (thermostat_mode_t)tw_config_get_i32("mode", MODE_AUTO);
    PAL_LOGI(TAG, "restored mode=%d from NVS", mode);

    /*
     * Safety thresholds can be customized per-device during provisioning
     * via APP_ keys (e.g. APP_temp_min, APP_temp_max) sent in a
     * ProvisionSetCmd.  Compile-time Kconfig defaults are used when the
     * keys are absent.
     */
    int32_t freeze_c = tw_config_get_i32("APP_temp_min", CONFIG_SAFETY_FREEZE_TEMP_C);
    int32_t max_c    = tw_config_get_i32("APP_temp_max", CONFIG_SAFETY_MAX_TEMP_C);
    PAL_LOGI(TAG, "safety thresholds: freeze=%d overheat=%d", (int)freeze_c, (int)max_c);

    tw_button_create(&button, PIN_BUTTON, on_button_press, NULL);
    tw_led_create(&led_mode, PIN_LED_MODE);
    tw_led_create(&led_relay, PIN_LED_RELAY);

    pal_gpio_init(PIN_RELAY, PAL_GPIO_OUTPUT);

    tw_safety_create(&safety, &(tw_safety_config_t){
        .low_threshold  = freeze_c,
        .high_threshold = max_c,
        .on_low  = on_freeze,
        .on_high = on_overheat,
        .ctx     = NULL,
    });

    tw_sensor_register(&(tw_sensor_config_t){
        .name = "temperature", .read = read_temperature,
    });
    tw_sensor_register(&(tw_sensor_config_t){
        .name = "humidity", .read = read_humidity,
    });

    PAL_LOGI(TAG, "thermostat initialised");
    return TW_OK;
}

tw_err_t thermostat_tick(const tw_device_config_t *dev)
{
    TW_UNUSED(dev);

    tw_button_poll(&button);

    int32_t temp = 0;
    tw_sensor_read_int("temperature", &temp);
    tw_safety_check(&safety, temp);

    if (!tw_safety_is_overriding(&safety)) {
        switch (mode) {
        case MODE_OFF:  relay_state = false;          break;
        case MODE_ON:   relay_state = true;           break;
        case MODE_AUTO: relay_state = relay_requested; break;
        }
    }

    pal_gpio_write(PIN_RELAY, relay_state);

    static const tw_led_pattern_t mode_patterns[] = {
        TW_LED_OFF, TW_LED_SOLID, TW_LED_BLINK_SLOW
    };
    tw_led_set_pattern(&led_mode, mode_patterns[mode]);
    tw_led_set_pattern(&led_relay, relay_state ? TW_LED_SOLID : TW_LED_OFF);
    tw_led_tick(&led_mode);
    tw_led_tick(&led_relay);

    return TW_OK;
}

/* ---------------------------------------------------------------------------
 * Message handlers
 * -------------------------------------------------------------------------*/

tw_err_t on_get_temperature(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    TW_UNUSED(req);
    int32_t val = 0;
    tw_sensor_read_int("temperature", &val);
    return tw_msg_respond_i32(resp, val);
}

tw_err_t on_get_humidity(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    TW_UNUSED(req);
    int32_t val = 0;
    tw_sensor_read_int("humidity", &val);
    return tw_msg_respond_i32(resp, val);
}

tw_err_t on_mode(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    static const char *names[] = { "off", "on", "auto" };

    if (req->method == TW_MSG_GET)
        return tw_msg_respond_text(resp, names[mode]);

    /* PUT: hub can only set "auto". */
    if (req->payload_len >= 4 &&
        memcmp(req->payload, "auto", 4) == 0) {
        mode = MODE_AUTO;
        tw_config_set_i32("mode", MODE_AUTO);
        return tw_msg_respond_empty(resp, TW_MSG_204_CHANGED);
    }

    return tw_msg_respond_empty(resp, TW_MSG_400_BAD_REQ);
}

tw_err_t on_relay(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    if (req->method == TW_MSG_GET)
        return tw_msg_respond_i32(resp, relay_state ? 1 : 0);

    if (mode != MODE_AUTO)
        return tw_msg_respond_empty(resp, TW_MSG_400_BAD_REQ);

    relay_requested = (req->payload_len > 0 && req->payload[0] == '1');
    return tw_msg_respond_empty(resp, TW_MSG_204_CHANGED);
}

tw_err_t on_get_status(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    TW_UNUSED(req);
    char buf[256];
    static const char *names[] = { "off", "on", "auto" };
    int n = snprintf(buf, sizeof(buf),
        "mode=%s relay=%d temp=%d safety=%s",
        names[mode], relay_state ? 1 : 0,
        (int)temp,
        tw_safety_is_overriding(&safety) ? "override" : "normal");
    return tw_msg_respond_buf(resp, buf, (size_t)n);
}
