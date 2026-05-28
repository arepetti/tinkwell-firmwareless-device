/*
 * pal_flash_esp.c -- Flash storage via ESP-IDF partitions.
 *
 * Supports mmap for WASM XIP (execute-in-place from flash).
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_flash.h"
#include "pal_log.h"
#include "esp_partition.h"

#include <string.h>

#define TAG "flash"

static const esp_partition_t *find_part(const char *label)
{
    return esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        label);
}

tw_err_t pal_flash_init(const char *label)
{
    const esp_partition_t *p = find_part(label);
    if (!p) {
        PAL_LOGW(TAG, "partition '%s' not found", label);
        return TW_ERR_NOT_FOUND;
    }
    PAL_LOGI(TAG, "partition '%s': offset=0x%lx size=%lu",
             label, (unsigned long)p->address, (unsigned long)p->size);
    return TW_OK;
}

tw_err_t pal_flash_erase(const char *label)
{
    const esp_partition_t *p = find_part(label);
    if (!p) return TW_ERR_NOT_FOUND;
    return esp_partition_erase_range(p, 0, p->size) == ESP_OK
               ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_flash_write(const char *label, size_t offset,
                         const void *data, size_t len)
{
    const esp_partition_t *p = find_part(label);
    if (!p) return TW_ERR_NOT_FOUND;
    return esp_partition_write(p, offset, data, len) == ESP_OK
               ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_flash_read(const char *label, size_t offset,
                        void *buf, size_t len)
{
    const esp_partition_t *p = find_part(label);
    if (!p) return TW_ERR_NOT_FOUND;
    return esp_partition_read(p, offset, buf, len) == ESP_OK
               ? TW_OK : TW_ERR_IO;
}

size_t pal_flash_size(const char *label)
{
    const esp_partition_t *p = find_part(label);
    return p ? p->size : 0;
}

/* mmap state (single partition at a time). */
static esp_partition_mmap_handle_t mmap_handle;
static const char *mmap_label;

const void *pal_flash_mmap(const char *label, size_t *out_len)
{
    const esp_partition_t *p = find_part(label);
    if (!p) return NULL;

    const void *ptr = NULL;
    esp_err_t err = esp_partition_mmap(p, 0, p->size,
                                       ESP_PARTITION_MMAP_DATA,
                                       &ptr, &mmap_handle);
    if (err != ESP_OK) return NULL;

    mmap_label = label;
    if (out_len) *out_len = p->size;
    PAL_LOGI(TAG, "mmap '%s' at %p (%lu bytes)",
             label, ptr, (unsigned long)p->size);
    return ptr;
}

void pal_flash_munmap(const char *label)
{
    if (mmap_label && strcmp(mmap_label, label) == 0) {
        esp_partition_munmap(mmap_handle);
        mmap_label = NULL;
    }
}
