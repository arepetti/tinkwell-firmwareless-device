/*
 * tw_block.h -- Opt-in Block1 (RFC 7959) reassembly helper.
 *
 * Application handlers that want full-payload reassembly in RAM can use
 * tw_block_ctx_t to accumulate Block1 fragments into a caller-provided
 * buffer.  The SDK's own OTA and applet services do NOT use this --
 * they stream directly to flash, validating block sequence inline.
 *
 * This helper has no libcoap dependency; it validates sequence numbers
 * and copies payload bytes using only the fields in tw_msg_request_t.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_BLOCK_H
#define TW_BLOCK_H

#include "tw_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reassembly context for Block1 transfers.
 *
 * Allocate one per resource that needs full reassembly.  The caller
 * owns the buffer; the context tracks write position and expected
 * sequence number.
 */
typedef struct {
    uint8_t *buf;       /**< Caller-owned reassembly buffer. */
    size_t   buf_size;  /**< Total size of @a buf in bytes. */
    size_t   received;  /**< Bytes accumulated so far. */
    uint32_t next_num;  /**< Next expected block sequence number (0-based). */
} tw_block_ctx_t;

/**
 * @brief Initialize (or reset) a block reassembly context.
 *
 * @param ctx      Context to initialize.
 * @param buf      Caller-owned buffer for reassembly.
 * @param buf_size Size of @a buf in bytes.
 */
void tw_block_init(tw_block_ctx_t *ctx, uint8_t *buf, size_t buf_size);

/**
 * @brief Feed one Block1 request into the reassembly context.
 *
 * Validates that @a req->block1_num matches the expected sequence,
 * copies payload into @a ctx->buf, echoes Block1 metadata in @a resp,
 * and returns:
 *
 *   - @c TW_ERR_NOT_READY + resp code 2.31 Continue when more blocks are expected.
 *   - @c TW_OK when the final block arrives (ctx->buf holds the full payload,
 *     ctx->received is the total length).
 *   - @c TW_ERR_INVAL + resp code 4.08 on out-of-order blocks.
 *   - @c TW_ERR_OVERFLOW + resp code 4.13 if the payload exceeds the buffer.
 *
 * @param ctx  Reassembly context (must have been initialized with tw_block_init).
 * @param req  Inbound request with has_block1 == true.
 * @param resp Response object; Block1 echo and code are set automatically.
 * @retval TW_OK            Final block received; reassembly complete.
 * @retval TW_ERR_NOT_READY More blocks expected (2.31 Continue sent).
 * @retval TW_ERR_INVAL     Block out of order (4.08 Incomplete sent).
 * @retval TW_ERR_OVERFLOW  Payload exceeds buffer (4.13 Too Large sent).
 */
tw_err_t tw_block_feed(tw_block_ctx_t *ctx,
                       tw_msg_request_t *req,
                       tw_msg_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* TW_BLOCK_H */
