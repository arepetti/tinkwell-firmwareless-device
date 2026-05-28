/*
 * transport_posix.c -- Stub transport for POSIX (host) builds.
 *
 * On a desktop, networking is always available, so init is a no-op.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_PLATFORM_ESPIDF

#include "transport.h"
#include "pal_log.h"

#define TAG "transport"

/** No-op on host builds: the PAL assumes the desktop already has working IP connectivity. */
tw_err_t tw_transport_init(void)
{
    PAL_LOGI(TAG, "POSIX host -- network assumed available");
    return TW_OK;
}

/** No-op: nothing to tear down for the stub transport. */
void tw_transport_deinit(void)
{
}

/** Always true: the host build does not model link loss in this stub. */
bool tw_transport_connected(void)
{
    return true;
}

/** Human-readable transport label for logs and diagnostics. */
const char *tw_transport_name(void)
{
    return "posix";
}

#endif /* !TW_PLATFORM_ESPIDF */
