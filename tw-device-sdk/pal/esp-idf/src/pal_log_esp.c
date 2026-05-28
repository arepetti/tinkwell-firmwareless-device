/*
 * pal_log_esp.c -- Logging via ESP-IDF ESP_LOGx.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_log.h"
#include "esp_log.h"
#include <stdarg.h>
#include <stdio.h>

void pal_log(pal_log_level_t level, const char *tag, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    switch (level) {
    case PAL_LOG_ERROR: ESP_LOGE(tag, "%s", buf); break;
    case PAL_LOG_WARN:  ESP_LOGW(tag, "%s", buf); break;
    case PAL_LOG_INFO:  ESP_LOGI(tag, "%s", buf); break;
    case PAL_LOG_DEBUG: ESP_LOGD(tag, "%s", buf); break;
    case PAL_LOG_TRACE: ESP_LOGV(tag, "%s", buf); break;
    }
}
