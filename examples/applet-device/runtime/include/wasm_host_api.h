/*
 * wasm_host_api.h -- Native functions exported to the WASM sandbox.
 *
 * These are the functions that WASM applets can call to interact with
 * the device hardware via the SDK.
 *
 * String Convention
 * -----------------
 * All string parameters use length-prefixed encoding: (ptr, len) where
 * ptr is an i32 offset into WASM linear memory and len is a u32 byte
 * count. Strings are NOT NUL-terminated.
 *
 * Memory Safety
 * -------------
 * WAMR's native binding translates a single app-addr → native pointer
 * but does NOT validate that the full range [ptr .. ptr+len) lies within
 * linear memory.  Implementations MUST bounds-check every (ptr, len) pair
 * via wasm_runtime_validate_app_addr() before reading the buffer.  Out-of-
 * bounds calls return a safe default (0 / 0.0f) and are silently dropped.
 *
 * Naming Convention
 * -----------------
 * Variant functions use a two-letter suffix _XY where:
 *   X = input type:  s = string (ptr+len), n = numeric i32 ID
 *   Y = return type: f = f32, n = i32
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef WASM_HOST_API_H
#define WASM_HOST_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Sensor access --------------------------------------------------- */

/* String name, f32 result */
float   tw_host_read_sensor_sf(const char *name, uint32_t name_len);
/* String name, i32 result */
int32_t tw_host_read_sensor_sn(const char *name, uint32_t name_len);
/* Numeric ID, f32 result */
float   tw_host_read_sensor_nf(int32_t sensor_id);
/* Numeric ID, i32 result */
int32_t tw_host_read_sensor_nn(int32_t sensor_id);

/* ---- GPIO ------------------------------------------------------------- */

void    tw_host_write_gpio(int32_t pin, int32_t value);
int32_t tw_host_read_gpio(int32_t pin);

/* ---- LED patterns ----------------------------------------------------- */

void tw_host_set_led(int32_t pin, int32_t pattern);

/* ---- Logging ---------------------------------------------------------- */

/* String message variant */
void tw_host_log_s(const char *msg, uint32_t msg_len, int32_t level);
/* Numeric message ID variant (for string-table-based logging) */
void tw_host_log_n(int32_t msg_id, int32_t level);

/* ---- NVS configuration (i32) ------------------------------------------ */

int32_t tw_host_config_get_i32_s(const char *key, uint32_t key_len, int32_t def);
int32_t tw_host_config_get_i32_n(int32_t key_id, int32_t def);
void    tw_host_config_set_i32_s(const char *key, uint32_t key_len, int32_t val);
void    tw_host_config_set_i32_n(int32_t key_id, int32_t val);

/* ---- NVS configuration (f32) ------------------------------------------ */

float   tw_host_config_get_f32_s(const char *key, uint32_t key_len, float def);
float   tw_host_config_get_f32_n(int32_t key_id, float def);
void    tw_host_config_set_f32_s(const char *key, uint32_t key_len, float val);
void    tw_host_config_set_f32_n(int32_t key_id, float val);

/* ---- Time ------------------------------------------------------------- */

/* Monotonic uptime in milliseconds. Wraps pal_uptime_ms(). */
int64_t tw_host_uptime_ms(void);

/* ---- Condition trap --------------------------------------------------- */

/* Called when a compiled state machine precondition or postcondition is
 * violated.  condition_id maps to the manifest entry for diagnostics. */
void tw_trap(uint32_t condition_id);

/* ---- State machine callback ------------------------------------------- */

/* Called on every state transition. machine_id identifies the machine,
 * from_id and to_id are state IDs within that machine. */
void sm_on_transition(int32_t machine_id, int32_t from_id, int32_t to_id);

#ifdef __cplusplus
}
#endif

#endif /* WASM_HOST_API_H */
