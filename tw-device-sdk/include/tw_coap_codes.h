/*
 * tw_coap_codes.h -- CoAP protocol constants for the TW Device SDK.
 *
 * Named constants for CoAP response codes, port assignments, and
 * code-byte encoding.  All values reference the relevant IETF RFCs
 * and are prefixed TW_ for use across compilation units and in
 * application code.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_COAP_CODES_H
#define TW_COAP_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Port assignments -- RFC 7252 section 12.2
 * -----------------------------------------------------------------------*/

/** @brief Default CoAP UDP port (IANA "coap").  RFC 7252 section 12.2. */
#define TW_COAP_DEFAULT_PORT    5683

/** @brief Default CoAP-over-DTLS UDP port (IANA "coaps").  RFC 7252 section 12.2. */
#define TW_COAPS_DEFAULT_PORT   5684

/* -------------------------------------------------------------------------
 * Response code encoding -- RFC 7252 section 3
 *
 * A CoAP response code is a single byte with:
 *   - bits 7..5: class  (3 bits, 0..7)
 *   - bits 4..0: detail (5 bits, 0..31)
 * The human-readable form is "class.detail", e.g. 2.05 Content.
 * -----------------------------------------------------------------------*/

/** @brief Number of bits to shift for the code class.  RFC 7252 section 3. */
#define TW_COAP_CODE_CLASS_SHIFT    5

/** @brief Mask for the 3-bit class after shifting.  RFC 7252 section 3. */
#define TW_COAP_CODE_CLASS_MASK     0x07u

/** @brief Mask for the 5-bit detail (low bits).  RFC 7252 section 3. */
#define TW_COAP_CODE_DETAIL_MASK    0x1Fu

/** @brief Encode a CoAP class.detail pair into a single code byte. */
#define TW_COAP_CODE(cls, detail)   ((uint8_t)(((cls) << 5) | (detail)))

/* -------------------------------------------------------------------------
 * Success codes (class 2) -- RFC 7252 section 5.9.1
 * -----------------------------------------------------------------------*/

/** @brief 2.01 Created.  RFC 7252 section 5.9.1.1. */
#define TW_COAP_201_CREATED     TW_COAP_CODE(2, 1)

/** @brief 2.02 Deleted.  RFC 7252 section 5.9.1.2. */
#define TW_COAP_202_DELETED     TW_COAP_CODE(2, 2)

/** @brief 2.04 Changed.  RFC 7252 section 5.9.1.4. */
#define TW_COAP_204_CHANGED     TW_COAP_CODE(2, 4)

/** @brief 2.05 Content.  RFC 7252 section 5.9.1.5. */
#define TW_COAP_205_CONTENT     TW_COAP_CODE(2, 5)

/** @brief 2.31 Continue.  RFC 7959 section 2.9 (block-wise transfers). */
#define TW_COAP_231_CONTINUE    TW_COAP_CODE(2, 31)

/* -------------------------------------------------------------------------
 * Client error codes (class 4) -- RFC 7252 section 5.9.2
 * -----------------------------------------------------------------------*/

/** @brief 4.00 Bad Request.  RFC 7252 section 5.9.2.1. */
#define TW_COAP_400_BAD_REQUEST         TW_COAP_CODE(4, 0)

/** @brief 4.04 Not Found.  RFC 7252 section 5.9.2.5. */
#define TW_COAP_404_NOT_FOUND           TW_COAP_CODE(4, 4)

/** @brief 4.05 Method Not Allowed.  RFC 7252 section 5.9.2.6. */
#define TW_COAP_405_NOT_ALLOWED         TW_COAP_CODE(4, 5)

/** @brief 4.08 Request Entity Incomplete.  RFC 7959 section 2.9. */
#define TW_COAP_408_INCOMPLETE          TW_COAP_CODE(4, 8)

/** @brief 4.13 Request Entity Too Large.  RFC 7252 section 5.9.2.14. */
#define TW_COAP_413_TOO_LARGE           TW_COAP_CODE(4, 13)

/* -------------------------------------------------------------------------
 * Server error codes (class 5) -- RFC 7252 section 5.9.3
 * -----------------------------------------------------------------------*/

/** @brief 5.00 Internal Server Error.  RFC 7252 section 5.9.3.1. */
#define TW_COAP_500_INTERNAL            TW_COAP_CODE(5, 0)

/* -------------------------------------------------------------------------
 * CoAP option numbers -- RFC 7252 section 5.10, RFC 7959 section 2.1
 * -----------------------------------------------------------------------*/

/** @brief CoAP option number for Block2 (RFC 7959 section 2.1). */
#define TW_COAP_OPT_BLOCK2         23
/** @brief CoAP option number for Block1 (RFC 7959 section 2.1). */
#define TW_COAP_OPT_BLOCK1         27

/* -------------------------------------------------------------------------
 * CoAP URI helpers
 * -----------------------------------------------------------------------*/

/** @brief Length of the URI scheme prefix "coap://".  RFC 7252 section 6. */
#define TW_COAP_URI_SCHEME_LEN  7

#ifdef __cplusplus
}
#endif

#endif /* TW_COAP_CODES_H */
