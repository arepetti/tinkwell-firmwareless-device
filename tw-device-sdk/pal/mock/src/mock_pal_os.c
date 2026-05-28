/*
 * mock_pal_os.c
 * SPDX-License-Identifier: MIT
 */

#include "pal_os.h"
#include "mock_pal.h"
#include <stdlib.h>

struct pal_mutex  { int dummy; };
struct pal_sem    { unsigned int count; };

tw_err_t pal_mutex_create(pal_mutex_t *out)
{
    mock_record("pal_mutex_create", NULL);
    *out = calloc(1, sizeof(struct pal_mutex));
    return *out ? TW_OK : TW_ERR_NOMEM;
}

tw_err_t pal_mutex_lock(pal_mutex_t m)   { (void)m; return TW_OK; }
tw_err_t pal_mutex_unlock(pal_mutex_t m) { (void)m; return TW_OK; }
void pal_mutex_destroy(pal_mutex_t m)    { free(m); }

tw_err_t pal_sem_create(pal_sem_t *out, unsigned int initial)
{
    mock_record("pal_sem_create", NULL);
    struct pal_sem *s = calloc(1, sizeof(*s));
    if (!s) return TW_ERR_NOMEM;
    s->count = initial;
    *out = s;
    return TW_OK;
}

tw_err_t pal_sem_wait(pal_sem_t s, int timeout_ms)
{
    (void)timeout_ms;
    if (s->count > 0) { s->count--; return TW_OK; }
    return TW_ERR_TIMEOUT;
}

tw_err_t pal_sem_post(pal_sem_t s)
{
    s->count++;
    return TW_OK;
}

void pal_sem_destroy(pal_sem_t s) { free(s); }

tw_err_t pal_task_create(const char *name, pal_task_fn_t fn, void *arg,
                         size_t stack_size, int priority)
{
    mock_record("pal_task_create", name);
    (void)fn; (void)arg; (void)stack_size; (void)priority;
    return TW_OK;
}

static uint64_t s_uptime;

void pal_sleep_ms(uint32_t ms) { s_uptime += ms; }
uint64_t pal_uptime_ms(void)   { return s_uptime; }
