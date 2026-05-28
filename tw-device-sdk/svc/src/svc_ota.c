/*
 * svc_ota.c -- OTA push receiver (auto-registered CoAP resources).
 *
 * The hub pushes firmware via CoAP Block1 transfers:
 *   1. POST /tw/ota/begin   { size: N, sha256: "..." }
 *   2. PUT  /tw/ota/block   (Block1 transfer, repeated)
 *   3. POST /tw/ota/commit  (trigger verify + reboot)
 *   4. GET  /tw/ota/status  (query state at any time)
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_ota.h"
#include "tw_types.h"
#include "tw_msg.h"
#include "tw_lock.h"
#include "pal_crypto.h"
#include "pal_ota.h"
#include "pal_log.h"
#include "pal_system.h"
#include "pal_os.h"

#ifdef CONFIG_TW_USE_PROTOBUF
#include <pb_decode.h>
#include <pb_encode.h>
#include "tw_protocol.pb.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "ota"

/* Delay before reboot so the CoAP client can read the final response. */
#define REBOOT_DELAY_MS 500

static tw_lock_t        s_lock;
static tw_ota_state_t   s_state = TW_OTA_IDLE;
static pal_ota_handle_t s_handle;
static size_t           s_expected_size;
static size_t           s_received;
static uint32_t         s_started_ms;
static uint8_t          s_expected_sha256[PAL_SHA256_DIGEST_SIZE];
static bool             s_has_sha256;
static pal_sha256_ctx_t s_sha_ctx;
static uint32_t         s_expected_block;

/*
 * Accessors are thread-safe: they may be called from the main loop while
 * handlers run in the CoAP poll context.  Handlers themselves are dispatched
 * serially by proto->poll() and do not need to hold s_lock.
 */

tw_ota_state_t tw_ota_state(void)
{
    tw_lock_acquire(s_lock);
    tw_ota_state_t st = s_state;
    tw_lock_release(s_lock);
    return st;
}

uint32_t tw_ota_progress_pct(void)
{
    tw_lock_acquire(s_lock);
    if (s_expected_size == 0 || s_state != TW_OTA_RECEIVING) {
        tw_lock_release(s_lock);
        return 0;
    }
    /* L9 fix: avoid (s_received * 100) overflow on 32-bit size_t. */
    uint32_t pct = (uint32_t)(s_received / (s_expected_size / 100 + 1));
    tw_lock_release(s_lock);
    return pct;
}

/* ---- CoAP resource handlers (auto-registered by the SDK) ---- */

/** Begin OTA: decode OtaBeginCmd protobuf, allocate slot, start receive state. */
tw_err_t svc_ota_on_begin(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    if (s_state == TW_OTA_RECEIVING) {
        PAL_LOGW(TAG, "OTA already in progress, aborting previous");
        pal_ota_abort(s_handle);
        s_handle = NULL;
    }

    s_expected_size = 0;
    s_has_sha256 = false;
    memset(s_expected_sha256, 0, sizeof(s_expected_sha256));

#ifdef CONFIG_TW_USE_PROTOBUF
    if (!req->payload || req->payload_len == 0) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "empty payload");
    }

    tw_OtaBeginCmd msg = tw_OtaBeginCmd_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(req->payload, req->payload_len);
    if (!pb_decode(&stream, tw_OtaBeginCmd_fields, &msg)) {
        PAL_LOGW(TAG, "OtaBeginCmd decode failed: %s", PB_GET_ERROR(&stream));
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "decode error");
    }

    s_expected_size = (size_t)msg.size_bytes;
    if (msg.sha256.size == PAL_SHA256_DIGEST_SIZE) {
        memcpy(s_expected_sha256, msg.sha256.bytes, PAL_SHA256_DIGEST_SIZE);
        s_has_sha256 = true;
    }
#else
    /*
     * C5 fix: copy payload to NUL-terminated buffer before strstr
     * to avoid reading past payload_len.
     */
    if (req->payload && req->payload_len > 0) {
        char hdr[256];
        size_t hlen = req->payload_len < sizeof(hdr) - 1
                      ? req->payload_len : sizeof(hdr) - 1;
        memcpy(hdr, req->payload, hlen);
        hdr[hlen] = '\0';

        const char *p = strstr(hdr, "size=");
        if (p) s_expected_size = (size_t)strtoul(p + 5, NULL, 10);

        const char *h = strstr(hdr, "sha256=");
        if (h) {
            h += 7;
            /* L6: verify enough hex chars exist for a full digest. */
            if (strlen(h) >= PAL_SHA256_DIGEST_SIZE * 2) {
                bool valid = true;
                for (int i = 0; i < PAL_SHA256_DIGEST_SIZE; i++) {
                    unsigned int b;
                    if (sscanf(h + i * 2, "%2x", &b) != 1) {
                        valid = false; break;
                    }
                    s_expected_sha256[i] = (uint8_t)b;
                }
                s_has_sha256 = valid;
            }
        }
    }
