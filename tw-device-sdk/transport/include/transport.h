/*
 * transport.h -- Unified transport interface (compile-time selected).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TW_TRANSPORT_WIFI,
    TW_TRANSPORT_ETHERNET,
    TW_TRANSPORT_THREAD,
    TW_TRANSPORT_BLE,
} tw_transport_type_t;

/* Initialise the selected transport(s) and wait for connectivity. */
tw_err_t tw_transport_init(void);

/* Shut down the transport. */
void     tw_transport_deinit(void);

/* Return true if network connectivity is available. */
bool     tw_transport_connected(void);

/* Name of the active transport for logging. */
const char *tw_transport_name(void);

#ifdef __cplusplus
}
#endif

#endif /* TRANSPORT_H */
