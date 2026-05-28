/*
 * pal_os_freertos.c -- OS primitives via FreeRTOS.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_os.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include <stdlib.h>

/* --- Mutex --- */

struct pal_mutex {
    SemaphoreHandle_t h;
};

tw_err_t pal_mutex_create(pal_mutex_t *out)
{
    struct pal_mutex *m = calloc(1, sizeof(*m));
    if (!m) return TW_ERR_NOMEM;
    m->h = xSemaphoreCreateMutex();
    if (!m->h) { free(m); return TW_ERR_NOMEM; }
    *out = m;
    return TW_OK;
}

tw_err_t pal_mutex_lock(pal_mutex_t m)
{
    return xSemaphoreTake(m->h, portMAX_DELAY) ? TW_OK : TW_ERR_TIMEOUT;
}

tw_err_t pal_mutex_unlock(pal_mutex_t m)
{
    xSemaphoreGive(m->h);
    return TW_OK;
}

void pal_mutex_destroy(pal_mutex_t m)
{
    if (!m) return;
    vSemaphoreDelete(m->h);
    free(m);
}

/* --- Semaphore --- */

struct pal_sem {
    SemaphoreHandle_t h;
};

tw_err_t pal_sem_create(pal_sem_t *out, unsigned int initial)
{
    struct pal_sem *s = calloc(1, sizeof(*s));
    if (!s) return TW_ERR_NOMEM;
    s->h = xSemaphoreCreateCounting(0xFFFF, initial);
    if (!s->h) { free(s); return TW_ERR_NOMEM; }
    *out = s;
    return TW_OK;
}

tw_err_t pal_sem_wait(pal_sem_t s, int timeout_ms)
{
    TickType_t ticks = timeout_ms < 0
        ? portMAX_DELAY
        : pdMS_TO_TICKS((uint32_t)timeout_ms);
    return xSemaphoreTake(s->h, ticks) ? TW_OK : TW_ERR_TIMEOUT;
}

tw_err_t pal_sem_post(pal_sem_t s)
{
    xSemaphoreGive(s->h);
    return TW_OK;
}

void pal_sem_destroy(pal_sem_t s)
{
    if (!s) return;
    vSemaphoreDelete(s->h);
    free(s);
}

/* --- Task --- */

tw_err_t pal_task_create(const char *name, pal_task_fn_t fn, void *arg,
                         size_t stack_size, int priority)
{
    if (stack_size == 0) stack_size = 4096;
    BaseType_t rc = xTaskCreate((TaskFunction_t)fn, name,
                                (uint32_t)stack_size, arg,
                                (UBaseType_t)priority, NULL);
    return rc == pdPASS ? TW_OK : TW_ERR_NOMEM;
}

/* --- Time --- */

void pal_sleep_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint64_t pal_uptime_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}
