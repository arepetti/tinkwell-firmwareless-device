/*
 * tw_msg.h -- Protocol-neutral messaging types and protocol abstraction.
 *
 * This header defines the request/response/resource types used by all
 * SDK services and application handlers.  The underlying wire protocol
 * (CoAP, MQTT, ...) is selected at compile time via Kconfig and
 * implemented behind the tw_protocol_t vtable.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_MSG_H
#define TW_MSG_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Method flags (combinable bitmask)
 * -----------------------------------------------------------------------*/

/** @brief Bit flag: resource may be read (maps to GET-style semantics on the wire). */
#define TW_MSG_GET    (1u << 0)
/** @brief Bit flag: resource may accept creates/submissions (maps to POST-style semantics). */
#define TW_MSG_POST   (1u << 1)
/** @brief Bit flag: resource may be replaced (maps to PUT-style semantics). */
#define TW_MSG_PUT    (1u << 2)
/** @brief Bit flag: resource may be removed (maps to DELETE-style semantics). */
#define TW_MSG_DELETE (1u << 3)

/* -------------------------------------------------------------------------
 * Request / Response
 * -----------------------------------------------------------------------*/

/**
 * @brief Inbound request presented to a ::tw_msg_handler_t.
 *
 * The protocol backend fills Block1 fields when a request carries an
 * RFC 7959 Block1 option.  Zero-initialized struct means "no block transfer".
 */
typedef struct {
    uint8_t         method;       /**< Bitmask of ::TW_MSG_* flags describing allowed/used methods. */
    const char     *path;         /**< Resource path (application-defined URI path string). */
    const uint8_t  *payload;      /**< Request body bytes, or NULL if empty. */
    size_t          payload_len;  /**< Length of @a payload in bytes. */
    const char     *query;        /**< Query string after '?', or NULL if none. */

    /* Block1 (RFC 7959) -- populated by the protocol backend when present. */
    bool            has_block1;   /**< True when the request carried a Block1 option. */
    uint32_t        block1_num;   /**< Block sequence number (0-based). */
    bool            block1_more;  /**< True if more blocks follow this one. */
    uint16_t        block1_szx;   /**< Block size exponent (4..10 => 16..1024 bytes). */
} tw_msg_request_t;

/**
 * @brief Outbound response produced by a ::tw_msg_handler_t.
 *
 * Handlers that process Block1 requests should set the @c set_block1 flag
 * and echo the block metadata so the protocol backend can encode the Block1
 * option in the response.
 */
typedef struct {
    uint8_t   code;               /**< Response code; numeric values align with RFC 7252 CoAP codes. */
    uint8_t  *payload;            /**< Response body buffer owned or supplied by the handler. */
    size_t    payload_len;        /**< Length of valid data in @a payload. */
    size_t    payload_capacity;   /**< Size of the @a payload buffer for append helpers. */

    /* Block1 echo -- handler sets these for block-at-a-time responses. */
    bool      set_block1;         /**< True to include a Block1 option in the wire response. */
    uint32_t  block1_num;         /**< Block sequence number to echo (matches request). */
    bool      block1_more;        /**< Echo of the "more" flag from the request. */
    uint16_t  block1_szx;         /**< Echo of the block size exponent from the request. */
} tw_msg_response_t;

/*
 * Response codes: numeric values match RFC 7252 (CoAP) so backends can map
 * directly without translation. Class 2.xx success, 4.xx client error, 5.xx server error.
 */
