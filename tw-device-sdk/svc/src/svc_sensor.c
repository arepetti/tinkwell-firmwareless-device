/*
 * svc_sensor.c -- Sensor registry and polling service.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_sensor.h"
#include "tw_lock.h"
#include "pal_log.h"

#include <string.h>

#define TAG "sensor"
#define MAX_SENSORS 8

static tw_lock_t s_lock;

static struct {
    tw_sensor_config_t cfg;
    int32_t            last_value;
    bool               registered;
} sensors[MAX_SENSORS];

/** Registers a named sensor in the next free slot; name must be unique in practice (lookup is linear). */
tw_err_t tw_sensor_register(const tw_sensor_config_t *cfg)
{
    tw_lock_acquire(s_lock);
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (!sensors[i].registered) {
            sensors[i].cfg        = *cfg;
            sensors[i].last_value = 0;
            sensors[i].registered = true;
            tw_lock_release(s_lock);
            PAL_LOGI(TAG, "registered sensor '%s'", cfg->name);
            return TW_OK;
        }
    }
    tw_lock_release(s_lock);
    return TW_ERR_OVERFLOW;
}

/** On-demand read: invokes the driver callback under lock and refreshes the cached last value on success. */
tw_err_t tw_sensor_read_int(const char *name, int32_t *out_value)
{
    if (!name || !out_value) return TW_ERR_INVAL;
    tw_lock_acquire(s_lock);
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (sensors[i].registered &&
            strcmp(sensors[i].cfg.name, name) == 0) {
            int32_t val = 0;
            tw_err_t err = TW_OK;
            if (sensors[i].cfg.read)
                err = sensors[i].cfg.read(&val, sensors[i].cfg.ctx);
            if (tw_ok(err)) {
                sensors[i].last_value = val;
                *out_value = val;
            }
            tw_lock_release(s_lock);
            return err;
        }
    }
    tw_lock_release(s_lock);
    return TW_ERR_NOT_FOUND;
}

/** Cheap presence check for resource tables without forcing a read. */
bool tw_sensor_available(const char *name)
{
    for (int i = 0; i < MAX_SENSORS; i++)
        if (sensors[i].registered &&
            strcmp(sensors[i].cfg.name, name) == 0)
            return true;
    return false;
}

/** Creates the registry mutex used by register/read/poll paths. */
void svc_sensor_init(void)
{
    tw_lock_init(&s_lock);
}

/** Returns the number of registered sensors (for heartbeat sensor push). */
int svc_sensor_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_SENSORS; i++)
        if (sensors[i].registered) count++;
    return count;
}

/** Retrieves sensor name and cached value by slot index (for heartbeat sensor push). */
tw_err_t svc_sensor_get_by_index(int idx, const char **name, int32_t *value)
{
    if (idx < 0 || idx >= MAX_SENSORS) return TW_ERR_INVAL;
    int slot = 0;
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (!sensors[i].registered) continue;
        if (slot == idx) {
            if (name)  *name  = sensors[i].cfg.name;
            if (value) *value = sensors[i].last_value;
            return TW_OK;
        }
        slot++;
    }
    return TW_ERR_NOT_FOUND;
}

/** Periodic refresh: updates last_value for all registered sensors that expose a read hook. */
void svc_sensor_poll(void)
{
    tw_lock_acquire(s_lock);
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (!sensors[i].registered || !sensors[i].cfg.read) continue;
        int32_t val;
        if (tw_ok(sensors[i].cfg.read(&val, sensors[i].cfg.ctx)))
            sensors[i].last_value = val;
    }
    tw_lock_release(s_lock);
}
