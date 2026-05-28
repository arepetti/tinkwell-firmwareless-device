/*
 * mock_pal_net.c
 * SPDX-License-Identifier: MIT
 */

#include "pal_net.h"
#include "mock_pal.h"

static uint8_t rx_buf[1024];
static size_t  rx_len;

void mock_net_set_rx(const void *data, size_t len)
{
    if (len > sizeof(rx_buf)) len = sizeof(rx_buf);
    memcpy(rx_buf, data, len);
    rx_len = len;
}

tw_err_t pal_net_init(void)
{
    mock_record("pal_net_init", NULL);
    return TW_OK;
}

pal_socket_t pal_udp_open(uint16_t local_port)
{
    mock_record("pal_udp_open", NULL);
    (void)local_port;
    return 42;
}

int pal_udp_sendto(pal_socket_t s, const void *buf, size_t len,
                   const pal_addr_t *dest)
{
    mock_record("pal_udp_sendto", NULL);
    (void)s; (void)buf; (void)dest;
    return (int)len;
}

int pal_udp_recvfrom(pal_socket_t s, void *buf, size_t len,
                     pal_addr_t *src, int timeout_ms)
{
    mock_record("pal_udp_recvfrom", NULL);
    (void)s; (void)timeout_ms;
    size_t n = rx_len < len ? rx_len : len;
    if (n > 0) {
        memcpy(buf, rx_buf, n);
        rx_len = 0;
    }
    if (src) {
        strcpy(src->host, "127.0.0.1");
        src->port = 5684;
    }
    return (int)n;
}

void pal_socket_close(pal_socket_t s)
{
    mock_record("pal_socket_close", NULL);
    (void)s;
}

void pal_net_deinit(void)
{
    mock_record("pal_net_deinit", NULL);
}
