/*
 * mock_pal.h -- Shared mock infrastructure for unit tests.
 *
 * Provides call recording and injectable return values for
 * all PAL functions so tests can verify SDK behaviour without
 * any real I/O.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef MOCK_PAL_H
#define MOCK_PAL_H

#include "tw_types.h"

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOCK_MAX_CALLS  256
#define MOCK_MAX_ARGLEN 128

typedef struct {
    char function[64];
    char args[MOCK_MAX_ARGLEN];
} mock_call_t;

typedef struct {
    mock_call_t calls[MOCK_MAX_CALLS];
    size_t      count;
} mock_call_log_t;

extern mock_call_log_t g_mock_log;

static inline void mock_reset(void)
{
    memset(&g_mock_log, 0, sizeof(g_mock_log));
}

static inline void mock_record(const char *fn, const char *args)
{
    if (g_mock_log.count >= MOCK_MAX_CALLS) return;
    mock_call_t *c = &g_mock_log.calls[g_mock_log.count++];
    strncpy(c->function, fn, sizeof(c->function) - 1);
    if (args)
        strncpy(c->args, args, sizeof(c->args) - 1);
}

static inline size_t mock_count(const char *fn)
{
    size_t n = 0;
    for (size_t i = 0; i < g_mock_log.count; i++)
        if (strcmp(g_mock_log.calls[i].function, fn) == 0) n++;
    return n;
}

/* Injectable values for mock functions. */
typedef struct {
    int32_t gpio_read_value;
    int32_t i2c_read_temp;
    int32_t i2c_read_humid;
    tw_err_t next_error;
} mock_config_t;

extern mock_config_t g_mock_cfg;

static inline void mock_config_reset(void)
{
    memset(&g_mock_cfg, 0, sizeof(g_mock_cfg));
    g_mock_cfg.i2c_read_temp  = 215;
    g_mock_cfg.i2c_read_humid = 450;
}

#ifdef __cplusplus
}
#endif

#endif /* MOCK_PAL_H */
