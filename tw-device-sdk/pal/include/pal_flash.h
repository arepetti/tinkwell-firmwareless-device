/*
 * pal_flash.h -- Raw flash read/write for applet storage.
 *
 * On ESP-IDF this maps to a named partition with optional mmap for XIP.
 * On POSIX this maps to a regular file.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_FLASH_H
#define PAL_FLASH_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open or resolve a flash partition by human-readable label.
 *
 * Backend contract: Map @p label to a partition table entry (e.g. ESP-IDF) or file path (POSIX).
 * Subsequent pal_flash_* calls with the same label refer to that region. May validate size and
 * alignment.
 *
 * Thread-safety: Call during initialization from the main task; concurrent init of the same label
 * is undefined.
 *
 * @param label Partition name (NUL-terminated, implementation-defined max length).
 * @retval TW_OK       Partition ready.
 * @retval TW_ERR_NOT_FOUND Unknown @p label.
 * @retval TW_ERR_IO    Mount or open failure.
 */
tw_err_t    pal_flash_init(const char *label);

/**
 * @brief Erase the entire partition associated with @p label.
 *
 * Backend contract: Erase all sectors in the partition to the erased flash value (0xFF typical).
 * Required before writes on many NOR flashes if the region was not already erased.
 *
 * Thread-safety: Exclusive access required; do not overlap with read/write/mmap on the same label.
 *
 * @param label Partition to erase.
 * @retval TW_OK       Erase completed.
 * @retval TW_ERR_NOT_FOUND Unknown @p label.
 * @retval TW_ERR_IO    Flash driver error.
 */
tw_err_t    pal_flash_erase(const char *label);

/**
 * @brief Write bytes at a byte offset within the partition.
 *
 * Backend contract: Program @p len bytes from @p data at @p offset from the partition start.
 * Offsets must fall within pal_flash_size; writes may require prior erase of the containing
 * sector(s). Unaligned writes follow hardware rules.
 *
 * Thread-safety: Not concurrent with erase or mmap on the same @p label without synchronization.
 *
 * @param label  Partition label.
 * @param offset Byte offset from partition base.
 * @param data   Source bytes (may be NULL if @p len is 0).
 * @param len    Number of bytes to write.
 * @retval TW_OK       Write succeeded.
 * @retval TW_ERR_INVAL Out of range or unaligned for hardware.
 * @retval TW_ERR_IO    Program failure.
 */
tw_err_t    pal_flash_write(const char *label, size_t offset,
                            const void *data, size_t len);

/**
 * @brief Read bytes at a byte offset within the partition.
 *
 * Backend contract: Copy up to @p len bytes into @p buf from @p offset. Reads need not follow
 * an erase.
 *
 * Thread-safety: Concurrent reads are safe if the implementation allows; not safe concurrent
 * with erase of overlapping regions.
 *
 * @param label  Partition label.
 * @param offset Byte offset from partition base.
 * @param buf    Destination buffer.
 * @param len    Number of bytes to read.
 * @retval TW_OK       Read completed.
 * @retval TW_ERR_INVAL Out of range.
 * @retval TW_ERR_IO    Read error.
 */
tw_err_t    pal_flash_read(const char *label, size_t offset,
                           void *buf, size_t len);

/**
 * @brief Return the size in bytes of the partition for @p label.
 *
 * Backend contract: Report the span usable by read/write/erase for that label.
 *
 * @param label Partition label.
 * @return Size in bytes, or @c 0 if unknown (implementation may use @c 0 for missing label).
 */
size_t      pal_flash_size(const char *label);

/**
 * @brief Map the partition read-only into the CPU address space (XIP when supported).
 *
 * Backend contract: If supported, return a const pointer to the partition contents and set
 * @p *out_len to the mapped length. On failure, return @c NULL. Caller must pair with
 * pal_flash_munmap for the same @p label when done. While mapped, avoid erasing that region.
 *
 * Thread-safety: Not concurrent with erase/write overlapping the mapping; serialize with other
 * PAL flash calls on the same label.
 *
 * @param label   Partition label.
 * @param out_len Non-NULL pointer to receive the mapped size in bytes.
 * @return Read-only pointer to the data, or @c NULL on failure.
 */
const void *pal_flash_mmap(const char *label, size_t *out_len);

/**
 * @brief Unmap a previous pal_flash_mmap for @p label.
 *
 * Backend contract: Release CPU mapping resources. @p label must match a successful mmap.
 *
 * @param label Partition label previously mmap'd.
 */
void        pal_flash_munmap(const char *label);

#ifdef __cplusplus
}
#endif

#endif /* PAL_FLASH_H */
