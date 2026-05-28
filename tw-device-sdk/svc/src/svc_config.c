/*
 * svc_config.c -- Typed NVS config wrappers.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_config.h"
#include "pal_nvs.h"

/** Reads a persisted int32 or returns the compile-time default when the key is absent. */
int32_t tw_config_get_i32(const char *key, int32_t def)
{
    int32_t v;
    return tw_ok(pal_nvs_get_i32(key, &v)) ? v : def;
}

/** Persists an int32 and commits so a sudden reset does not lose the last write. */
void tw_config_set_i32(const char *key, int32_t value)
{
    pal_nvs_set_i32(key, value);
    pal_nvs_commit();
}

/** Copies a string key into buf on success, otherwise returns def (may be NULL). */
const char *tw_config_get_str(const char *key, const char *def,
                              char *buf, size_t buf_size)
{
    if (tw_ok(pal_nvs_get_str(key, buf, buf_size)))
        return buf;
    return def;
}

/** Stores a string value and commits for crash-safe configuration updates. */
void tw_config_set_str(const char *key, const char *value)
{
    pal_nvs_set_str(key, value);
    pal_nvs_commit();
}
