/*
 * tw_msg_helpers.c -- Response helpers extracted for test builds.
 *
 * These are the protocol-neutral tw_msg_respond_* functions normally
 * compiled as part of proto_coap.c.  Test builds cannot link libcoap,
 * so this file provides the same implementations standalone.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_msg.h"

#include <string.h>
#include <stdio.h>

tw_err_t tw_msg_respond_with_code(tw_msg_response_t *resp,
                                   uint8_t code,
                                   const char *diagnostic)
{
    resp->code = code;
    if (diagnostic) {
        size_t len = strlen(diagnostic);
        if (len > resp->payload_capacity) return TW_ERR_OVERFLOW;
        memcpy(resp->payload, diagnostic, len);
        resp->payload_len = len;
    } else {
        resp->payload_len = 0;
    }
    return TW_OK;
}

tw_err_t tw_msg_respond_text(tw_msg_response_t *resp, const char *text)
{
    size_t len = strlen(text);
    if (len > resp->payload_capacity) return TW_ERR_OVERFLOW;
    memcpy(resp->payload, text, len);
    resp->payload_len = len;
    resp->code = TW_MSG_205_CONTENT;
    return TW_OK;
}

tw_err_t tw_msg_respond_i32(tw_msg_response_t *resp, int32_t value)
{
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "%d", (int)value);
    if (n < 0 || (size_t)n >= sizeof(buf)) return TW_ERR_OVERFLOW;
    if ((size_t)n > resp->payload_capacity) return TW_ERR_OVERFLOW;
    memcpy(resp->payload, buf, (size_t)n);
    resp->payload_len = (size_t)n;
    resp->code = TW_MSG_205_CONTENT;
    return TW_OK;
}

tw_err_t tw_msg_respond_buf(tw_msg_response_t *resp,
                            const void *data, size_t len)
{
    if (len > resp->payload_capacity) return TW_ERR_OVERFLOW;
    memcpy(resp->payload, data, len);
    resp->payload_len = len;
    resp->code = TW_MSG_205_CONTENT;
    return TW_OK;
}

tw_err_t tw_msg_respond_empty(tw_msg_response_t *resp, uint8_t code)
{
    resp->payload_len = 0;
    resp->code = code;
    return TW_OK;
}
