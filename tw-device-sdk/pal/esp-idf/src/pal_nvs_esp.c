/*
 * pal_nvs_esp.c -- Key-value storage via ESP-IDF NVS.
 *
 * When CONFIG_TW_NVS_ENCRYPT is enabled, uses nvs_flash_secure_init()
 * with an encrypted key partition ("nvs_key").  The keys partition must
 * be present in the partition table and flash encryption must be active.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_nvs.h"
#include "pal_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#ifdef CONFIG_TW_NVS_ENCRYPT
#include "nvs_sec_provider.h"
#include "esp_partition.h"
#endif

#define TAG       "nvs"
#define NAMESPACE "tw_device"

static nvs_handle_t handle;

tw_err_t pal_nvs_init(void)
{
    esp_err_t err;

#ifdef CONFIG_TW_NVS_ENCRYPT
    const esp_partition_t *key_part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS, "nvs_key");

    if (key_part) {
        nvs_sec_cfg_t cfg;
        err = nvs_flash_read_security_cfg(key_part, &cfg);
        if (err == ESP_ERR_NVS_KEYS_NOT_INITIALIZED) {
            err = nvs_flash_generate_keys(key_part, &cfg);
        }
        if (err == ESP_OK) {
            err = nvs_flash_secure_init(&cfg);
        } else {
            PAL_LOGW(TAG, "NVS encryption key setup failed (%d), falling back", err);
            err = nvs_flash_init();
        }
    } else {
        PAL_LOGW(TAG, "nvs_key partition not found, using unencrypted NVS");
        err = nvs_flash_init();
    }
#else
    err = nvs_flash_init();
#endif

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return TW_ERR_IO;

    err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return TW_ERR_IO;

    PAL_LOGI(TAG, "NVS initialised (namespace=%s%s)", NAMESPACE,
#ifdef CONFIG_TW_NVS_ENCRYPT
             ", encrypted"
#else
             ""
#endif
    );
    return TW_OK;
}

tw_err_t pal_nvs_get_i32(const char *key, int32_t *out)
{
    return nvs_get_i32(handle, key, out) == ESP_OK
               ? TW_OK : TW_ERR_NOT_FOUND;
}

tw_err_t pal_nvs_set_i32(const char *key, int32_t value)
{
    return nvs_set_i32(handle, key, value) == ESP_OK
               ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_nvs_get_str(const char *key, char *buf, size_t buf_size)
{
    return nvs_get_str(handle, key, buf, &buf_size) == ESP_OK
               ? TW_OK : TW_ERR_NOT_FOUND;
}

tw_err_t pal_nvs_set_str(const char *key, const char *value)
{
    return nvs_set_str(handle, key, value) == ESP_OK
               ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_nvs_get_blob(const char *key, void *buf, size_t *len)
{
    return nvs_get_blob(handle, key, buf, len) == ESP_OK
               ? TW_OK : TW_ERR_NOT_FOUND;
}

tw_err_t pal_nvs_set_blob(const char *key, const void *buf, size_t len)
{
    return nvs_set_blob(handle, key, buf, len) == ESP_OK
               ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_nvs_erase(const char *key)
{
    return nvs_erase_key(handle, key) == ESP_OK
               ? TW_OK : TW_ERR_NOT_FOUND;
}

tw_err_t pal_nvs_commit(void)
{
    return nvs_commit(handle) == ESP_OK ? TW_OK : TW_ERR_IO;
}
