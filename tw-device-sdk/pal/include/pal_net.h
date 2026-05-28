/*
 * pal_net.h -- Socket-level network abstraction.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_NET_H
#define PAL_NET_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque socket handle (integer file-descriptor style on many platforms). */
typedef int pal_socket_t;

/** @brief Sentinel value indicating no valid socket (compare after open or on error). */
#define PAL_INVALID_SOCKET (-1)

/** @brief Socket protocol family / type requested when opening connections. */
typedef enum {
    PAL_SOCK_DGRAM,   /**< UDP datagram socket. */
    PAL_SOCK_STREAM,  /**< TCP stream socket (if used by future APIs). */
} pal_sock_type_t;

/**
 * @brief Hostname or textual address plus UDP/TCP port for send/receive endpoints.
 *
 * Backend contract: @c host is a NUL-terminated C string of at most 63 characters plus terminator
 * in the @c host[64] buffer (IPv4 dotted-quad, IPv6 textual form, or hostname per resolver support).
 * @c port is in host byte order unless the implementation documents otherwise.
 */
typedef struct {
    char     host[64]; /**< Hostname or IP string buffer (NUL-terminated, max 63 chars + '\\0'). */
    uint16_t port;     /**< UDP/TCP port number (host byte order). */
} pal_addr_t;

/**
 * @brief Initialize the network stack (interfaces, DNS, etc.).
 *
 * Backend contract: Bring the platform network layer to a state where UDP sockets can be
 * created. May be a no-op on always-on stacks.
 *
 * Thread-safety: Call from main or network task during startup; not required to be re-entrant.
 *
 * @retval TW_OK       Stack ready.
 * @retval TW_ERR_IO   Network unavailable or initialization failed.
 */
tw_err_t     pal_net_init(void);

/**
 * @brief Open a UDP socket bound to a local port.
 *
 * Backend contract: Create a datagram socket and bind to @p local_port (0 may mean ephemeral).
 * Return a handle usable with pal_udp_sendto / pal_udp_recvfrom, or @c PAL_INVALID_SOCKET on
 * failure (also check platform convention for tw_err_t if extended error reporting exists).
 *
 * Thread-safety: Serialize socket creation with other PAL net calls if the implementation is not thread-safe.
 *
 * @param local_port UDP port to bind in host byte order; @c 0 often selects an ephemeral port.
 * @return Valid @c pal_socket_t on success, or @c PAL_INVALID_SOCKET on failure.
 */
pal_socket_t pal_udp_open(uint16_t local_port);

/**
 * @brief Send a UDP datagram to a destination address.
 *
 * Backend contract: Transmit @p len bytes from @p buf to @p dest (host + port). Partial sends
 * may return a short count; @c 0 may indicate an error or non-blocking refusal. Implementations
 * should document whether @p host is resolved here or must be numeric.
 *
 * Thread-safety: One socket may not be safe for concurrent sends; use external sync if needed.
 *
 * @param s    UDP socket from pal_udp_open.
 * @param buf  Datagram payload (may be NULL only if @p len is 0).
 * @param len  Payload length in bytes.
 * @param dest Non-NULL destination address and port.
 * @return Number of bytes accepted for transmission, or negative platform-specific error code.
 */
int          pal_udp_sendto(pal_socket_t s, const void *buf, size_t len,
                            const pal_addr_t *dest);

/**
 * @brief Receive a UDP datagram (blocking or timeout-based).
 *
 * Backend contract: Wait up to @p timeout_ms for a datagram. If @p timeout_ms is negative,
 * behavior is implementation-defined (often block indefinitely). On success, write payload into
 * @p buf (up to @p len), fill @p src with sender address if non-NULL, and return byte count.
 * Return @c 0 for an empty datagram, negative values for errors or timeout per implementation.
 *
 * Thread-safety: Do not recv concurrently on the same socket from multiple tasks without locking.
 *
 * @param s          UDP socket from pal_udp_open.
 * @param buf        Buffer for incoming payload.
 * @param len        Maximum bytes to store in @p buf.
 * @param src        Optional out: filled with source address/port of the datagram.
 * @param timeout_ms Wait time in milliseconds; @c 0 may poll; negative may mean infinite wait.
 * @return Number of bytes received, @c 0 if applicable, or negative error/timeout indicator.
 */
int          pal_udp_recvfrom(pal_socket_t s, void *buf, size_t len,
                              pal_addr_t *src, int timeout_ms);

/**
 * @brief Close a socket and release OS resources.
 *
 * Backend contract: Invalidate @p s for future operations. Safe to call on @c PAL_INVALID_SOCKET
 * if the implementation allows; otherwise document.
 *
 * Thread-safety: After close, no other thread should use @p s.
 *
 * @param s Socket to close.
 */
void         pal_socket_close(pal_socket_t s);

/**
 * @brief Tear down network resources allocated by pal_net_init.
 *
 * Backend contract: Reverse pal_net_init; close lingering state. Call before shutdown.
 *
 * Thread-safety: Call when no other task is using PAL net sockets.
 */
void         pal_net_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_NET_H */
