/*
 * mock_pal_system.c
 * SPDX-License-Identifier: MIT
 */

#include "pal_system.h"
#include "mock_pal.h"

static int s_reboot_count;

void pal_system_reboot(void)
{
    mock_record("pal_system_reboot", NULL);
    s_reboot_count++;
}

void pal_system_reboot_to_factory(void)
{
    mock_record("pal_system_reboot_to_factory", NULL);
    s_reboot_count++;
}

pal_boot_reason_t pal_system_boot_reason(void) { return PAL_BOOT_COLD; }
uint32_t pal_system_free_heap(void) { return 128 * 1024; }
const char *pal_system_chip_info(void) { return "mock-test"; }
