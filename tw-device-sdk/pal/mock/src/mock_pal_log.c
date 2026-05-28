/*
 * mock_pal_log.c -- Logs to stderr (same as POSIX).
 * SPDX-License-Identifier: MIT
 */

#include "pal_log.h"
#include <stdio.h>
#include <stdarg.h>

void pal_log(pal_log_level_t level, const char *tag, const char *fmt, ...)
{
    static const char *lvl[] = { "E", "W", "I", "D", "V" };
    fprintf(stderr, "%s [%s] ", lvl[level], tag);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
