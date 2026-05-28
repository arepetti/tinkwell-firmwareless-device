/*
 * pal_ota.h -- OTA firmware partition abstraction.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_OTA_H
#define PAL_OTA_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle for an in-progress OTA write session (implementation-defined lifetime). */
typedef struct pal_ota_handle *pal_ota_handle_t;

/**
 * @brief Start a new OTA image write session for an image of known size.
 *
 * Backend contract: Select the inactive OTA slot (or staging area), erase it as needed, and
 * initialize streaming writes. Set @p *out to a handle valid until pal_ota_finish, pal_ota_abort,
 * or process exit. @p image_size is the full firmware image size in bytes for bounds checking.
 *
 * Thread-safety: Only one active OTA session at a time unless the implementation documents
 * otherwise. Call from the main update task.
 *
 * @param out        Non-NULL pointer to receive a new session handle.
 * @param image_size Total expected image size in bytes.
 * @retval TW_OK       Session started; @p *out is valid.
 * @retval TW_ERR_BUSY Another OTA session is active.
 * @retval TW_ERR_INVAL Invalid @p image_size or state.
 * @retval TW_ERR_IO    Flash erase or partition error.
 */
tw_err_t pal_ota_begin(pal_ota_handle_t *out, size_t image_size);

/**
 * @brief Append payload bytes to the OTA image in order.
 *
 * Backend contract: Write @p len bytes from @p data to the next offset in the update partition.
 * Calls must be strictly sequential for a contiguous image unless the port documents block-wise
 * random access (not typical).
 *
 * Thread-safety: Same task as pal_ota_begin; not concurrent with other pal_ota_write on @p h.
 *
 * @param h    Active handle from pal_ota_begin.
 * @param data Next chunk of firmware image (may be NULL if @p len is 0).
 * @param len  Chunk size in bytes.
 * @retval TW_OK       Bytes written.
 * @retval TW_ERR_INVAL Invalid handle or would exceed @p image_size from pal_ota_begin.
 * @retval TW_ERR_IO    Flash write error.
 */
tw_err_t pal_ota_write(pal_ota_handle_t h, const void *data, size_t len);

/**
 * @brief Finalize the OTA write and mark the image ready for boot validation.
 *
 * Backend contract: Close the write session, verify checksum/signature if required by the port,
 * and set the image metadata so pal_ota_set_boot_partition can select it. Invalidates @p h.
 *
 * Thread-safety: Single-threaded with respect to @p h.
 *
 * @param h Session handle from pal_ota_begin (consumed).
 * @retval TW_OK       Image accepted and session closed.
 * @retval TW_ERR_INVAL Incomplete image or bad handle.
 * @retval TW_ERR_IO    Metadata or flash error.
 */
tw_err_t pal_ota_finish(pal_ota_handle_t h);

/**
 * @brief Cancel an in-progress OTA session without installing the image.
 *
 * Backend contract: Discard partial writes, release @p h, and leave the previous running image
 * authoritative. Safe to call after errors.
 *
 * @param h Session handle (may be invalid after return).
 * @retval TW_OK       Aborted cleanly.
 * @retval TW_ERR_INVAL Invalid @p h if detectable.
 */
tw_err_t pal_ota_abort(pal_ota_handle_t h);

/**
 * @brief Configure the bootloader to boot from the updated partition on next reset.
 *
 * Backend contract: Set boot metadata to the slot written by the last successful OTA pipeline.
 * Typically followed by pal_system_reboot. Does not validate image content beyond what
 * pal_ota_finish already enforced.
 *
 * Thread-safety: Call from task context during update flow.
 *
 * @retval TW_OK       Boot slot updated.
 * @retval TW_ERR_IO   Bootloader interaction failure.
 * @retval TW_ERR_INVAL No completed OTA image available.
 */
tw_err_t pal_ota_set_boot_partition(void);

/**
 * @brief Confirm that the currently running firmware is acceptable (cancel rollback).
 *
 * Backend contract: After booting a new image, the application must call this to mark the image
 * valid; otherwise the next reboot may revert to the previous partition (A/B schemes).
 *
 * Thread-safety: Call from main task after self-tests pass.
 *
 * @retval TW_OK     Running image marked valid.
 * @retval TW_ERR_IO Bootloader or NVS update failed.
 */
tw_err_t pal_ota_mark_valid(void);

/**
 * @brief Force boot from the previous known-good firmware partition.
 *
 * Backend contract: On next boot, select the alternate or factory slot per platform policy.
 * Used when the new image fails validation at runtime.
 *
 * @retval TW_OK       Rollback scheduled or applied.
 * @retval TW_ERR_IO   Bootloader error.
 * @retval TW_ERR_INVAL No rollback target.
 */
tw_err_t pal_ota_rollback(void);

/**
 * @brief Whether the running image is awaiting pal_ota_mark_valid confirmation.
 *
 * Backend contract: Return @c true after boot into a new OTA image until mark_valid succeeds;
 * @c false otherwise.
 *
 * Thread-safety: Read-only; safe from multiple tasks but treat as boot-time state.
 *
 * @return @c true if verification is still pending, @c false if not in that state.
 */
bool     pal_ota_is_pending_verify(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_OTA_H */
