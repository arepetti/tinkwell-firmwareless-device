/*
 * pal_net_posix.c -- BSD socket networking.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_net.h"
#include "pal_log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <netdb.h>

#define TAG "net"

tw_err_t pal_net_init(void)
{
    PAL_LOGI(TAG, "POSIX networking ready");
    return TW_OK;
}

pal_socket_t pal_udp_open(uint16_t local_port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        PAL_LOGE(TAG, "socket: %s", strerror(errno));
        return PAL_INVALID_SOCKET;
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in sa = {
        .sin_family      = AF_INET,
        .sin_port        = htons(local_port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        PAL_LOGE(TAG, "bind :%u: %s", local_port, strerror(errno));
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
    inet_pton(AF_INET, dest->host, &sa.sin_addr);

    int n = (int)sendto(s, buf, len, 0,
                        (struct sockaddr *)&sa, sizeof(sa));
    if (n < 0)
        PAL_LOGE(TAG, "sendto: %s", strerror(errno));
    return n;
}

int pal_udp_recvfrom(pal_socket_t s, void *buf, size_t len,
                     pal_addr_t *src, int timeout_ms)
{
    struct pollfd pfd = { .fd = s, .events = POLLIN };
    int rc = poll(&pfd, 1, timeout_ms);
    if (rc <= 0)
        return rc == 0 ? 0 : -1;

    struct sockaddr_in sa;
    socklen_t salen = sizeof(sa);
    int n = (int)recvfrom(s, buf, len, 0,
                          (struct sockaddr *)&sa, &salen);
    if (n > 0 && src) {
        inet_ntop(AF_INET, &sa.sin_addr, src->host, sizeof(src->host));
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
