/*
 * thermostat.h -- Thermostat state machine and message handlers.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include "tw_device.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MODE_OFF  = 0,
    MODE_ON   = 1,
    MODE_AUTO = 2,
} thermostat_mode_t;

/* Lifecycle callbacks wired into tw_device_config_t. */
tw_err_t thermostat_init(const tw_device_config_t *dev);
tw_err_t thermostat_tick(const tw_device_config_t *dev);

/* Message resource handlers. */
tw_err_t on_get_temperature(tw_msg_request_t *req, tw_msg_response_t *resp);
tw_err_t on_get_humidity(tw_msg_request_t *req, tw_msg_response_t *resp);
tw_err_t on_mode(tw_msg_request_t *req, tw_msg_response_t *resp);
tw_err_t on_relay(tw_msg_request_t *req, tw_msg_response_t *resp);
tw_err_t on_get_status(tw_msg_request_t *req, tw_msg_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* THERMOSTAT_H */
