/*
 * mock_pal_nvs.c -- In-memory NVS mock.
 * SPDX-License-Identifier: MIT
 */

#include "pal_nvs.h"
#include "mock_pal.h"
#include <string.h>
#include <stdlib.h>

#define MAX_KEYS 64

typedef struct {
    char    key[32];
    uint8_t data[256];
    size_t  len;
    bool    is_str;
    bool    is_i32;
    int32_t i32_val;
} nvs_entry_t;

static nvs_entry_t entries[MAX_KEYS];
static size_t      entry_count;

static nvs_entry_t *find(const char *key)
{
    for (size_t i = 0; i < entry_count; i++)
        if (strcmp(entries[i].key, key) == 0) return &entries[i];
    return NULL;
}

tw_err_t pal_nvs_init(void)
{
    mock_record("pal_nvs_init", NULL);
    entry_count = 0;
    return TW_OK;
}

tw_err_t pal_nvs_get_i32(const char *key, int32_t *out)
{
    nvs_entry_t *e = find(key);
    if (!e || !e->is_i32) return TW_ERR_NOT_FOUND;
    *out = e->i32_val;
    return TW_OK;
}

tw_err_t pal_nvs_set_i32(const char *key, int32_t value)
{
    nvs_entry_t *e = find(key);
    if (!e) {
        if (entry_count >= MAX_KEYS) return TW_ERR_OVERFLOW;
        e = &entries[entry_count++];
        strncpy(e->key, key, sizeof(e->key) - 1);
    }
    e->is_i32   = true;
    e->i32_val  = value;
    return TW_OK;
}

tw_err_t pal_nvs_get_str(const char *key, char *buf, size_t buf_size)
{
    nvs_entry_t *e = find(key);
    if (!e || !e->is_str) return TW_ERR_NOT_FOUND;
    strncpy(buf, (const char *)e->data, buf_size - 1);
    buf[buf_size - 1] = '\0';
    return TW_OK;
}

tw_err_t pal_nvs_set_str(const char *key, const char *value)
{
    nvs_entry_t *e = find(key);
    if (!e) {
        if (entry_count >= MAX_KEYS) return TW_ERR_OVERFLOW;
        e = &entries[entry_count++];
        strncpy(e->key, key, sizeof(e->key) - 1);
    }
    e->is_str = true;
    strncpy((char *)e->data, value, sizeof(e->data) - 1);
    e->len = strlen(value);
    return TW_OK;
}

tw_err_t pal_nvs_get_blob(const char *key, void *buf, size_t *len)
{
    nvs_entry_t *e = find(key);
    if (!e) return TW_ERR_NOT_FOUND;
    size_t n = *len < e->len ? *len : e->len;
    memcpy(buf, e->data, n);
    *len = n;
    return TW_OK;
}

tw_err_t pal_nvs_set_blob(const char *key, const void *buf, size_t len)
{
    nvs_entry_t *e = find(key);
    if (!e) {
        if (entry_count >= MAX_KEYS) return TW_ERR_OVERFLOW;
        e = &entries[entry_count++];
        strncpy(e->key, key, sizeof(e->key) - 1);
    }
    if (len > sizeof(e->data)) return TW_ERR_OVERFLOW;
    memcpy(e->data, buf, len);
    e->len = len;
    return TW_OK;
}

tw_err_t pal_nvs_erase(const char *key)
{
    nvs_entry_t *e = find(key);
    if (!e) return TW_ERR_NOT_FOUND;
    memset(e, 0, sizeof(*e));
    return TW_OK;
}

tw_err_t pal_nvs_commit(void) { return TW_OK; }
