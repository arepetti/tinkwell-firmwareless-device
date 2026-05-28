/*
 * tw_kvtext.h -- INI-like key=value text format for provisioning and
 *                identity data exchange.
 *
 * Format specification:
 *   - One key=value per line, LF-terminated.
 *   - Maximum line length: 128 bytes (including LF).
 *   - Lines starting with //, #, or ; are comments (ignored).
 *   - Blank lines are ignored.
 *   - Lines starting with [ and ending with ] are ignored (INI groups).
 *   - Key: no spaces, kebab-case (e.g. vendor-id, product-display-name).
 *   - First = is the delimiter; everything after is the value.
 *   - Leading/trailing whitespace on key and value is trimmed.
 *   - No quoting, no escaping.
 *   - Numeric values parsed as integers by the consumer.
 *
 * Standard keys:
 *   uuid                  16 hex-encoded bytes (32 chars)
 *   vendor-id             int32
 *   product-id            int32
 *   vendor-display-name   UTF-8 string
 *   product-display-name  UTF-8 string
 *   variant               uint8
 *   serial-number         int32
 *   device-name           UTF-8 string
 *   fw-version            UTF-8 string
 *   hw-version            UTF-8 string
 *   ssid                  WiFi SSID
 *   password              WiFi password
 *   hub-url               CoAP endpoint URL
 *   psk                   hex-encoded 32 bytes
 *   pubkey                hex-encoded Ed25519 public key
 *   provision-phase       "factory" or "hub"
 *   provision-cmd         "commit" or "reset"
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_KVTEXT_H
#define TW_KVTEXT_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum length of one line in kvtext format, including the line terminator. */
#define TW_KVTEXT_MAX_LINE 128

/* -------------------------------------------------------------------------
 * Parser -- visitor callback for each key=value pair
 * -----------------------------------------------------------------------*/

/**
 * @brief Invoked for each key=value pair discovered during ::tw_kvtext_parse.
 * @param ctx User context from ::tw_kvtext_parse.
 * @param key Trimmed key string (valid only for the duration of this call).
 * @param value Trimmed value string (valid only for the duration of this call).
 */
typedef void (*tw_kvtext_visitor_fn)(void *ctx,
                                    const char *key,
                                    const char *value);

/**
 * @brief Parses a kvtext buffer and invokes @a cb for each valid pair.
 * @param buf Input bytes; need not be NUL-terminated.
 * @param len Length of @a buf in bytes.
 * @param cb Visitor called once per parsed key=value pair.
 * @param ctx Opaque argument passed to @a cb.
 */
void tw_kvtext_parse(const char *buf, size_t len,
                     tw_kvtext_visitor_fn cb, void *ctx);

/* -------------------------------------------------------------------------
 * Writer -- append key=value lines to a buffer
 * -------------------------------------------------------------------------*/

/**
 * @brief Appends a key=value line terminated by LF at @a *pos and advances @a *pos.
 * @param buf Output buffer.
 * @param cap Total size of @a buf.
 * @param pos In/out cursor; on success, advanced past the appended line.
 * @param key Key string (no embedded newline).
 * @param value Value string (no embedded newline).
 * @retval TW_OK on success.
 * @retval TW_ERR_OVERFLOW if the line does not fit in the remaining capacity.
 */
tw_err_t tw_kvtext_write_str(char *buf, size_t cap, size_t *pos,
                             const char *key, const char *value);

/**
 * @brief Appends a line with a decimal integer value.
 * @param buf Output buffer.
 * @param cap Total size of @a buf.
 * @param pos In/out cursor.
 * @param key Key string.
 * @param value Integer rendered in decimal.
 * @retval TW_OK on success.
 * @retval TW_ERR_OVERFLOW if the line does not fit.
 */
tw_err_t tw_kvtext_write_i32(char *buf, size_t cap, size_t *pos,
                             const char *key, int32_t value);

/**
 * @brief Appends a line with an unsigned 8-bit value in decimal.
 * @param buf Output buffer.
 * @param cap Total size of @a buf.
 * @param pos In/out cursor.
 * @param key Key string.
 * @param value 0..255 rendered in decimal.
 * @retval TW_OK on success.
 * @retval TW_ERR_OVERFLOW if the line does not fit.
 */
tw_err_t tw_kvtext_write_u8(char *buf, size_t cap, size_t *pos,
                            const char *key, uint8_t value);

/**
 * @brief Appends a line whose value is hex-encoded binary (lowercase hex, no prefix).
 * @param buf Output buffer.
 * @param cap Total size of @a buf.
 * @param pos In/out cursor.
 * @param key Key string.
 * @param data Bytes to encode.
 * @param data_len Number of bytes in @a data.
 * @retval TW_OK on success.
 * @retval TW_ERR_OVERFLOW if the encoded line does not fit.
 */
tw_err_t tw_kvtext_write_hex(char *buf, size_t cap, size_t *pos,
                             const char *key,
                             const uint8_t *data, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* TW_KVTEXT_H */
