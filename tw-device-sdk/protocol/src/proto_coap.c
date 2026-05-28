/*
 * proto_coap.c -- Binary CoAP (RFC 7252) protocol backend using libcoap.
 *
 * Provides:
 *   - tw_msg_respond_* response helpers (protocol-neutral API)
 *   - tw_protocol_t vtable for CoAP (init, poll, deinit, send)
 *   - Block1 (RFC 7959) option extraction and echo
 *
 * libcoap handles message serialization, deduplication, retransmission,
 * and Block2 (server-side) transparently.  Block1 metadata is extracted
 * from inbound requests and passed through tw_msg_request_t so service
 * handlers can validate block sequence themselves.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_msg.h"
#include "pal_log.h"
#include "tw_coap_codes.h"

#include <coap3/coap.h>
#include <string.h>
#include <stdio.h>

#define TAG "coap"

/* RFC 7252 section 12.1.2 -- method codes as used in the CoAP header. */
#define COAP_METHOD_GET    1
#define COAP_METHOD_POST   2
#define COAP_METHOD_PUT    3
#define COAP_METHOD_DELETE 4

/* Response payload scratch buffer for handlers. */
#define RESP_BUF_SIZE 1024

/* Client-side response timeout for outbound requests (heartbeat, etc.). */
#define CLIENT_TIMEOUT_MS 5000

/* -----------------------------------------------------------------------
 * Response helpers (protocol-neutral API, used by all service handlers)
 * ---------------------------------------------------------------------*/

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

/* -----------------------------------------------------------------------
 * libcoap server state
 * ---------------------------------------------------------------------*/

typedef struct {
    tw_msg_resource_t *resources;
    coap_context_t    *ctx;
    coap_endpoint_t   *ep;
} coap_server_t;

static coap_server_t server;

/** Map a libcoap request code to the TW_MSG_* method bitmask. */
static uint8_t method_from_pdu(coap_pdu_code_t code)
{
    switch (code) {
    case COAP_REQUEST_CODE_GET:    return TW_MSG_GET;
    case COAP_REQUEST_CODE_POST:   return TW_MSG_POST;
    case COAP_REQUEST_CODE_PUT:    return TW_MSG_PUT;
    case COAP_REQUEST_CODE_DELETE: return TW_MSG_DELETE;
    default:                       return 0;
    }
}

/** Extract the full URI path from a CoAP PDU into a static buffer. */
static const char *extract_path(const coap_pdu_t *pdu,
                                char *buf, size_t buf_size)
{
    coap_opt_iterator_t oi;
    coap_opt_t *opt;
    size_t pos = 0;

    coap_option_iterator_init(pdu, &oi, COAP_OPT_ALL);
    while ((opt = coap_option_next(&oi)) != NULL) {
        if (oi.number != COAP_OPTION_URI_PATH)
            continue;
        size_t seg_len = coap_opt_length(opt);
        const uint8_t *seg_val = coap_opt_value(opt);
        if (pos + 1 + seg_len >= buf_size)
            break;
        buf[pos++] = '/';
        memcpy(buf + pos, seg_val, seg_len);
        pos += seg_len;
    }
    if (pos == 0 && buf_size > 1) {
        buf[0] = '/';
        pos = 1;
    }
    buf[pos] = '\0';
    return buf;
}

/** Find a registered resource matching @a path and @a method, or NULL. */
static tw_msg_resource_t *find_resource(const char *path, uint8_t method)
{
    for (tw_msg_resource_t *r = server.resources; r->path; r++) {
        if (strcmp(r->path, path) == 0 && (r->methods & method))
            return r;
    }
    return NULL;
}

/**
 * Generic libcoap handler installed for every registered resource.
 * Bridges between the libcoap PDU model and the tw_msg_* handler API.
 */
