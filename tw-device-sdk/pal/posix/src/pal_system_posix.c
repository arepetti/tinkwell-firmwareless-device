/*
 * pal_system_posix.c -- System info on POSIX.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_system.h"
#include "pal_log.h"

#include <stdlib.h>
#include <unistd.h>

#define TAG "sys"

void pal_system_reboot(void)
{
    PAL_LOGW(TAG, "reboot requested -- exiting process");
    _exit(0);
}

void pal_system_reboot_to_factory(void)
{
    PAL_LOGW(TAG, "reboot-to-factory requested -- exiting (no factory partition on POSIX)");
    _exit(0);
}

pal_boot_reason_t pal_system_boot_reason(void)
{
    return PAL_BOOT_COLD;
}

uint32_t pal_system_free_heap(void)
{
    return 0; /* not meaningful on a hosted OS */
}

const char *pal_system_chip_info(void)
{
    return "posix-host";
}
