/*
 * pal_ota_posix.c -- OTA via temporary file swap.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_ota.h"
#include "pal_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "ota"

struct pal_ota_handle {
    FILE   *f;
    size_t  written;
    size_t  expected;
};

static const char *staging_path(void)
{
    static char buf[512];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sizeof(buf), "%s/.tw-device/ota_staging.bin", home);
    return buf;
}

tw_err_t pal_ota_begin(pal_ota_handle_t *out, size_t image_size)
{
    struct pal_ota_handle *h = calloc(1, sizeof(*h));
    if (!h) return TW_ERR_NOMEM;

    h->f = fopen(staging_path(), "wb");
    if (!h->f) { free(h); return TW_ERR_IO; }
    h->expected = image_size;

    PAL_LOGI(TAG, "OTA begin, expecting %zu bytes", image_size);
    *out = h;
    return TW_OK;
}

tw_err_t pal_ota_write(pal_ota_handle_t h, const void *data, size_t len)
{
    size_t n = fwrite(data, 1, len, h->f);
    h->written += n;
    return n == len ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_ota_finish(pal_ota_handle_t h)
{
    fclose(h->f);
    h->f = NULL;
    PAL_LOGI(TAG, "OTA finish, wrote %zu / %zu bytes",
             h->written, h->expected);
    free(h);
    return TW_OK;
}

tw_err_t pal_ota_abort(pal_ota_handle_t h)
{
    if (h->f) fclose(h->f);
    remove(staging_path());
    free(h);
    PAL_LOGW(TAG, "OTA aborted");
    return TW_OK;
}

tw_err_t pal_ota_set_boot_partition(void) { return TW_OK; }
tw_err_t pal_ota_mark_valid(void)         { return TW_OK; }
tw_err_t pal_ota_rollback(void)           { return TW_OK; }
bool     pal_ota_is_pending_verify(void)  { return false; }