#endif

    if (s_expected_size == 0) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "missing size");
    }

    tw_err_t err = pal_ota_begin(&s_handle, s_expected_size);
    if (err != TW_OK) {
        s_state = TW_OTA_ERROR;
        return tw_msg_respond_with_code(resp, TW_MSG_500_INTERNAL, NULL);
    }

    s_state          = TW_OTA_RECEIVING;
    s_received       = 0;
    s_expected_block = 0;
    s_started_ms     = (uint32_t)pal_uptime_ms();
    pal_sha256_init(&s_sha_ctx);

    PAL_LOGI(TAG, "OTA begin: expecting %zu bytes", s_expected_size);
    return tw_msg_respond_with_code(resp, TW_MSG_201_CREATED, NULL);
}

/** Receive one Block1 chunk, validate sequence, and update running hash. */
tw_err_t svc_ota_on_block(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    if (s_state != TW_OTA_RECEIVING || !s_handle) {
        return tw_msg_respond_with_code(resp, TW_MSG_408_INCOMPLETE, "no active OTA");
    }

    /* Binary CoAP Block1 is the only transfer path. */
    if (!req->has_block1) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ,
                                         "Block1 required");
    }

    /* Validate block sequence. */
    if (req->block1_num != s_expected_block) {
        PAL_LOGW(TAG, "OTA block out of order: got %u, expected %u",
                 (unsigned)req->block1_num, (unsigned)s_expected_block);
        return tw_msg_respond_with_code(resp, TW_MSG_408_INCOMPLETE,
                                         "out of order");
    }

    if (!req->payload || req->payload_len == 0) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "empty block");
    }

    /* H4 fix: overflow-safe comparison (s_received <= s_expected_size is an invariant). */
    if (req->payload_len > s_expected_size - s_received) {
        PAL_LOGE(TAG, "OTA block exceeds declared size (%zu + %zu > %zu)",
                 s_received, req->payload_len, s_expected_size);
        pal_ota_abort(s_handle);
        s_handle = NULL;
        s_state  = TW_OTA_ERROR;
        return tw_msg_respond_with_code(resp, TW_MSG_413_TOO_LARGE, NULL);
    }

    tw_err_t err = pal_ota_write(s_handle, req->payload, req->payload_len);
    if (err != TW_OK) {
        PAL_LOGE(TAG, "OTA write failed at offset %zu", s_received);
        pal_ota_abort(s_handle);
        s_handle = NULL;
        s_state  = TW_OTA_ERROR;
        return tw_msg_respond_with_code(resp, TW_MSG_500_INTERNAL, "write failed");
    }

    pal_sha256_update(&s_sha_ctx, req->payload, req->payload_len);
    s_received += req->payload_len;
    s_expected_block++;

    PAL_LOGD(TAG, "OTA block #%u: +%zu (%zu / %zu)",
             (unsigned)req->block1_num, req->payload_len,
             s_received, s_expected_size);

    /* Echo Block1 option (RFC 7959). */
    resp->set_block1  = true;
    resp->block1_num  = req->block1_num;
    resp->block1_more = req->block1_more;
    resp->block1_szx  = req->block1_szx;

    if (req->block1_more) {
        resp->code = TW_MSG_231_CONTINUE;
    } else {
        resp->code = TW_MSG_204_CHANGED;
    }
    return TW_OK;
}

