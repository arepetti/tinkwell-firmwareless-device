/*
 * wasm_host_api.c -- Native host functions exposed to the WASM sandbox.
 *
 * These will be registered with WAMR via wasm_runtime_register_natives().
 * For now they're plain C functions; the WAMR binding glue will wrap
 * them with address translation from WASM linear memory.
 *
 * String parameters from WASM are (ptr, len) — NOT NUL-terminated.
 *
 * Memory safety
 * -------------
 * WAMR's native binding translates a single app-addr to a native pointer
 * but does NOT validate that (ptr + len) stays within linear memory.
 * Every function that receives a (ptr, len) pair MUST call
 * VALIDATE_APP_RANGE() before reading the buffer.  When WAMR is
 * integrated this macro expands to wasm_runtime_validate_app_addr();
 * until then it is a stub that always succeeds.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wasm_host_api.h"
#include "tw_sensor.h"
#include "tw_led.h"
#include "tw_config.h"
#include "pal_gpio.h"
#include "pal_log.h"
#include "pal_os.h"

#include <string.h>
#include <stdbool.h>

#define TAG "wasm-host"
#define MAX_STR 128

/*
 * Bounds-checking macro for (ptr, len) string parameters.
 *
 * Once WAMR is integrated, replace this with:
 *
 *   #define VALIDATE_APP_RANGE(ptr, len) \
 *       wasm_runtime_validate_app_addr(get_module_inst(), (uint32_t)(uintptr_t)(ptr), (len))
 *
 * The macro returns false if the range is out-of-bounds so callers can
 * early-return a safe default.
 */
#define VALIDATE_APP_RANGE(ptr, len) validate_range((ptr), (len))

static bool validate_range(const void *ptr, uint32_t len)
{
    if (!ptr && len > 0)
        return false;

    /*
     * TODO: when WAMR is integrated, call
     *   wasm_runtime_validate_app_addr(module_inst, app_offset, len)
     * to verify (app_offset .. app_offset + len - 1) lies within the
     * module's linear memory.  The current stub trusts the pointer
     * because we have no module instance yet.
     */
    (void)len;
    return ptr != NULL || len == 0;
}

static void make_nul_terminated(char *dst, const char *src, uint32_t len)
{
    if (len >= MAX_STR)
        len = MAX_STR - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/* ---- Sensor access --------------------------------------------------- */

float tw_host_read_sensor_sf(const char *name, uint32_t name_len)
{
    if (!VALIDATE_APP_RANGE(name, name_len))
        return 0.0f;

    char buf[MAX_STR];
    make_nul_terminated(buf, name, name_len);
    int32_t raw = 0;
    tw_sensor_read_int(buf, &raw);
    return (float)raw;
}

int32_t tw_host_read_sensor_sn(const char *name, uint32_t name_len)
{
    if (!VALIDATE_APP_RANGE(name, name_len))
        return 0;

    char buf[MAX_STR];
    make_nul_terminated(buf, name, name_len);
    int32_t val = 0;
    tw_sensor_read_int(buf, &val);
    return val;
}

float tw_host_read_sensor_nf(int32_t sensor_id)
{
    (void)sensor_id;
    return 0.0f; /* TODO: implement sensor ID lookup table */
}

int32_t tw_host_read_sensor_nn(int32_t sensor_id)
{
    (void)sensor_id;
    return 0; /* TODO: implement sensor ID lookup table */
}

/* ---- GPIO ------------------------------------------------------------- */

void tw_host_write_gpio(int32_t pin, int32_t value)
{
    pal_gpio_write(pin, value != 0);
}

int32_t tw_host_read_gpio(int32_t pin)
{
    return pal_gpio_read(pin) ? 1 : 0;
}

/* ---- LED patterns ----------------------------------------------------- */

void tw_host_set_led(int32_t pin, int32_t pattern)
{
    pal_gpio_write(pin, pattern != 0);
}

/* ---- Logging ---------------------------------------------------------- */

void tw_host_log_s(const char *msg, uint32_t msg_len, int32_t level)
{
    if (!VALIDATE_APP_RANGE(msg, msg_len))
        return;

    char buf[MAX_STR];
    make_nul_terminated(buf, msg, msg_len);
    pal_log((pal_log_level_t)level, "applet", "%s", buf);
}

void tw_host_log_n(int32_t msg_id, int32_t level)
{
    pal_log((pal_log_level_t)level, "applet", "msg#%d", msg_id);
}

/* ---- NVS configuration ------------------------------------------------ */

int32_t tw_host_config_get_i32_s(const char *key, uint32_t key_len, int32_t def)
{
    if (!VALIDATE_APP_RANGE(key, key_len))
        return def;

    char buf[MAX_STR];
    make_nul_terminated(buf, key, key_len);
    return tw_config_get_i32(buf, def);
}

int32_t tw_host_config_get_i32_n(int32_t key_id, int32_t def)
{
    (void)key_id;
    return def; /* TODO: implement key ID lookup table */
}

void tw_host_config_set_i32_s(const char *key, uint32_t key_len, int32_t val)
{
    if (!VALIDATE_APP_RANGE(key, key_len))
        return;

    char buf[MAX_STR];
    make_nul_terminated(buf, key, key_len);
    tw_config_set_i32(buf, val);
}

void tw_host_config_set_i32_n(int32_t key_id, int32_t val)
{
    (void)key_id;
    (void)val; /* TODO: implement key ID lookup table */
}

/* ---- NVS configuration (f32) ------------------------------------------ */

float tw_host_config_get_f32_s(const char *key, uint32_t key_len, float def)
{
    if (!VALIDATE_APP_RANGE(key, key_len))
        return def;

    char buf[MAX_STR];
    make_nul_terminated(buf, key, key_len);
    int32_t raw = tw_config_get_i32(buf, (int32_t)def);
    return (float)raw;
}

float tw_host_config_get_f32_n(int32_t key_id, float def)
{
    (void)key_id;
    return def; /* TODO: implement key ID lookup table */
}

void tw_host_config_set_f32_s(const char *key, uint32_t key_len, float val)
{
    if (!VALIDATE_APP_RANGE(key, key_len))
        return;

    char buf[MAX_STR];
    make_nul_terminated(buf, key, key_len);
    tw_config_set_i32(buf, (int32_t)val);
}

void tw_host_config_set_f32_n(int32_t key_id, float val)
{
    (void)key_id;
    (void)val; /* TODO: implement key ID lookup table */
}

/* ---- Time ------------------------------------------------------------- */

int64_t tw_host_uptime_ms(void)
{
    return (int64_t)pal_uptime_ms();
}

/* ---- State machine callback ------------------------------------------- */

void sm_on_transition(int32_t machine_id, int32_t from_id, int32_t to_id)
{
    pal_log(PAL_LOG_INFO, TAG, "sm[%d] transition %d -> %d",
            machine_id, from_id, to_id);
}
