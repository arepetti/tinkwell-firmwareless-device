/*
 * tw_kvtext.c -- INI-like key=value text format parser and writer.
 *
 * Pure C, no PAL dependencies, fully unit-testable.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_kvtext.h"
#include <string.h>
#include <stdio.h>

/* ---- Helpers ---- */

/** Advances past ASCII space and tab so key/value parsing ignores leading whitespace. */
static const char *skip_space(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    return p;
}

/** Shrinks the end pointer past trailing whitespace and line endings for stable token bounds. */
static const char *rtrim(const char *start, const char *end)
{
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' ||
                           end[-1] == '\r' || end[-1] == '\n'))
        end--;
    return end;
}

/* ---- Parser ---- */

/**
 * Line-oriented scan: invokes the visitor for each key=value pair, skipping blanks,
 * comments, and INI-style section headers so a single buffer can back config or RPC text.
 */
void tw_kvtext_parse(const char *buf, size_t len,
                     tw_kvtext_visitor_fn cb, void *ctx)
{
    if (!buf || !cb) return;

    const char *p   = buf;
    const char *end = buf + len;

    char key_buf[TW_KVTEXT_MAX_LINE];
    char val_buf[TW_KVTEXT_MAX_LINE];

    while (p < end) {
        const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
        const char *line_end = nl ? nl : end;

        const char *ls = skip_space(p, line_end);
        const char *le = rtrim(ls, line_end);
        size_t line_len = (size_t)(le - ls);

        p = nl ? nl + 1 : end;

        if (line_len == 0)
            continue;

        if (ls[0] == '#' || ls[0] == ';')
            continue;

        if (line_len >= 2 && ls[0] == '/' && ls[1] == '/')
            continue;

        if (ls[0] == '[' && le[-1] == ']')
            continue;

        const char *eq = (const char *)memchr(ls, '=', line_len);
        if (!eq)
            continue;

        const char *key_start = ls;
        const char *key_end   = rtrim(key_start, eq);
        size_t klen = (size_t)(key_end - key_start);
        if (klen == 0 || klen >= sizeof(key_buf))
            continue;

        const char *val_start = skip_space(eq + 1, le);
        const char *val_end   = le;
        size_t vlen = (size_t)(val_end - val_start);
        if (vlen >= sizeof(val_buf))
            vlen = sizeof(val_buf) - 1;

        memcpy(key_buf, key_start, klen);
        key_buf[klen] = '\0';

        memcpy(val_buf, val_start, vlen);
        val_buf[vlen] = '\0';

        cb(ctx, key_buf, val_buf);
    }
}

/* ---- Writer ---- */

/** Appends a key=value line with newline; grows *pos for incremental buffer assembly. */
tw_err_t tw_kvtext_write_str(char *buf, size_t cap, size_t *pos,
                             const char *key, const char *value)
{
    if (!buf || !pos || !key || !value) return TW_ERR_INVAL;

    int n = snprintf(buf + *pos, cap - *pos, "%s=%s\n", key, value);
    if (n < 0 || (size_t)n >= cap - *pos)
        return TW_ERR_OVERFLOW;
    *pos += (size_t)n;
    return TW_OK;
}

/** Appends a signed decimal value as key=value text (portable wire format for integers). */
tw_err_t tw_kvtext_write_i32(char *buf, size_t cap, size_t *pos,
                             const char *key, int32_t value)
{
    if (!buf || !pos || !key) return TW_ERR_INVAL;

    int n = snprintf(buf + *pos, cap - *pos, "%s=%d\n", key, (int)value);
    if (n < 0 || (size_t)n >= cap - *pos)
        return TW_ERR_OVERFLOW;
    *pos += (size_t)n;
    return TW_OK;
}

/** Convenience wrapper for 8-bit fields without duplicating snprintf call sites. */
tw_err_t tw_kvtext_write_u8(char *buf, size_t cap, size_t *pos,
                            const char *key, uint8_t value)
{
    return tw_kvtext_write_i32(buf, cap, pos, key, (int32_t)value);
}

/** Writes binary blobs as lowercase hex (no separators) so opaque keys fit the text format. */
tw_err_t tw_kvtext_write_hex(char *buf, size_t cap, size_t *pos,
                             const char *key,
                             const uint8_t *data, size_t data_len)
{
    if (!buf || !pos || !key || !data) return TW_ERR_INVAL;

    size_t klen = strlen(key);
    size_t needed = klen + 1 + data_len * 2 + 1;  /* key=hex\n */
    if (*pos + needed > cap)
        return TW_ERR_OVERFLOW;

    static const char hx[] = "0123456789abcdef";
    char *p = buf + *pos;

    memcpy(p, key, klen);
    p += klen;
    *p++ = '=';

    for (size_t i = 0; i < data_len; i++) {
        *p++ = hx[(data[i] >> 4) & 0x0F];
        *p++ = hx[ data[i]       & 0x0F];
    }
    *p++ = '\n';
    *pos = (size_t)(p - buf);
    return TW_OK;
}
