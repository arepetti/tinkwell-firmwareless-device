/*
 * mock_pal_ota.c
 * SPDX-License-Identifier: MIT
 */

#include "pal_ota.h"
#include "mock_pal.h"
#include <stdlib.h>
#include <string.h>

struct pal_ota_handle {
    uint8_t *buf;
    size_t   written;
    size_t   expected;
};

static bool s_pending_verify;

tw_err_t pal_ota_begin(pal_ota_handle_t *out, size_t image_size)
{
    mock_record("pal_ota_begin", NULL);
    struct pal_ota_handle *h = calloc(1, sizeof(*h));
    if (!h) return TW_ERR_NOMEM;
    h->buf = calloc(1, image_size);
    if (!h->buf) { free(h); return TW_ERR_NOMEM; }
    h->expected = image_size;
    *out = h;
    return TW_OK;
}

tw_err_t pal_ota_write(pal_ota_handle_t h, const void *data, size_t len)
{
    mock_record("pal_ota_write", NULL);
    if (h->written + len > h->expected) return TW_ERR_OVERFLOW;
    memcpy(h->buf + h->written, data, len);
    h->written += len;
    return TW_OK;
}

tw_err_t pal_ota_finish(pal_ota_handle_t h)
{
    mock_record("pal_ota_finish", NULL);
    free(h->buf);
    free(h);
    return TW_OK;
}

tw_err_t pal_ota_abort(pal_ota_handle_t h)
{
    mock_record("pal_ota_abort", NULL);
    free(h->buf);
    free(h);
    return TW_OK;
}

tw_err_t pal_ota_set_boot_partition(void) { return TW_OK; }
tw_err_t pal_ota_mark_valid(void)         { s_pending_verify = false; return TW_OK; }
tw_err_t pal_ota_rollback(void)           { return TW_OK; }
bool     pal_ota_is_pending_verify(void)  { return s_pending_verify; }
