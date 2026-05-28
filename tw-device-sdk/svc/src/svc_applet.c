/*
 * svc_applet.c -- Applet push/store/load service.
 *
 * Manages the lifecycle of WASM applets pushed from the hub:
 *   POST /tw/applet/push   -- Block1 transfer of .wasm binary
 *   POST /tw/applet/commit -- finalise and load the applet
 *   GET  /tw/applet/status -- current applet state
 *
 * The applet binary is stored in a dedicated flash partition
 * (ESP-IDF) or a file (POSIX) and optionally memory-mapped
 * for interpreter XIP.
 *
 * Only compiled when CONFIG_TW_APPLET_ENABLED is set.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_applet.h"
#include "tw_types.h"
#include "tw_msg.h"
#include "pal_flash.h"
#include "pal_log.h"

#ifdef CONFIG_TW_USE_PROTOBUF
#include <pb_encode.h>
#include "tw_protocol.pb.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "applet"

#ifndef CONFIG_TW_APPLET_MAX_SIZE
#define CONFIG_TW_APPLET_MAX_SIZE 65536
#endif

#ifndef CONFIG_TW_APPLET_FLASH_PARTITION
#define CONFIG_TW_APPLET_FLASH_PARTITION "applet"
#endif

static tw_applet_state_t s_state = TW_APPLET_NONE;
static size_t            s_received;
static size_t            s_expected;
static char              s_version[32];
static uint32_t          s_expected_block;

tw_applet_state_t tw_applet_state(void)
{
    return s_state;
}

const char *tw_applet_version(void)
{
    return s_version[0] ? s_version : NULL;
}

/* ---- CoAP handlers ---- */

/**
 * Bounded search for a substring within a byte buffer of known length.
 * Unlike strstr, this never reads past buf + buf_len.
 */
static const char *bounded_find(const uint8_t *buf, size_t buf_len,
                                const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen > buf_len) return NULL;
    for (size_t i = 0; i <= buf_len - nlen; i++) {
        if (memcmp(buf + i, needle, nlen) == 0)
            return (const char *)(buf + i);
    }
    return NULL;
}

