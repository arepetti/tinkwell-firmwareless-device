/*
 * tw_hub.h -- Hub heartbeat/mailbox types and helpers.
 *
 * The device periodically POSTs a heartbeat to the hub (CoAP client).
 * The hub responds with any queued commands.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_HUB_H
#define TW_HUB_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Persists the hub CoAP endpoint used for heartbeat and outbound requests.
 *
 * The string is typically a CoAP URI as used with RFC 7252 clients (e.g. `coap://host:port`
 * or `coaps://...` when DTLS is enabled). Exact parsing is implementation-defined but must
 * match what the PAL/network stack expects for dialing the hub.
 *
 * @param coap_uri NUL-terminated URI or host specifier for the hub; semantics match the SDK PAL.
 * @retval TW_OK if the address was stored.
 * @retval TW_ERR_INVAL if @a coap_uri is invalid.
 * @retval Other ::tw_err_t on persistent storage or transport configuration failure.
 */
tw_err_t tw_hub_set_address(const char *coap_uri);

/**
 * @brief Copies the currently configured hub address from NVS into @a buf.
 * @param buf Output buffer for a NUL-terminated string.
 * @param buf_size Size of @a buf including space for the terminator.
 * @retval TW_OK if a string was written (possibly empty if unset, per implementation).
 * @retval TW_ERR_OVERFLOW if the stored value does not fit.
 */
tw_err_t tw_hub_get_address(char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* TW_HUB_H */
