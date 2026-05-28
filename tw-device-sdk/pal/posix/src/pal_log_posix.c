/*
 * pal_log_posix.c -- Logging via fprintf(stderr).
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static const char *level_str[] = { "E", "W", "I", "D", "T" };

void pal_log(pal_log_level_t level, const char *tag, const char *fmt, ...)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    unsigned sec  = (unsigned)(ts.tv_sec % 3600);
    unsigned msec = (unsigned)(ts.tv_nsec / 1000000);

    fprintf(stderr, "%s (%u.%03u) [%s] ", level_str[level], sec, msec, tag);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
}