/** Receive applet binary blocks (Block1) and write to flash. */
tw_err_t svc_applet_on_push(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    /*
     * C2 fix: only restart on a new block 0, not on any request while
     * loading.  This allows multi-block transfers to proceed.
     */
    if (s_state == TW_APPLET_LOADING && req->has_block1 &&
        req->block1_num == 0) {
        PAL_LOGW(TAG, "push already in progress, restarting");
        s_received = 0;
        s_expected = 0;
        s_expected_block = 0;
        pal_flash_erase(CONFIG_TW_APPLET_FLASH_PARTITION);
        s_state = TW_APPLET_NONE;
    }

    if (!req->has_block1) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ,
                                         "Block1 required");
    }

    /*
     * First block: parse "size=NNNN" header from the payload.
     * Any bytes after the header are the beginning of the WASM binary
     * and are written to flash so no data is lost (C3 fix).
     */
    if (req->block1_num == 0 && s_state != TW_APPLET_LOADING) {
        s_expected = 0;
        const uint8_t *data_start = NULL;
        size_t data_len = 0;

        if (req->payload && req->payload_len > 0) {
            /* C5 fix: bounded search instead of strstr on raw payload. */
            const char *p = bounded_find(req->payload, req->payload_len,
                                         "size=");
            if (p) {
                s_expected = (size_t)strtoul(p + 5, NULL, 10);

                /* Skip past the header to find where binary data begins. */
                const char *end = p + 5;
                const char *limit = (const char *)req->payload + req->payload_len;
                while (end < limit && *end >= '0' && *end <= '9') end++;
                while (end < limit && (*end == '\n' || *end == '\r' ||
                                       *end == '&' || *end == ' '))
                    end++;
                data_start = (const uint8_t *)end;
                data_len = (size_t)(limit - end);
            }
        }

        if (s_expected == 0 || s_expected > CONFIG_TW_APPLET_MAX_SIZE) {
            return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ,
                                             "invalid size");
        }

        pal_flash_erase(CONFIG_TW_APPLET_FLASH_PARTITION);
        s_received = 0;
        s_expected_block = 0;
        s_state = TW_APPLET_LOADING;
        PAL_LOGI(TAG, "applet push begin: %zu bytes", s_expected);

        /* C3 fix: write any binary data trailing the header to flash. */
        if (data_start && data_len > 0) {
            if (data_len > s_expected) {
                s_state = TW_APPLET_ERROR;
                return tw_msg_respond_with_code(resp, TW_MSG_413_TOO_LARGE,
                                                 NULL);
            }
            tw_err_t err = pal_flash_write(CONFIG_TW_APPLET_FLASH_PARTITION,
                                           0, data_start, data_len);
            if (err != TW_OK) {
                s_state = TW_APPLET_ERROR;
                return tw_msg_respond_with_code(resp, TW_MSG_500_INTERNAL,
                                                 "flash write failed");
            }
            s_received = data_len;
        }

        resp->set_block1  = true;
        resp->block1_num  = req->block1_num;
        resp->block1_more = req->block1_more;
        resp->block1_szx  = req->block1_szx;
        s_expected_block++;

        resp->code = req->block1_more ? TW_MSG_231_CONTINUE : TW_MSG_201_CREATED;
        return TW_OK;
    }

    /* Subsequent blocks: validate sequence and write to flash. */
    if (s_state != TW_APPLET_LOADING) {
        return tw_msg_respond_with_code(resp, TW_MSG_408_INCOMPLETE,
                                         "no active push");
    }

    if (req->block1_num != s_expected_block) {
        PAL_LOGW(TAG, "applet block out of order: got %u, expected %u",
                 (unsigned)req->block1_num, (unsigned)s_expected_block);
        return tw_msg_respond_with_code(resp, TW_MSG_408_INCOMPLETE,
                                         "out of order");
    }

    if (!req->payload || req->payload_len == 0) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "empty block");
    }

    /* H5 fix: overflow-safe bounds check before writing to flash. */
    if (req->payload_len > s_expected - s_received) {
        PAL_LOGE(TAG, "applet block exceeds declared size (%zu + %zu > %zu)",
                 s_received, req->payload_len, s_expected);
        s_state = TW_APPLET_ERROR;
        return tw_msg_respond_with_code(resp, TW_MSG_413_TOO_LARGE, NULL);
    }

    tw_err_t err = pal_flash_write(CONFIG_TW_APPLET_FLASH_PARTITION,
                                   s_received, req->payload, req->payload_len);
    if (err != TW_OK) {
        s_state = TW_APPLET_ERROR;
        return tw_msg_respond_with_code(resp, TW_MSG_500_INTERNAL,
                                         "flash write failed");
    }

    s_received += req->payload_len;
    s_expected_block++;
    PAL_LOGD(TAG, "applet block #%u: +%zu (%zu / %zu)",
             (unsigned)req->block1_num, req->payload_len,
             s_received, s_expected);

    resp->set_block1  = true;
    resp->block1_num  = req->block1_num;
    resp->block1_more = req->block1_more;
    resp->block1_szx  = req->block1_szx;

    resp->code = req->block1_more ? TW_MSG_231_CONTINUE : TW_MSG_204_CHANGED;
    return TW_OK;
}

