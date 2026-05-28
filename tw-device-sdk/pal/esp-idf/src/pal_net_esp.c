/*
 * pal_net_esp.c -- Networking via lwIP (works over WiFi, Ethernet, or Thread).
 *
 * ESP-IDF's lwIP provides BSD-like sockets, so the implementation is
 * almost identical to the POSIX backend.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_net.h"
#include "pal_log.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include <string.h>

#define TAG "net"

tw_err_t pal_net_init(void)
{
    PAL_LOGI(TAG, "lwIP networking ready");
    return TW_OK;
}

pal_socket_t pal_udp_open(uint16_t local_port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        PAL_LOGE(TAG, "socket failed: %d", errno);
        return PAL_INVALID_SOCKET;
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family      = AF_INET;
    sa.sin_port        = htons(local_port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        PAL_LOGE(TAG, "bind :%u failed: %d", local_port, errno);
        close(fd);
        return PAL_INVALID_SOCKET;
    }

    PAL_LOGI(TAG, "UDP socket bound to :%u (fd=%d)", local_port, fd);
    return fd;
}

int pal_udp_sendto(pal_socket_t s, const void *buf, size_t len,
                   const pal_addr_t *dest)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(dest->port);
    inet_aton(dest->host, &sa.sin_addr);

    return (int)sendto(s, buf, len, 0,
                       (struct sockaddr *)&sa, sizeof(sa));
}

int pal_udp_recvfrom(pal_socket_t s, void *buf, size_t len,
                     pal_addr_t *src, int timeout_ms)
{
    struct timeval tv = {
        .tv_sec  = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);

    int rc = select(s + 1, &fds, NULL, NULL, &tv);
    if (rc <= 0) return rc == 0 ? 0 : -1;

    struct sockaddr_in sa;
    socklen_t salen = sizeof(sa);
    int n = (int)recvfrom(s, buf, len, 0,
                          (struct sockaddr *)&sa, &salen);
    if (n > 0 && src) {
        inet_ntoa_r(sa.sin_addr, src->host, sizeof(src->host));
        src->port = ntohs(sa.sin_port);
    }
    return n;
}

void pal_socket_close(pal_socket_t s)
{
    if (s != PAL_INVALID_SOCKET)
        close(s);
}

void pal_net_deinit(void)
{
}
