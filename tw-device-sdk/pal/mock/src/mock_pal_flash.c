/*
 * mock_pal_flash.c -- In-memory flash mock.
 * SPDX-License-Identifier: MIT
 */

#include "pal_flash.h"
#include "mock_pal.h"
#include <stdlib.h>
#include <string.h>

#define MOCK_FLASH_SIZE (256 * 1024)

static uint8_t *s_flash;
static size_t   s_size;

tw_err_t pal_flash_init(const char *label)
{
    mock_record("pal_flash_init", label);
    if (!s_flash) {
        s_flash = calloc(1, MOCK_FLASH_SIZE);
        s_size  = MOCK_FLASH_SIZE;
    }
    return s_flash ? TW_OK : TW_ERR_NOMEM;
}

tw_err_t pal_flash_erase(const char *label)
{
    mock_record("pal_flash_erase", label);
    if (s_flash) memset(s_flash, 0xFF, s_size);
    return TW_OK;
}

tw_err_t pal_flash_write(const char *label, size_t offset,
                         const void *data, size_t len)
{
    (void)label;
    if (!s_flash || offset + len > s_size) return TW_ERR_OVERFLOW;
    memcpy(s_flash + offset, data, len);
    return TW_OK;
}

tw_err_t pal_flash_read(const char *label, size_t offset,
                        void *buf, size_t len)
{
    (void)label;
    if (!s_flash || offset + len > s_size) return TW_ERR_OVERFLOW;
    memcpy(buf, s_flash + offset, len);
    return TW_OK;
}

size_t pal_flash_size(const char *label)
{
    (void)label;
    return s_size;
}

const void *pal_flash_mmap(const char *label, size_t *out_len)
{
    (void)label;
    if (out_len) *out_len = s_size;
    return s_flash;
}

void pal_flash_munmap(const char *label)
{
    (void)label;
}

/* Shared mock state (must be defined in exactly one TU). */
mock_call_log_t g_mock_log;
mock_config_t   g_mock_cfg;