static void coap_handler(coap_resource_t *resource,
                         coap_session_t *session,
                         const coap_pdu_t *request,
                         const coap_string_t *query,
                         coap_pdu_t *response)
{
    (void)resource;
    (void)session;

    uint8_t method = method_from_pdu(coap_pdu_get_code(request));
    char path_buf[128];
    const char *path = extract_path(request, path_buf, sizeof(path_buf));

    tw_msg_resource_t *res = find_resource(path, method);
    if (!res) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_NOT_FOUND);
        return;
    }

    /* Extract request payload. */
    const uint8_t *payload = NULL;
    size_t payload_len = 0;
    coap_get_data(request, &payload_len, &payload);

    /* Extract Block1 option (RFC 7959). */
    coap_opt_t *block1_opt = NULL;
    coap_opt_iterator_t oi;
    coap_option_iterator_init(request, &oi, COAP_OPT_ALL);
    coap_opt_t *o;
    while ((o = coap_option_next(&oi)) != NULL) {
        if (oi.number == TW_COAP_OPT_BLOCK1) {
            block1_opt = o;
            break;
        }
    }

    /* Build the tw_msg_request_t. */
    tw_msg_request_t req = {
        .method      = method,
        .path        = path,
        .payload     = payload,
        .payload_len = payload_len,
        .query       = query ? (const char *)query->s : NULL,
    };

    if (block1_opt) {
        /*
         * RFC 7959 section 2.2: Block1 option is encoded as a variable-length
         * integer.  Bits: NUM (upper bits), M (bit 3), SZX (bits 2..0).
         */
        unsigned int block1_val = coap_decode_var_bytes(
            coap_opt_value(block1_opt), coap_opt_length(block1_opt));
        req.has_block1  = true;
        req.block1_num  = block1_val >> 4;
        req.block1_more = (block1_val >> 3) & 1;
        req.block1_szx  = block1_val & 0x07;
    }

    /* Invoke the handler. */
    uint8_t resp_buf[RESP_BUF_SIZE];
    tw_msg_response_t resp = {
        .payload          = resp_buf,
        .payload_capacity = sizeof(resp_buf),
    };

    res->handler(&req, &resp);

    /* Set the response code from the handler's tw_msg code. */
    coap_pdu_set_code(response,
                      COAP_RESPONSE_CODE(
                          (resp.code >> TW_COAP_CODE_CLASS_SHIFT) & TW_COAP_CODE_CLASS_MASK,
                          resp.code & TW_COAP_CODE_DETAIL_MASK));

    /* Echo Block1 option if the handler requested it. */
    if (resp.set_block1) {
        unsigned int block1_val =
            (resp.block1_num << 4) |
            ((resp.block1_more ? 1u : 0u) << 3) |
            (resp.block1_szx & 0x07);
        uint8_t enc[4];
        unsigned int enc_len = coap_encode_var_safe(enc, sizeof(enc), block1_val);
        coap_add_option(response, TW_COAP_OPT_BLOCK1, enc_len, enc);
    }

    /* Add response payload if any. */
    if (resp.payload_len > 0) {
        coap_add_data(response, resp.payload_len, resp.payload);
    }
}

/* ---- vtable implementation ---- */

static tw_err_t coap_init_fn(tw_protocol_t *self,
                              tw_msg_resource_t *resources, uint16_t port)
{
    (void)self;

    coap_startup();
    server.resources = resources;

    server.ctx = coap_new_context(NULL);
    if (!server.ctx) {
        PAL_LOGE(TAG, "coap_new_context() failed");
        return TW_ERR_IO;
    }

    /* Bind to all interfaces on the given UDP port. */
    coap_address_t addr;
    coap_address_init(&addr);
    addr.addr.sin.sin_family = AF_INET;
    addr.addr.sin.sin_port = htons(port);
    addr.addr.sin.sin_addr.s_addr = INADDR_ANY;

    server.ep = coap_new_endpoint(server.ctx, &addr, COAP_PROTO_UDP);
    if (!server.ep) {
        PAL_LOGE(TAG, "coap_new_endpoint() failed on :%u", port);
        coap_free_context(server.ctx);
        server.ctx = NULL;
        return TW_ERR_IO;
    }

    /*
     * Register each tw_msg_resource_t as a libcoap resource.
     * All use the same generic handler that bridges to tw_msg_handler_t.
     */
    for (tw_msg_resource_t *r = resources; r->path; r++) {
        coap_str_const_t *uri = coap_make_str_const(r->path + 1);
        coap_resource_t *cr = coap_resource_init(uri, 0);
        if (!cr) continue;

        if (r->methods & TW_MSG_GET)
            coap_register_handler(cr, COAP_REQUEST_GET, coap_handler);
        if (r->methods & TW_MSG_POST)
            coap_register_handler(cr, COAP_REQUEST_POST, coap_handler);
        if (r->methods & TW_MSG_PUT)
            coap_register_handler(cr, COAP_REQUEST_PUT, coap_handler);
        if (r->methods & TW_MSG_DELETE)
            coap_register_handler(cr, COAP_REQUEST_DELETE, coap_handler);

        coap_add_resource(server.ctx, cr);
    }

    PAL_LOGI(TAG, "CoAP server (RFC 7252/libcoap) listening on :%u", port);
    return TW_OK;
}

static void coap_poll_fn(tw_protocol_t *self, int timeout_ms)
{
    (void)self;
    if (!server.ctx) return;
    coap_io_process(server.ctx, (uint32_t)timeout_ms);
}

static void coap_deinit_fn(tw_protocol_t *self)
{
    (void)self;
    if (server.ctx) {
        coap_free_context(server.ctx);
        server.ctx = NULL;
        server.ep  = NULL;
    }
    coap_cleanup();
}

/*
 * Client-side state for the synchronous send path (heartbeat, etc.).
 * A response callback captures the reply payload so coap_send_fn can
 * return it to the caller.
 */
