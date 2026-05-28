/*
 * pal_ota_esp.c -- OTA via ESP-IDF's esp_ota_ops (A/B partitions).
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_ota.h"
#include "pal_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include <stdlib.h>

#define TAG "ota"

struct pal_ota_handle {
    esp_ota_handle_t h;
    const esp_partition_t *part;
    size_t written;
    size_t expected;
};

tw_err_t pal_ota_begin(pal_ota_handle_t *out, size_t image_size)
{
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) {
        PAL_LOGE(TAG, "no OTA partition found");
        return TW_ERR_NOT_FOUND;
    }

    struct pal_ota_handle *h = calloc(1, sizeof(*h));
    if (!h) return TW_ERR_NOMEM;

    esp_err_t err = esp_ota_begin(part, image_size, &h->h);
    if (err != ESP_OK) {
        PAL_LOGE(TAG, "esp_ota_begin: 0x%x", err);
        free(h);
        return TW_ERR_IO;
    }

    h->part     = part;
    h->expected = image_size;
    *out = h;

    PAL_LOGI(TAG, "OTA begin on partition '%s', %zu bytes",
             part->label, image_size);
    return TW_OK;
}

tw_err_t pal_ota_write(pal_ota_handle_t h, const void *data, size_t len)
{
    esp_err_t err = esp_ota_write(h->h, data, len);
    if (err != ESP_OK) return TW_ERR_IO;
    h->written += len;
    return TW_OK;
}

tw_err_t pal_ota_finish(pal_ota_handle_t h)
{
    esp_err_t err = esp_ota_end(h->h);
    if (err != ESP_OK) {
        PAL_LOGE(TAG, "esp_ota_end: 0x%x", err);
        free(h);
        return TW_ERR_IO;
    }
    PAL_LOGI(TAG, "OTA finish: %zu / %zu bytes", h->written, h->expected);
    free(h);
    return TW_OK;
}

tw_err_t pal_ota_abort(pal_ota_handle_t h)
{
    esp_ota_abort(h->h);
    free(h);
    PAL_LOGW(TAG, "OTA aborted");
    return TW_OK;
}

tw_err_t pal_ota_set_boot_partition(void)
{
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) return TW_ERR_NOT_FOUND;
    return esp_ota_set_boot_partition(part) == ESP_OK
               ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_ota_mark_valid(void)
{
    return esp_ota_mark_app_valid_cancel_rollback() == ESP_OK
               ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_ota_rollback(void)
{
    return esp_ota_mark_app_invalid_rollback_and_reboot() == ESP_OK
               ? TW_OK : TW_ERR_IO;
}

bool pal_ota_is_pending_verify(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK)
        return false;
    return state == ESP_OTA_IMG_PENDING_VERIFY;
}
