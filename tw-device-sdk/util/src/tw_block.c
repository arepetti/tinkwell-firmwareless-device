/*
 * tw_block.c -- Opt-in Block1 (RFC 7959) reassembly helper.
 *
 * Pure SDK utility with no libcoap dependency.  Validates sequence
 * numbers and copies payload bytes into a caller-provided buffer.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_block.h"

#include <string.h>

void tw_block_init(tw_block_ctx_t *ctx, uint8_t *buf, size_t buf_size)
{
    ctx->buf      = buf;
    ctx->buf_size = buf_size;
    ctx->received = 0;
    ctx->next_num = 0;
}

tw_err_t tw_block_feed(tw_block_ctx_t *ctx,
                       tw_msg_request_t *req,
                       tw_msg_response_t *resp)
{
    if (!req->has_block1) {
        tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "expected Block1");
        return TW_ERR_INVAL;
    }

    if (req->block1_num != ctx->next_num) {
        tw_msg_respond_with_code(resp, TW_MSG_408_INCOMPLETE, "out of order");
        return TW_ERR_INVAL;
    }

    if (ctx->received + req->payload_len > ctx->buf_size) {
        tw_msg_respond_with_code(resp, TW_MSG_413_TOO_LARGE, NULL);
        return TW_ERR_OVERFLOW;
    }

    memcpy(ctx->buf + ctx->received, req->payload, req->payload_len);
    ctx->received += req->payload_len;
    ctx->next_num++;

    /* Echo Block1 option so the client knows the server accepted this block. */
    resp->set_block1  = true;
    resp->block1_num  = req->block1_num;
    resp->block1_more = req->block1_more;
    resp->block1_szx  = req->block1_szx;

    if (req->block1_more) {
        /* RFC 7959 section 2.5: respond 2.31 Continue while more blocks pending. */
        resp->code = TW_MSG_231_CONTINUE;
        return TW_ERR_NOT_READY;
    }

    /* Final block: reassembly complete. */
    resp->code = TW_MSG_204_CHANGED;
    return TW_OK;
}
