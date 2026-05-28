/*
 * pal_crypto_esp.c -- SHA-256 via mbedTLS (bundled with ESP-IDF).
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_crypto.h"
#include "mbedtls/sha256.h"
#include <string.h>

tw_err_t pal_crypto_sha256(const void *data, size_t len,
                           uint8_t digest[PAL_SHA256_DIGEST_SIZE])
{
    int ret = mbedtls_sha256((const unsigned char *)data, len, digest, 0);
    return ret == 0 ? TW_OK : TW_ERR_IO;
}

/*
 * Incremental API: wraps mbedtls_sha256_context state into our
 * pal_sha256_ctx_t.  Since the struct layouts differ, we use a
 * stack-allocated mbedtls context and copy state through the
 * public-domain SHA-256 fields in pal_sha256_ctx_t.
 *
 * For simplicity, we reuse the POSIX incremental approach since
 * mbedTLS's context is opaque and larger than our struct.  The
 * transform core is hardware-accelerated on ESP32-S3/C3/C6 via
 * mbedTLS anyway for the one-shot API; the incremental path
 * here is only used for OTA verification and is plenty fast.
 */

static uint32_t ror32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

static void sha256_transform(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4]<<24) | ((uint32_t)block[i*4+1]<<16) |
               ((uint32_t)block[i*4+2]<<8) | block[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror32(w[i-15],7) ^ ror32(w[i-15],18) ^ (w[i-15]>>3);
        uint32_t s1 = ror32(w[i-2],17) ^ ror32(w[i-2],19)  ^ (w[i-2]>>10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a=state[0]; b=state[1]; c=state[2]; d=state[3];
    e=state[4]; f=state[5]; g=state[6]; h=state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ror32(e,6) ^ ror32(e,11) ^ ror32(e,25);
        uint32_t ch = (e&f) ^ (~e&g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ror32(a,2) ^ ror32(a,13) ^ ror32(a,22);
        uint32_t maj = (a&b) ^ (a&c) ^ (b&c);
        uint32_t t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

void pal_sha256_init(pal_sha256_ctx_t *ctx)
{
    static const uint32_t iv[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19,
    };
    memcpy(ctx->state, iv, sizeof(iv));
    ctx->buf_len   = 0;
    ctx->total_len = 0;
}

void pal_sha256_update(pal_sha256_ctx_t *ctx, const void *data, size_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    ctx->total_len += len;
    if (ctx->buf_len > 0) {
        size_t need = 64 - ctx->buf_len;
        size_t copy = len < need ? len : need;
        memcpy(ctx->buf + ctx->buf_len, src, copy);
        ctx->buf_len += copy; src += copy; len -= copy;
        if (ctx->buf_len == 64) { sha256_transform(ctx->state, ctx->buf); ctx->buf_len = 0; }
    }
    while (len >= 64) { sha256_transform(ctx->state, src); src += 64; len -= 64; }
    if (len > 0) { memcpy(ctx->buf, src, len); ctx->buf_len = len; }
}

tw_err_t pal_sha256_finish(pal_sha256_ctx_t *ctx,
                           uint8_t digest[PAL_SHA256_DIGEST_SIZE])
{
    uint8_t pad[128];
    memset(pad, 0, sizeof(pad));
    memcpy(pad, ctx->buf, ctx->buf_len);
    pad[ctx->buf_len] = 0x80;
    size_t pad_len = (ctx->buf_len < 56) ? 64 : 128;
    uint64_t bits = ctx->total_len * 8;
    for (int j = 0; j < 8; j++) pad[pad_len - 1 - j] = (uint8_t)(bits >> (j * 8));
    for (size_t p = 0; p < pad_len; p += 64) sha256_transform(ctx->state, pad + p);
    for (int j = 0; j < 8; j++) {
        digest[j*4+0] = (uint8_t)(ctx->state[j] >> 24);
        digest[j*4+1] = (uint8_t)(ctx->state[j] >> 16);
        digest[j*4+2] = (uint8_t)(ctx->state[j] >> 8);
        digest[j*4+3] = (uint8_t)(ctx->state[j]);
    }
    return TW_OK;
}