/** Verify image, switch boot partition, and reboot. */
tw_err_t svc_ota_on_commit(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    TW_UNUSED(req);

    if (s_state != TW_OTA_RECEIVING || !s_handle) {
        return tw_msg_respond_with_code(resp, TW_MSG_408_INCOMPLETE, NULL);
    }

    if (s_received < s_expected_size) {
        PAL_LOGW(TAG, "OTA commit with incomplete data: %zu / %zu",
                 s_received, s_expected_size);
        return tw_msg_respond_with_code(resp, TW_MSG_408_INCOMPLETE,
                                         "incomplete transfer");
    }

    s_state = TW_OTA_VERIFYING;
    PAL_LOGI(TAG, "OTA verifying...");

#ifdef CONFIG_TW_OTA_VERIFY_SIGNATURE
    if (s_has_sha256) {
        uint8_t computed[PAL_SHA256_DIGEST_SIZE];
        pal_sha256_finish(&s_sha_ctx, computed);
        if (memcmp(computed, s_expected_sha256, PAL_SHA256_DIGEST_SIZE) != 0) {
            PAL_LOGE(TAG, "OTA SHA-256 mismatch");
            pal_ota_abort(s_handle);
            s_handle = NULL;
            s_state  = TW_OTA_ERROR;
            return tw_msg_respond_with_code(resp, TW_MSG_500_INTERNAL,
                                             "SHA-256 verification failed");
        }
        PAL_LOGI(TAG, "OTA SHA-256 verified OK");
    } else {
        PAL_LOGW(TAG, "no SHA-256 provided, skipping hash verification");
    }
#endif

    tw_err_t err = pal_ota_finish(s_handle);
    s_handle = NULL;

    if (err != TW_OK) {
        s_state = TW_OTA_ERROR;
        PAL_LOGE(TAG, "OTA verify failed");
        return tw_msg_respond_with_code(resp, TW_MSG_500_INTERNAL,
                                         "verification failed");
    }

    err = pal_ota_set_boot_partition();
    if (err != TW_OK) {
        s_state = TW_OTA_ERROR;
        return tw_msg_respond_with_code(resp, TW_MSG_500_INTERNAL,
                                         "set boot partition failed");
    }

    uint32_t elapsed = (uint32_t)pal_uptime_ms() - s_started_ms;
    PAL_LOGI(TAG, "OTA complete: %zu bytes in %lu ms, rebooting...",
             s_received, (unsigned long)elapsed);

    s_state = TW_OTA_REBOOTING;

#ifdef CONFIG_TW_USE_PROTOBUF
    {
        tw_OtaCommitReply reply = tw_OtaCommitReply_init_zero;
        reply.success = true;
        uint8_t rbuf[tw_OtaCommitReply_size];
        pb_ostream_t rs = pb_ostream_from_buffer(rbuf, sizeof(rbuf));
        if (pb_encode(&rs, tw_OtaCommitReply_fields, &reply)) {
            resp->code = TW_MSG_204_CHANGED;
            tw_msg_respond_buf(resp, rbuf, rs.bytes_written);
        } else {
            tw_msg_respond_with_code(resp, TW_MSG_204_CHANGED, "rebooting");
        }
    }
#else
    tw_msg_respond_with_code(resp, TW_MSG_204_CHANGED, "rebooting");
#endif

    pal_sleep_ms(REBOOT_DELAY_MS);
    pal_system_reboot();
    return TW_OK;
}

/** Return current OTA state and progress. */
tw_err_t svc_ota_on_status(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    TW_UNUSED(req);

#ifdef CONFIG_TW_USE_PROTOBUF
    static const char *state_names[] = {
        [TW_OTA_IDLE]           = "idle",
        [TW_OTA_RECEIVING]      = "downloading",
        [TW_OTA_VERIFYING]      = "verifying",
        [TW_OTA_REBOOTING]      = "rebooting",
        [TW_OTA_ERROR]          = "error",
        [TW_OTA_PENDING_VERIFY] = "pending-verify",
    };

    tw_OtaStatus msg = tw_OtaStatus_init_zero;
    const char *sn = (s_state < TW_ARRAY_SIZE(state_names) && state_names[s_state])
                     ? state_names[s_state] : "unknown";
    strncpy(msg.state, sn, sizeof(msg.state) - 1);
    msg.progress_percent = tw_ota_progress_pct();

    uint8_t buf[tw_OtaStatus_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
    if (!pb_encode(&stream, tw_OtaStatus_fields, &msg)) {
        return tw_msg_respond_empty(resp, TW_MSG_500_INTERNAL);
    }

    resp->code = TW_MSG_205_CONTENT;
    return tw_msg_respond_buf(resp, buf, stream.bytes_written);
#else
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "state=%d&progress=%lu&received=%zu&expected=%zu\n",
                     (int)s_state, (unsigned long)tw_ota_progress_pct(),
                     s_received, s_expected_size);
    if (n < 0 || (size_t)n >= sizeof(buf))
        return tw_msg_respond_empty(resp, TW_MSG_500_INTERNAL);
    return tw_msg_respond_text(resp, buf);
#endif
}

/* OTA resource table (appended by svc_device.c at startup). */
tw_msg_resource_t svc_ota_resources[] = {
    { "/tw/ota/begin",  TW_MSG_POST, svc_ota_on_begin },
    { "/tw/ota/block",  TW_MSG_PUT,  svc_ota_on_block },
    { "/tw/ota/commit", TW_MSG_POST, svc_ota_on_commit },
    { "/tw/ota/status", TW_MSG_GET,  svc_ota_on_status },
    TW_MSG_RESOURCE_END
};

void svc_ota_init(void)
{
    tw_lock_init(&s_lock);
    if (pal_ota_is_pending_verify()) {
        s_state = TW_OTA_PENDING_VERIFY;
        PAL_LOGI(TAG, "running newly flashed firmware -- pending verification");
    }
}

void svc_ota_mark_valid(void)
{
    if (s_state == TW_OTA_PENDING_VERIFY) {
        pal_ota_mark_valid();
        s_state = TW_OTA_IDLE;
        PAL_LOGI(TAG, "firmware marked valid");
    }
}
