/*
 * tw_cmd.h -- Hub-pushed command endpoint handlers.
 *
 * The hub delivers commands as individual CoAP POST requests to
 * per-command device endpoints (e.g. /tw/reboot, /tw/set-config).
 * Each handler decodes the protobuf payload via nanopb and acts on it.
 *
 * The exported resource table svc_cmd_resources[] is appended to the
 * merged resource table by svc_device.c.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_CMD_H
#define TW_CMD_H

#include "tw_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resource table for hub-pushed command endpoints.
 *
 * Registered paths (POST):
 *   /tw/reboot        -- orderly device reboot
 *   /tw/set-config    -- apply configuration entries
 *   /tw/ota-available -- signal new firmware availability
 *   /tw/app           -- application-defined command
 *
 * Terminated by TW_MSG_RESOURCE_END.
 */
extern tw_msg_resource_t svc_cmd_resources[];

/**
 * @brief Initializes the command subsystem.
 * @param cfg Device configuration (for on_command callback).
 */
void svc_cmd_init(const struct tw_device_config *cfg);

#ifdef __cplusplus
}
#endif

#endif /* TW_CMD_H */
