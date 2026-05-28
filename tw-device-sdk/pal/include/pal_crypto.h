/*
 * pal_crypto.h -- Minimal cryptographic primitives for the SDK.
 *
 * Currently limited to SHA-256 for OTA image verification.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_CRYPTO_H
#define PAL_CRYPTO_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Output size in bytes of the SHA-256 message digest (FIPS 180-4). */
#define PAL_SHA256_DIGEST_SIZE 32

/**
 * @brief Compute SHA-256 over a contiguous buffer (one-shot).
 *
 * Backend contract: Compute the full SHA-256 digest per FIPS 180-4 over @p len bytes at @p data
 * and write exactly @c PAL_SHA256_DIGEST_SIZE bytes to @p digest. The digest buffer must not
 * overlap @p data if the implementation requires disjoint regions.
 *
 * Thread-safety: Re-entrant if @p data and @p digest do not alias shared mutable state.
 *
 * @param data   Input message (may be NULL if @p len is 0).
 * @param len    Message length in bytes.
 * @param digest Output buffer of @c PAL_SHA256_DIGEST_SIZE bytes.
 * @retval TW_OK       Digest written.
 * @retval TW_ERR_INVAL Invalid pointers.
 * @retval TW_ERR_IO    Hardware accelerator or driver failure.
 */
tw_err_t pal_crypto_sha256(const void *data, size_t len,
                           uint8_t digest[PAL_SHA256_DIGEST_SIZE]);

/**
 * @brief Incremental SHA-256 context for streaming (e.g. OTA blocks).
 *
 * Field layout aligns with FIPS 180-4: 256-bit internal state as eight 32-bit words in @c state[8],
 * and a 64-byte (512-bit) block buffer @c buf matching the SHA-256 block size. @c buf_len tracks
 * partial buffer usage; @c total_len tracks accumulated message length for final padding (exact
 * representation is implementation-defined).
 */
typedef struct {
    uint32_t state[8];   /**< Internal hash state (eight 32-bit words per FIPS 180-4). */
    uint8_t  buf[64];    /**< 64-byte block buffer; block size per FIPS 180-4 Section 4.2. */
    size_t   buf_len;    /**< Valid bytes currently in @c buf. */
    uint64_t total_len;  /**< Accumulated message length for padding (semantics per implementation). */
} pal_sha256_ctx_t;

/**
 * @brief Initialize a SHA-256 incremental context.
 *
 * Backend contract: Set @p ctx to the standard SHA-256 initial hash values H(0)..H(7) per FIPS 180-4
 * and clear buffer and length fields.
 *
 * Thread-safety: One context per thread; do not share without locking.
 *
 * @param ctx Non-NULL context to reset.
 */
void     pal_sha256_init(pal_sha256_ctx_t *ctx);

/**
 * @brief Feed more message bytes into an incremental SHA-256 computation.
 *
 * Backend contract: Update @p ctx with @p len bytes from @p data. Safe to call repeatedly; the
 * concatenation of all segments in order equals the message hashed by FIPS 180-4.
 *
 * @param ctx  Initialized or partially updated context.
 * @param data Next message segment (may be NULL if @p len is 0).
 * @param len  Segment length in bytes.
 */
void     pal_sha256_update(pal_sha256_ctx_t *ctx, const void *data, size_t len);

/**
 * @brief Finalize incremental SHA-256 and output the digest.
 *
 * Backend contract: Apply FIPS 180-4 padding to @p ctx, compute the final digest into @p digest,
 * and leave @p ctx in an implementation-defined state (do not reuse without pal_sha256_init).
 *
 * @param ctx    Context after one or more pal_sha256_update calls.
 * @param digest Output buffer of @c PAL_SHA256_DIGEST_SIZE bytes.
 * @retval TW_OK       Digest written.
 * @retval TW_ERR_INVAL Invalid @p ctx or @p digest.
 * @retval TW_ERR_IO    Accelerator failure.
 */
tw_err_t pal_sha256_finish(pal_sha256_ctx_t *ctx,
                           uint8_t digest[PAL_SHA256_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* PAL_CRYPTO_H */
