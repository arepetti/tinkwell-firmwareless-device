/*
 * pal_system.h -- System information and control.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_SYSTEM_H
#define PAL_SYSTEM_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Classified reason for the current boot or last reset. */
typedef enum {
    PAL_BOOT_COLD,      /**< Power-on or external reset pin; not a soft reboot from firmware. */
    PAL_BOOT_OTA,       /**< Bootloader chose a slot after an OTA update. */
    PAL_BOOT_WATCHDOG,  /**< Watchdog timer expiry triggered reset. */
    PAL_BOOT_DEEPSLEEP, /**< Resumed from deep sleep (may overlap with other flags per SoC). */
    PAL_BOOT_UNKNOWN,   /**< Reason not available from ROM or bootloader. */
} pal_boot_reason_t;

/**
 * @brief Request a normal system reset.
 *
 * Backend contract: Perform a chip or application reset as appropriate; typically does not return.
 * Flush volatile state per platform requirements before calling.
 *
 * Thread-safety: Call from task context; not ISR-safe unless documented.
 */
void              pal_system_reboot(void);

/**
 * @brief Reset and prefer factory or recovery firmware behavior.
 *
 * Backend contract: Clear user configuration and/or boot from a factory partition as defined
 * by the port. May erase NVS or similar; document destructive effects in the implementation.
 *
 * Thread-safety: Task context only.
 */
void              pal_system_reboot_to_factory(void);

/**
 * @brief Return the classified boot reason for this run.
 *
 * Backend contract: Read ROM or bootloader flags and map to pal_boot_reason_t. Best effort if
 * multiple causes apply.
 *
 * Thread-safety: Safe any time after platform init; typically constant for the process lifetime.
 *
 * @return Boot reason classification.
 */
pal_boot_reason_t pal_system_boot_reason(void);

/**
 * @brief Approximate free heap size for dynamic allocation.
 *
 * Backend contract: Return the number of bytes currently available from the C heap or primary
 * allocator (may be approximate and change under fragmentation).
 *
 * Thread-safety: May be called from multiple tasks; value is a snapshot.
 *
 * @return Free heap bytes (platform-defined semantics).
 */
uint32_t          pal_system_free_heap(void);

/**
 * @brief Human-readable chip or platform identifier string.
 *
 * Backend contract: Return a static or immortal NUL-terminated string (e.g. SoC name, revision).
 * Pointer remains valid for the lifetime of the firmware.
 *
 * Thread-safety: Read-only; safe from any context once returned.
 *
 * @return Pointer to chip info string (never NULL if contract satisfied).
 */
const char       *pal_system_chip_info(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_SYSTEM_H */
