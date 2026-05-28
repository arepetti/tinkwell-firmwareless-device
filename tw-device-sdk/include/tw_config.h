/*
 * tw_config.h -- Typed NVS configuration helpers.
 *
 * Thin wrappers over pal_nvs with defaults.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_CONFIG_H
#define TW_CONFIG_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads a signed 32-bit configuration value from non-volatile storage.
 * @param key NUL-terminated key recognized by the PAL NVS layer.
 * @param def Value returned when @a key is absent or cannot be interpreted as int32.
 * @return Stored integer on success; @a def if missing or invalid.
 */
int32_t tw_config_get_i32(const char *key, int32_t def);

/**
 * @brief Writes a signed 32-bit configuration value to non-volatile storage.
 * @param key NUL-terminated key.
 * @param value Value to persist.
 */
void    tw_config_set_i32(const char *key, int32_t value);

/**
 * @brief Reads a string configuration value into @a buf, with fallback to @a def.
 * @param key NUL-terminated key.
 * @param def Default NUL-terminated string when the key is absent.
 * @param buf Output buffer for the stored or default string.
 * @param buf_size Size of @a buf in bytes including space for the NUL terminator.
 * @return Pointer to @a buf containing the effective string; if the stored value is too long, behavior is implementation-defined (typically truncation with NUL).
 */
const char *tw_config_get_str(const char *key, const char *def,
                              char *buf, size_t buf_size);

/**
 * @brief Persists a string configuration value.
 * @param key NUL-terminated key.
 * @param value NUL-terminated string to store (may be empty).
 */
void        tw_config_set_str(const char *key, const char *value);

#ifdef __cplusplus
}
#endif

#endif /* TW_CONFIG_H */
