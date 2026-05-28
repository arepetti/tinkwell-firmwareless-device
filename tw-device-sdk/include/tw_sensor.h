/*
 * tw_sensor.h -- Sensor reading service.
 *
 * The SDK polls registered sensors periodically.  Application code
 * reads the latest value via tw_sensor_read_int / tw_sensor_read_float.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_SENSOR_H
#define TW_SENSOR_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Driver hook invoked on each poll interval to sample hardware into @a out_value.
 * @param out_value Receives the scaled integer reading (application-defined unit).
 * @param ctx Opaque context from ::tw_sensor_config_t::ctx.
 * @retval TW_OK if @a out_value is valid this cycle.
 * @retval Other ::tw_err_t if the sample should be skipped or treated as stale.
 */
typedef tw_err_t (*tw_sensor_read_fn_t)(int32_t *out_value, void *ctx);

/** @brief Registration parameters for one named logical sensor. */
typedef struct {
    const char         *name;             /**< Unique name used with ::tw_sensor_read_int and ::tw_sensor_available. */
    tw_sensor_read_fn_t read;             /**< Callback that performs the hardware read. */
    void               *ctx;              /**< Opaque pointer passed to @a read. */
    uint32_t            poll_interval_ms; /**< Minimum time between ::read invocations (SDK scheduling). */
} tw_sensor_config_t;

/**
 * @brief Registers a sensor for periodic polling and last-value caching.
 * @param cfg Non-NULL configuration; @a name must remain valid for the lifetime of registration.
 * @retval TW_OK on success.
 * @retval TW_ERR_INVAL if @a cfg or required fields are invalid.
 * @retval TW_ERR_BUSY if the name is already registered.
 */
tw_err_t tw_sensor_register(const tw_sensor_config_t *cfg);

/**
 * @brief Returns the most recent cached integer reading for a registered sensor.
 * @param name Same name passed to ::tw_sensor_register.
 * @param out_value Receives the last successful sample.
 * @retval TW_OK if a value is available.
 * @retval TW_ERR_NOT_FOUND if @a name is unknown.
 */
tw_err_t tw_sensor_read_int(const char *name, int32_t *out_value);

/**
 * @brief Returns whether a sensor has been registered and has had at least one successful sample.
 * @param name Sensor name.
 * @return True if the sensor exists and data may be read; false otherwise.
 */
bool     tw_sensor_available(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* TW_SENSOR_H */