/** @brief RFC 7252 section 5.9.1.1 -- 2.01 Created (0x41). */
#define TW_MSG_201_CREATED     0x41
/** @brief RFC 7252 section 5.9.1.2 -- 2.02 Deleted (0x42). */
#define TW_MSG_202_DELETED     0x42
/** @brief RFC 7252 section 5.9.1.4 -- 2.04 Changed (0x44). */
#define TW_MSG_204_CHANGED     0x44
/** @brief RFC 7252 section 5.9.1.5 -- 2.05 Content (0x45). */
#define TW_MSG_205_CONTENT     0x45
/** @brief RFC 7959 section 2.9 -- 2.31 Continue (0x5F). */
#define TW_MSG_231_CONTINUE    0x5F
/** @brief RFC 7252 section 5.9.2.1 -- 4.00 Bad Request (0x80). */
#define TW_MSG_400_BAD_REQ     0x80
/** @brief RFC 7252 section 5.9.2.4 -- 4.03 Forbidden (0x83). */
#define TW_MSG_403_FORBIDDEN   0x83
/** @brief RFC 7252 section 5.9.2.5 -- 4.04 Not Found (0x84). */
#define TW_MSG_404_NOT_FOUND   0x84
/** @brief RFC 7252 section 5.9.2.6 -- 4.05 Method Not Allowed (0x85). */
#define TW_MSG_405_NOT_ALLOWED 0x85
/** @brief RFC 7959 section 2.9 -- 4.08 Request Entity Incomplete (0x88). */
#define TW_MSG_408_INCOMPLETE  0x88
/** @brief RFC 7252 section 5.9.2.14 -- 4.13 Request Entity Too Large (0x8D). */
#define TW_MSG_413_TOO_LARGE   0x8D
/** @brief RFC 7252 section 5.9.3.1 -- 5.00 Internal Server Error (0xA0). */
#define TW_MSG_500_INTERNAL    0xA0

/* -------------------------------------------------------------------------
 * Response helpers
 * -----------------------------------------------------------------------*/

/**
 * @brief Sets @a resp->code and optionally writes a diagnostic text payload.
 *
 * This is the primary response helper for binary CoAP.  The protocol backend
 * encodes @a code in the CoAP header; @a diagnostic (if non-NULL) is sent as
 * a text/plain payload for debugging.
 *
 * @param resp        Response object to fill.
 * @param code        RFC 7252 response code (e.g. ::TW_MSG_201_CREATED).
 * @param diagnostic  Optional NUL-terminated explanation string, or NULL.
 * @retval TW_OK on success.
 * @retval TW_ERR_OVERFLOW if @a diagnostic does not fit in @a resp.
 */
tw_err_t tw_msg_respond_with_code(tw_msg_response_t *resp,
                                   uint8_t code,
                                   const char *diagnostic);

/**
 * @brief Writes a UTF-8 text body and sets ::TW_MSG_205_CONTENT as code.
 * @param resp Response object to fill; @a payload_capacity must allow the encoded text.
 * @param text NUL-terminated string to send as the response body.
 * @retval TW_OK on success.
 * @retval TW_ERR_OVERFLOW if the text does not fit in @a resp.
 */
tw_err_t tw_msg_respond_text(tw_msg_response_t *resp, const char *text);

/**
 * @brief Encodes a signed 32-bit integer as text in the response body.
 * @param resp Response object to fill.
 * @param value Integer to serialize into @a resp.
 * @retval TW_OK on success.
 * @retval TW_ERR_OVERFLOW if the encoded form does not fit.
 */
tw_err_t tw_msg_respond_i32(tw_msg_response_t *resp, int32_t value);

/**
 * @brief Copies raw bytes into the response payload.
 * @param resp Response object to fill.
 * @param data Bytes to copy.
 * @param len Number of bytes to copy.
 * @retval TW_OK on success.
 * @retval TW_ERR_OVERFLOW if @a len exceeds @a payload_capacity.
 */
tw_err_t tw_msg_respond_buf(tw_msg_response_t *resp,
                            const void *data, size_t len);

/**
 * @brief Sends a response with no body, only a status code.
 * @param resp Response object whose code is set to @a code.
 * @param code RFC 7252-style response code (e.g. ::TW_MSG_204_CHANGED).
 * @retval TW_OK on success.
 */