/** Finalise push, record version, and mark applet ready for load. */
tw_err_t svc_applet_on_commit(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    if (s_state != TW_APPLET_LOADING) {
        return tw_msg_respond_with_code(resp, TW_MSG_408_INCOMPLETE,
                                         "no active push");
    }

    /* C4 fix: reject commit if the transfer is incomplete. */
    if (s_received < s_expected) {
        PAL_LOGW(TAG, "applet commit with incomplete data: %zu / %zu",
                 s_received, s_expected);
        return tw_msg_respond_with_code(resp, TW_MSG_408_INCOMPLETE,
                                         "incomplete transfer");
    }

    if (req->payload && req->payload_len > 0) {
        /* C5 fix: bounded search for version= header. */
        const char *p = bounded_find(req->payload, req->payload_len,
                                     "version=");
        if (p) {
            const char *v = p + 8;
            const char *limit = (const char *)req->payload + req->payload_len;
            size_t vlen = 0;
            while (v + vlen < limit && v[vlen] != '&' && v[vlen] != '\n' &&
                   vlen < sizeof(s_version) - 1)
                vlen++;
            memcpy(s_version, v, vlen);
            s_version[vlen] = '\0';
        }
    }

    PAL_LOGI(TAG, "applet committed: %zu bytes, version=%s",
             s_received, s_version[0] ? s_version : "unknown");

    s_state = TW_APPLET_RUNNING;

#ifdef CONFIG_TW_USE_PROTOBUF
    {
        tw_AppletCommitReply reply = tw_AppletCommitReply_init_zero;
        reply.success = true;
        uint8_t rbuf[tw_AppletCommitReply_size];
        pb_ostream_t rs = pb_ostream_from_buffer(rbuf, sizeof(rbuf));
        if (pb_encode(&rs, tw_AppletCommitReply_fields, &reply)) {
            resp->code = TW_MSG_204_CHANGED;
            tw_msg_respond_buf(resp, rbuf, rs.bytes_written);
        } else {
            tw_msg_respond_with_code(resp, TW_MSG_204_CHANGED, NULL);
        }
    }
#else
    tw_msg_respond_with_code(resp, TW_MSG_204_CHANGED, NULL);
#endif

    /*
     * The actual WASM module load happens in the application's
     * runtime layer (wasm_runtime.c), which polls tw_applet_state()
     * and calls pal_flash_mmap() when it sees RUNNING.
     */
    return TW_OK;
}

/** Return applet state, version, and size. */
tw_err_t svc_applet_on_status(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    TW_UNUSED(req);

#ifdef CONFIG_TW_USE_PROTOBUF
    static const char *state_names[] = {
        [TW_APPLET_NONE]    = "empty",
        [TW_APPLET_LOADING] = "loading",
        [TW_APPLET_RUNNING] = "running",
        [TW_APPLET_ERROR]   = "error",
    };

    tw_AppletStatus msg = tw_AppletStatus_init_zero;
    const char *sn = (s_state < TW_ARRAY_SIZE(state_names) && state_names[s_state])
                     ? state_names[s_state] : "unknown";
    strncpy(msg.state, sn, sizeof(msg.state) - 1);
    msg.size_bytes = (uint32_t)s_received;

    uint8_t buf[tw_AppletStatus_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
    if (!pb_encode(&stream, tw_AppletStatus_fields, &msg)) {
        return tw_msg_respond_empty(resp, TW_MSG_500_INTERNAL);
    }

    resp->code = TW_MSG_205_CONTENT;
    return tw_msg_respond_buf(resp, buf, stream.bytes_written);
#else
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "state=%d&version=%s&size=%zu\n",
                     (int)s_state,
                     s_version[0] ? s_version : "none",
                     s_received);
    if (n < 0 || (size_t)n >= sizeof(buf))
        return tw_msg_respond_empty(resp, TW_MSG_500_INTERNAL);
    return tw_msg_respond_text(resp, buf);
#endif
}

tw_msg_resource_t svc_applet_resources[] = {
    { "/tw/applet/push",   TW_MSG_POST, svc_applet_on_push },
    { "/tw/applet/commit", TW_MSG_POST, svc_applet_on_commit },
    { "/tw/applet/status", TW_MSG_GET,  svc_applet_on_status },
    TW_MSG_RESOURCE_END
};

void svc_applet_init(void)
{
    tw_err_t err = pal_flash_init(CONFIG_TW_APPLET_FLASH_PARTITION);
    if (err == TW_OK) {
        size_t sz = pal_flash_size(CONFIG_TW_APPLET_FLASH_PARTITION);
        PAL_LOGI(TAG, "applet partition ready (%zu bytes)", sz);
    } else {
        PAL_LOGW(TAG, "applet partition not available");
    }
}
