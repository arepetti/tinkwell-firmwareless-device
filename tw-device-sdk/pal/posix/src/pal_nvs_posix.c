/*
 * pal_nvs_posix.c -- Key-value storage backed by a JSON file.
 *
 * Stores data in $HOME/.tw-device/nvs.json.  The format is simple
 * enough to parse without a JSON library (flat key:value lines).
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_nvs.h"
#include "pal_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define TAG      "nvs"
#define MAX_KEYS 64
#define MAX_VLEN 256

static struct {
    char key[64];
    char val[MAX_VLEN];
} store[MAX_KEYS];

static int  count;
static char nvs_path[512];

static int find(const char *key)
{
    for (int i = 0; i < count; i++)
        if (strcmp(store[i].key, key) == 0)
            return i;
    return -1;
}

static void load(void)
{
    FILE *f = fopen(nvs_path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f) && count < MAX_KEYS) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        /* trim trailing newline */
        size_t vl = strlen(val);
        if (vl > 0 && val[vl - 1] == '\n') val[vl - 1] = '\0';
        snprintf(store[count].key, sizeof(store[count].key), "%s", line);
        snprintf(store[count].val, sizeof(store[count].val), "%s", val);
        count++;
    }
    fclose(f);
}

static void save(void)
{
    int fd = open(nvs_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return;
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); return; }
    for (int i = 0; i < count; i++)
        fprintf(f, "%s=%s\n", store[i].key, store[i].val);
    fclose(f);
}

tw_err_t pal_nvs_init(void)
{
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(nvs_path, sizeof(nvs_path), "%s/.tw-device", home);
    mkdir(nvs_path, 0700);
    snprintf(nvs_path, sizeof(nvs_path), "%s/.tw-device/nvs.dat", home);

    count = 0;
    load();

    chmod(nvs_path, 0600);

    PAL_LOGI(TAG, "NVS loaded %d keys from %s", count, nvs_path);
    return TW_OK;
}

tw_err_t pal_nvs_get_i32(const char *key, int32_t *out)
{
    int i = find(key);
    if (i < 0) return TW_ERR_NOT_FOUND;
    *out = (int32_t)atoi(store[i].val);
    return TW_OK;
}

tw_err_t pal_nvs_set_i32(const char *key, int32_t value)
{
    int i = find(key);
    if (i < 0) {
        if (count >= MAX_KEYS) return TW_ERR_OVERFLOW;
        i = count++;
        snprintf(store[i].key, sizeof(store[i].key), "%s", key);
    }
    snprintf(store[i].val, sizeof(store[i].val), "%d", (int)value);
    return TW_OK;
}

tw_err_t pal_nvs_get_str(const char *key, char *buf, size_t buf_size)
{
    int i = find(key);
    if (i < 0) return TW_ERR_NOT_FOUND;
    snprintf(buf, buf_size, "%s", store[i].val);
    return TW_OK;
}

tw_err_t pal_nvs_set_str(const char *key, const char *value)
{
    int i = find(key);
    if (i < 0) {
        if (count >= MAX_KEYS) return TW_ERR_OVERFLOW;
        i = count++;
        snprintf(store[i].key, sizeof(store[i].key), "%s", key);
    }
    snprintf(store[i].val, sizeof(store[i].val), "%s", value);
    return TW_OK;
}

tw_err_t pal_nvs_get_blob(const char *key, void *buf, size_t *len)
{
    int i = find(key);
    if (i < 0) return TW_ERR_NOT_FOUND;

    /* Stored as hex string. Decode back to binary. */
    const char *hex = store[i].val;
    size_t hex_len = strlen(hex);
    size_t bin_len = hex_len / 2;
    if (bin_len > *len) bin_len = *len;

    uint8_t *out = (uint8_t *)buf;
    for (size_t j = 0; j < bin_len; j++) {
        unsigned int b;
        if (sscanf(hex + j * 2, "%2x", &b) != 1) break;
        out[j] = (uint8_t)b;
    }
    *len = bin_len;
    return TW_OK;
}

tw_err_t pal_nvs_set_blob(const char *key, const void *buf, size_t len)
{
    if (len * 2 >= MAX_VLEN) return TW_ERR_OVERFLOW;

    int i = find(key);
    if (i < 0) {
        if (count >= MAX_KEYS) return TW_ERR_OVERFLOW;
        i = count++;
        snprintf(store[i].key, sizeof(store[i].key), "%s", key);
    }

    /* Encode binary as hex string. */
    const uint8_t *src = (const uint8_t *)buf;
    for (size_t j = 0; j < len; j++)
        snprintf(store[i].val + j * 2, 3, "%02x", src[j]);
    store[i].val[len * 2] = '\0';
    return TW_OK;
}

tw_err_t pal_nvs_erase(const char *key)
{
    int i = find(key);
    if (i < 0) return TW_ERR_NOT_FOUND;
    if (i < count - 1)
        store[i] = store[count - 1];
    count--;
    return TW_OK;
}

tw_err_t pal_nvs_commit(void)
{
    save();
    return TW_OK;
}
