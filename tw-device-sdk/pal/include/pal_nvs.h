/*
 * pal_nvs.h -- Non-volatile key-value storage abstraction.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_NVS_H
#define PAL_NVS_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize non-volatile storage for subsequent get/set operations.
 *
 * Backend contract: Mount or open the NVS partition / file backend, create namespaces as needed,
 * and prepare for key-value access. Must succeed before other pal_nvs_* calls unless the
 * implementation documents lazy init.
 *
 * Thread-safety: Call once from startup (main task) before concurrent NVS use, or serialize
 * with a mutex if init can race.
 *
 * @retval TW_OK     NVS ready.
 * @retval TW_ERR_IO  Storage missing, corrupt, or hardware failure.
 */
tw_err_t pal_nvs_init(void);

/**
 * @brief Read a signed 32-bit integer by key.
 *
 * Backend contract: If @p key exists and was stored as an i32-compatible value, write it to
 * @p *out and return @c TW_OK. If missing, return @c TW_ERR_NOT_FOUND (or documented equivalent).
 *
 * Thread-safety: Serialize with writers for the same key if the backend does not guarantee atomicity.
 *
 * @param key  NUL-terminated ASCII key (implementation may impose max length).
 * @param out  Non-NULL pointer to receive the value.
 * @retval TW_OK           Value stored in @p *out.
 * @retval TW_ERR_NOT_FOUND @p key does not exist or wrong type.
 * @retval TW_ERR_INVAL     Invalid @p key or @p out.
 * @retval TW_ERR_IO        Storage read error.
 */
tw_err_t pal_nvs_get_i32(const char *key, int32_t *out);

/**
 * @brief Store a signed 32-bit integer under a key (staging until commit if required).
 *
 * Backend contract: Associate @p value with @p key. May buffer until pal_nvs_commit depending
 * on the backend (e.g. ESP-IDF style).
 *
 * Thread-safety: Not safe concurrent writes to the same key without external sync unless documented.
 *
 * @param key   NUL-terminated key string.
 * @param value Value to persist.
 * @retval TW_OK       Value staged or written.
 * @retval TW_ERR_INVAL Invalid @p key.
 * @retval TW_ERR_IO    Write or quota failure.
 */
tw_err_t pal_nvs_set_i32(const char *key, int32_t value);

/**
 * @brief Read a string value into a caller buffer.
 *
 * Backend contract: Copy the NUL-terminated string for @p key into @p buf, writing at most
 * @p buf_size bytes including the terminator. If the value is too long, return @c TW_ERR_OVERFLOW
 * or documented error without a full copy.
 *
 * @param key      NUL-terminated key.
 * @param buf      Destination buffer.
 * @param buf_size Size of @p buf in bytes (>= 1).
 * @retval TW_OK           String copied (NUL-terminated if space allows).
 * @retval TW_ERR_NOT_FOUND Key missing or wrong type.
 * @retval TW_ERR_OVERFLOW  Value longer than @p buf_size allows.
 * @retval TW_ERR_INVAL     Invalid arguments.
 * @retval TW_ERR_IO        Storage read error.
 */
tw_err_t pal_nvs_get_str(const char *key, char *buf, size_t buf_size);

/**
 * @brief Store a string value under a key.
 *
 * Backend contract: Persist the string @p value (NUL-terminated). Length limits are
 * implementation-defined.
 *
 * @param key   NUL-terminated key.
 * @param value String to store (NUL-terminated).
 * @retval TW_OK       Stored (staged or committed per backend).
 * @retval TW_ERR_INVAL Invalid key or value.
 * @retval TW_ERR_IO    Write failure.
 */
tw_err_t pal_nvs_set_str(const char *key, const char *value);

/**
 * @brief Read an opaque binary blob by key.
 *
 * Backend contract: On input, @p *len is the size of @p buf. On success, set @p *len to the
 * actual bytes written (<= input size). If @p buf is too small, typical behavior is
 * @c TW_ERR_OVERFLOW with required size documented or returned via @p len.
 *
 * @param key  NUL-terminated key.
 * @param buf  Buffer for blob data.
 * @param len  In: capacity of @p buf; out: bytes stored on success.
 * @retval TW_OK           Blob read.
 * @retval TW_ERR_NOT_FOUND Key missing or wrong type.
 * @retval TW_ERR_OVERFLOW  Buffer too small.
 * @retval TW_ERR_INVAL     Invalid arguments.
 * @retval TW_ERR_IO        Storage read error.
 */
tw_err_t pal_nvs_get_blob(const char *key, void *buf, size_t *len);

/**
 * @brief Store a binary blob under a key.
 *
 * Backend contract: Persist @p len bytes from @p buf under @p key.
 *
 * @param key  NUL-terminated key.
 * @param buf  Blob bytes (may be NULL only if @p len is 0).
 * @param len  Blob length in bytes.
 * @retval TW_OK       Stored.
 * @retval TW_ERR_INVAL Invalid arguments or quota exceeded.
 * @retval TW_ERR_IO    Write failure.
 */
tw_err_t pal_nvs_set_blob(const char *key, const void *buf, size_t len);

/**
 * @brief Remove a key and its value from NVS.
 *
 * Backend contract: Delete @p key if present. Idempotent delete of missing key may return
 * @c TW_OK or @c TW_ERR_NOT_FOUND per implementation.
 *
 * @param key NUL-terminated key.
 * @retval TW_OK           Key removed.
 * @retval TW_ERR_NOT_FOUND Key did not exist (if distinguished).
 * @retval TW_ERR_INVAL     Invalid @p key.
 * @retval TW_ERR_IO        Storage error.
 */
tw_err_t pal_nvs_erase(const char *key);

/**
 * @brief Atomically persist all pending NVS changes.
 *
 * Backend contract: For backends that stage writes, commit them to non-volatile media. For
 * direct-write backends, may be a no-op returning @c TW_OK. Call after batches of set/erase
 * for durability.
 *
 * Thread-safety: Do not overlap commit with concurrent uncoordinated writes if the backend
 * does not support it.
 *
 * @retval TW_OK     Commit succeeded.
 * @retval TW_ERR_IO Power loss during write, flash error, or similar.
 */
tw_err_t pal_nvs_commit(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_NVS_H */