tw_err_t tw_msg_respond_empty(tw_msg_response_t *resp, uint8_t code);

/* -------------------------------------------------------------------------
 * Resource definition
 * -------------------------------------------------------------------------*/

/**
 * @brief Application callback that handles one matched resource request.
 * @param req Inbound request for this resource.
 * @param resp Response object to fill (RFC 7252-mapped @a code and optional body).
 * @retval TW_OK after a successful response build.
 * @retval Other ::tw_err_t to signal handler-side failure to the stack.
 */
typedef tw_err_t (*tw_msg_handler_t)(tw_msg_request_t *req,
                                     tw_msg_response_t *resp);

/** @brief Static table entry registering a path, allowed methods, and handler. */
typedef struct {
    const char       *path;    /**< URI path prefix or exact path for this resource. */
    uint8_t           methods; /**< Bitmask of ::TW_MSG_* flags this handler supports. */
    tw_msg_handler_t  handler; /**< Called when a request matches @a path and method. */
} tw_msg_resource_t;

/** @brief Sentinel that terminates a ::tw_msg_resource_t array (NULL path, no handler). */
#define TW_MSG_RESOURCE_END  { NULL, 0, NULL }

/* -------------------------------------------------------------------------
 * Protocol abstraction vtable
 * -------------------------------------------------------------------------*/

struct tw_protocol;

/**
 * @brief Protocol backend: initialization, polling, teardown, and hub-directed requests.
 *
 * Implementations are selected at build time; the device core uses this vtable so
 * application code stays independent of CoAP versus other transports.
 */
typedef struct tw_protocol {
    /**
     * @brief Starts the protocol stack and registers the resource table.
     * @param self Protocol instance.
     * @param resources NULL-terminated array of ::tw_msg_resource_t ending with ::TW_MSG_RESOURCE_END.
     * @param port Local listen port for inbound requests (meaning depends on backend).
     * @retval TW_OK if the stack is ready to accept traffic.
     * @retval Other ::tw_err_t on bind, socket, or resource registration failure.
     */
    tw_err_t (*init)(struct tw_protocol *self,
                     tw_msg_resource_t *resources, uint16_t port);

    /**
     * @brief Runs protocol maintenance and dispatches I/O for up to @a timeout_ms.
     * @param self Protocol instance.
     * @param timeout_ms Maximum time to block waiting for work, in milliseconds.
     */
    void (*poll)(struct tw_protocol *self, int timeout_ms);

    /**
     * @brief Releases protocol resources and stops listening.
     * @param self Protocol instance.
     */
    void (*deinit)(struct tw_protocol *self);

    /**
     * @brief Sends a request to the hub and optionally receives a response (heartbeat, outbound RPC).
     * @param self Protocol instance.
     * @param host Hub hostname or address string.
     * @param port Hub port number.
     * @param path Request path on the hub.
     * @param payload Request body bytes, or NULL.
     * @param payload_len Length of @a payload.
     * @param resp_buf Buffer for optional response body.
     * @param resp_buf_size Size of @a resp_buf.
     * @param resp_len On success, set to the number of bytes written to @a resp_buf.
     * @param timeout_ms Overall transaction timeout in milliseconds.
     * @retval TW_OK if the exchange completed per backend rules.
     * @retval Other ::tw_err_t codes on timeout, refusal, or transport failure.
     */
    tw_err_t (*send)(struct tw_protocol *self,
                     const char *host, uint16_t port,
                     const char *path,
                     const uint8_t *payload, size_t payload_len,
                     uint8_t *resp_buf, size_t resp_buf_size,
                     size_t *resp_len, int timeout_ms);
} tw_protocol_t;

/**
 * @brief Returns the compile-time selected protocol implementation (singleton).
 * @return Pointer to a static ::tw_protocol_t; must not be freed.
 */
tw_protocol_t *tw_protocol_create(void);

#ifdef __cplusplus
}
#endif

#endif /* TW_MSG_H */
