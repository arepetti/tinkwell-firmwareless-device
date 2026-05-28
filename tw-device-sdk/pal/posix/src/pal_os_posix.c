/*
 * pal_os_posix.c -- OS primitives via pthreads.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_os.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/* --- Mutex --- */

struct pal_mutex {
    pthread_mutex_t m;
};

tw_err_t pal_mutex_create(pal_mutex_t *out)
{
    struct pal_mutex *mx = calloc(1, sizeof(*mx));
    if (!mx) return TW_ERR_NOMEM;
    pthread_mutex_init(&mx->m, NULL);
    *out = mx;
    return TW_OK;
}

tw_err_t pal_mutex_lock(pal_mutex_t m)   { pthread_mutex_lock(&m->m);   return TW_OK; }
tw_err_t pal_mutex_unlock(pal_mutex_t m) { pthread_mutex_unlock(&m->m); return TW_OK; }
void     pal_mutex_destroy(pal_mutex_t m)
{
    if (!m) return;
    pthread_mutex_destroy(&m->m);
    free(m);
}

/* --- Semaphore --- */

struct pal_sem {
    sem_t s;
};

tw_err_t pal_sem_create(pal_sem_t *out, unsigned int initial)
{
    struct pal_sem *sm = calloc(1, sizeof(*sm));
    if (!sm) return TW_ERR_NOMEM;
    sem_init(&sm->s, 0, initial);
    *out = sm;
    return TW_OK;
}

tw_err_t pal_sem_wait(pal_sem_t s, int timeout_ms)
{
    if (timeout_ms < 0) {
        sem_wait(&s->s);
        return TW_OK;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    return sem_timedwait(&s->s, &ts) == 0 ? TW_OK : TW_ERR_TIMEOUT;
}

tw_err_t pal_sem_post(pal_sem_t s)   { sem_post(&s->s); return TW_OK; }
void     pal_sem_destroy(pal_sem_t s)
{
    if (!s) return;
    sem_destroy(&s->s);
    free(s);
}

/* --- Task --- */

typedef struct {
    pal_task_fn_t fn;
    void         *arg;
} task_trampoline_t;

static void *trampoline(void *raw)
{
    task_trampoline_t *t = raw;
    t->fn(t->arg);
    free(t);
    return NULL;
}

tw_err_t pal_task_create(const char *name, pal_task_fn_t fn, void *arg,
                         size_t stack_size, int priority)
{
    TW_UNUSED(name); TW_UNUSED(priority);
    task_trampoline_t *t = malloc(sizeof(*t));
    if (!t) return TW_ERR_NOMEM;
    t->fn  = fn;
    t->arg = arg;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (stack_size > 0)
        pthread_attr_setstacksize(&attr, stack_size);

    pthread_t tid;
    int rc = pthread_create(&tid, &attr, trampoline, t);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        free(t);
        return TW_ERR_NOMEM;
    }
    return TW_OK;
}

/* --- Time --- */

void pal_sleep_ms(uint32_t ms)
{
    usleep((useconds_t)ms * 1000u);
}

uint64_t pal_uptime_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}