typedef struct {
    uint8_t *resp_buf;
    size_t   resp_buf_size;
    size_t  *resp_len;
    bool     done;
} send_ctx_t;

static coap_response_t send_response_handler(coap_session_t *session,
                                              const coap_pdu_t *sent,
                                              const coap_pdu_t *received,
                                              const coap_mid_t mid)
{
    (void)session;
    (void)sent;
    (void)mid;

    send_ctx_t *sctx = (send_ctx_t *)coap_get_app_data(
        coap_session_get_context(session));
    if (!sctx) return COAP_RESPONSE_OK;

    const uint8_t *data = NULL;
    size_t data_len = 0;
    coap_get_data(received, &data_len, &data);

    if (sctx->resp_buf && data_len > 0) {
        size_t copy = data_len < sctx->resp_buf_size
                          ? data_len : sctx->resp_buf_size;
        memcpy(sctx->resp_buf, data, copy);
        if (sctx->resp_len) *sctx->resp_len = copy;
    }
    sctx->done = true;
    return COAP_RESPONSE_OK;
}

/**
 * Send a CoAP CON POST to the hub and block until a response arrives
 * or timeout_ms elapses.
 */
static tw_err_t coap_send_fn(tw_protocol_t *self,
                              const char *host, uint16_t port,
                              const char *path,
                              const uint8_t *payload, size_t payload_len,
                              uint8_t *resp_buf, size_t resp_buf_size,
                              size_t *resp_len, int timeout_ms)
{
    (void)self;

    if (resp_len) *resp_len = 0;

    coap_context_t *ctx = coap_new_context(NULL);
    if (!ctx) return TW_ERR_IO;

    send_ctx_t sctx = {
        .resp_buf      = resp_buf,
        .resp_buf_size = resp_buf_size,
        .resp_len      = resp_len,
        .done          = false,
    };
    coap_set_app_data(ctx, &sctx);

    /* Resolve destination address. */
    coap_address_t dst;
    coap_address_init(&dst);
    dst.addr.sin.sin_family = AF_INET;
    dst.addr.sin.sin_port = htons(port);

    /* Simple numeric IPv4 resolution. */
    struct in_addr in;
    if (inet_pton(AF_INET, host, &in) == 1) {
        dst.addr.sin.sin_addr = in;
    } else {
        PAL_LOGW(TAG, "cannot resolve host: %s", host);
        coap_free_context(ctx);
        return TW_ERR_IO;
    }

    coap_session_t *session = coap_new_client_session(
        ctx, NULL, &dst, COAP_PROTO_UDP);
    if (!session) {
        coap_free_context(ctx);
        return TW_ERR_IO;
    }

    coap_register_response_handler(ctx, send_response_handler);

    /* Build a CON POST PDU. */
    coap_pdu_t *pdu = coap_new_pdu(COAP_MESSAGE_CON, COAP_REQUEST_CODE_POST,
                                    session);
    if (!pdu) {
        coap_session_release(session);
        coap_free_context(ctx);
        return TW_ERR_IO;
    }

    /* Add URI-Path options (split on '/'). */
    const char *p = path;
    if (*p == '/') p++;
    while (*p) {
        const char *seg_end = strchr(p, '/');
        size_t seg_len = seg_end ? (size_t)(seg_end - p) : strlen(p);
        coap_add_option(pdu, COAP_OPTION_URI_PATH, seg_len, (const uint8_t *)p);
        p += seg_len;
        if (*p == '/') p++;
    }

    if (payload && payload_len > 0)
        coap_add_data(pdu, payload_len, payload);

    coap_mid_t mid = coap_send(session, pdu);
    if (mid == COAP_INVALID_MID) {
        coap_session_release(session);
        coap_free_context(ctx);
        return TW_ERR_IO;
    }

    /* Poll until response or timeout. */
    int remaining = timeout_ms > 0 ? timeout_ms : CLIENT_TIMEOUT_MS;
    while (!sctx.done && remaining > 0) {
        int step = remaining < 100 ? remaining : 100;
        coap_io_process(ctx, (uint32_t)step);
        remaining -= step;
    }

    coap_session_release(session);
    coap_free_context(ctx);

    return sctx.done ? TW_OK : TW_ERR_TIMEOUT;
}

/* ---- Singleton ---- */

static tw_protocol_t s_coap_protocol = {
    .init   = coap_init_fn,
    .poll   = coap_poll_fn,
    .deinit = coap_deinit_fn,
    .send   = coap_send_fn,
};

#ifdef CONFIG_TW_PROTOCOL_COAP
tw_protocol_t *tw_protocol_create(void) { return &s_coap_protocol; }
#elif !defined(CONFIG_TW_PROTOCOL_MQTT)
/* Default when no Kconfig selection (POSIX builds without Kconfig). */
tw_protocol_t *tw_protocol_create(void) { return &s_coap_protocol; }
#endif
