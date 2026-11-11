/* PQMagic's SM3 interface (hash/sm3/x86-64/include/sm3.h) on top of the
 * project's SM3 compression function, so that the reference/optimized
 * backend switch of pqc_prims.h applies to every PQMagic algorithm.
 * Same behaviour as PQMagic's hash/sm3/x86-64/sm3.c, which is not built. */
#include <string.h>
#include "hash/sm3/x86-64/include/sm3.h"
#include "pqc_prims.h"

static void store_be32(u1 *p, uint32_t v)
{
    p[0] = (u1)(v >> 24);
    p[1] = (u1)(v >> 16);
    p[2] = (u1)(v >> 8);
    p[3] = (u1)v;
}

void sm3_compress(u4 digest[8], const u1 *block, u4 nblocks)
{
    pqc_sm3_compress((uint32_t *)digest, block, nblocks);
}

void sm3_init(SM3_CTX *ctx)
{
    ctx->digest[0] = 0x7380166F;
    ctx->digest[1] = 0x4914B2B9;
    ctx->digest[2] = 0x172442D7;
    ctx->digest[3] = 0xDA8A0600;
    ctx->digest[4] = 0xA96F30BC;
    ctx->digest[5] = 0x163138AA;
    ctx->digest[6] = 0xE38DEE4D;
    ctx->digest[7] = 0xB0FB0E4E;
    ctx->nblocks = 0;
    ctx->num = 0;
}

void sm3_update(SM3_CTX *ctx, const u1 *data, size_t data_len)
{
    size_t nblock, left;

    if (ctx->num) {
        left = SM3_BLOCK_SIZE - ctx->num;
        if (data_len < left) {
            memcpy(ctx->block + ctx->num, data, data_len);
            ctx->num += (uint32_t)data_len;
            return;
        }
        memcpy(ctx->block + ctx->num, data, left);
        pqc_sm3_compress(ctx->digest, ctx->block, 1);
        ctx->nblocks++;
        data += left;
        data_len -= left;
        ctx->num = 0;
    }
    nblock = data_len / SM3_BLOCK_SIZE;
    left = data_len % SM3_BLOCK_SIZE;
    if (nblock > 0) {
        pqc_sm3_compress(ctx->digest, data, nblock);
        ctx->nblocks += (uint32_t)nblock;
    }
    ctx->num = (uint32_t)left;
    memcpy(ctx->block, data + data_len - left, left);
}

void sm3_final(SM3_CTX *ctx, u1 digest[SM3_DIGEST_LENGTH])
{
    int i;

    ctx->block[ctx->num] = 0x80;
    if (ctx->num + 9 <= SM3_BLOCK_SIZE) {
        memset(ctx->block + ctx->num + 1, 0, SM3_BLOCK_SIZE - ctx->num - 9);
    } else {
        memset(ctx->block + ctx->num + 1, 0, SM3_BLOCK_SIZE - ctx->num - 1);
        pqc_sm3_compress(ctx->digest, ctx->block, 1);
        memset(ctx->block, 0, SM3_BLOCK_SIZE - 8);
    }
    /* message length in bits, big-endian, as PQMagic computes it */
    store_be32(ctx->block + SM3_BLOCK_SIZE - 8, ctx->nblocks >> 23);
    store_be32(ctx->block + SM3_BLOCK_SIZE - 4, (ctx->nblocks << 9) + (ctx->num << 3));
    pqc_sm3_compress(ctx->digest, ctx->block, 1);
    for (i = 0; i < 8; i++) {
        store_be32(digest + 4 * i, ctx->digest[i]);
    }
}

/* One-shot SM3 without a context: full blocks are compressed straight from
 * the input and only the padded tail is copied. Used by the optimized
 * variant; the reference variant keeps PQMagic's init/update/final flow. */
static const uint32_t sm3_iv[8] = {0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
                                   0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E};

static void sm3_tail(uint32_t st[8], const u1 *tail, size_t rem, uint64_t total_bytes)
{
    u1 last[128];
    size_t n = (rem < 56) ? 64 : 128;
    uint64_t bits = total_bytes << 3;

    memcpy(last, tail, rem);
    last[rem] = 0x80;
    memset(last + rem + 1, 0, n - rem - 9);
    store_be32(last + n - 8, (uint32_t)(bits >> 32));
    store_be32(last + n - 4, (uint32_t)bits);
    pqc_sm3_compress(st, last, n / 64);
}

static void sm3_oneshot(const u1 *data, size_t datalen, u1 dgst[SM3_DIGEST_LENGTH])
{
    uint32_t st[8];
    size_t full = datalen / 64;
    int i;

    memcpy(st, sm3_iv, sizeof(st));
    if (full) {
        pqc_sm3_compress(st, data, full);
    }
    sm3_tail(st, data + 64 * full, datalen - 64 * full, datalen);
    for (i = 0; i < 8; i++) {
        store_be32(dgst + 4 * i, st[i]);
    }
}

size_t sm3(const u1 *data, size_t datalen, u1 dgst[SM3_DIGEST_LENGTH])
{
    SM3_CTX ctx;

#ifndef PQM_NO_LEAN_SM3
    if (pqc_prims_selected() == PQC_PRIMS_OPT) {
        sm3_oneshot(data, datalen, dgst);
        return SM3_DIGEST_LENGTH;
    }
#endif
    sm3_init(&ctx);
    sm3_update(&ctx, data, datalen);
    sm3_final(&ctx, dgst);
    memset(&ctx, 0, sizeof(SM3_CTX));
    return SM3_DIGEST_LENGTH;
}

#define IPAD 0x36
#define OPAD 0x5C

void sm3_hmac_init(SM3_HMAC_CTX *ctx, const u1 *key, size_t keylen)
{
    int i;

    if (keylen <= SM3_BLOCK_SIZE) {
        memcpy(ctx->key, key, keylen);
        memset(ctx->key + keylen, 0, SM3_BLOCK_SIZE - keylen);
    } else {
        sm3_init(&ctx->sm3_ctx);
        sm3_update(&ctx->sm3_ctx, key, keylen);
        sm3_final(&ctx->sm3_ctx, ctx->key);
        memset(ctx->key + SM3_DIGEST_LENGTH, 0, SM3_BLOCK_SIZE - SM3_DIGEST_LENGTH);
    }
    for (i = 0; i < SM3_BLOCK_SIZE; i++) {
        ctx->key[i] ^= IPAD;
    }
    sm3_init(&ctx->sm3_ctx);
    sm3_update(&ctx->sm3_ctx, ctx->key, SM3_BLOCK_SIZE);
}

void sm3_hmac_update(SM3_HMAC_CTX *ctx, const u1 *data, size_t data_len)
{
    sm3_update(&ctx->sm3_ctx, data, data_len);
}

void sm3_hmac_final(SM3_HMAC_CTX *ctx, u1 mac[SM3_HMAC_SIZE])
{
    int i;

    for (i = 0; i < SM3_BLOCK_SIZE; i++) {
        ctx->key[i] ^= (IPAD ^ OPAD);
    }
    sm3_final(&ctx->sm3_ctx, mac);
    sm3_init(&ctx->sm3_ctx);
    sm3_update(&ctx->sm3_ctx, ctx->key, SM3_BLOCK_SIZE);
    sm3_update(&ctx->sm3_ctx, mac, SM3_DIGEST_LENGTH);
    sm3_final(&ctx->sm3_ctx, mac);
}

size_t sm3_hmac(const u1 *data, size_t data_len, const u1 *key, size_t key_len, u1 mac[SM3_HMAC_SIZE])
{
    SM3_HMAC_CTX ctx;

    sm3_hmac_init(&ctx, key, key_len);
    sm3_hmac_update(&ctx, data, data_len);
    sm3_hmac_final(&ctx, mac);
    memset(&ctx, 0, sizeof(ctx));
    return SM3_HMAC_SIZE;
}

/* ---- sm3_extended: SM3(counter || in) for counter = 0, 1, ... ----
 * PQMagic's function (hash/sm3/sm3_extended.c) is built under the name
 * pqm_sm3_extended_upstream and serves the reference variant. The optimized
 * variant uses the code below: for the short inputs that dominate (seeds
 * with a nonce) the padded block is laid out once and only the four counter
 * bytes change between calls of the compression function. */
void pqm_sm3_extended_upstream(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen);

static void sm3_extended_fast(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen)
{
    uint32_t nonce = 0;
    size_t done = 0;
    int i;

    if (inlen <= 51) {
        u1 block[64];

        memcpy(block + 4, in, inlen);
        block[4 + inlen] = 0x80;
        memset(block + 5 + inlen, 0, 64 - 5 - inlen - 4);
        store_be32(block + 60, (uint32_t)((inlen + 4) << 3));
        while (done < outlen) {
            uint32_t st[8];
            size_t take = outlen - done;

            store_be32(block, nonce++);
            memcpy(st, sm3_iv, sizeof(st));
            pqc_sm3_compress(st, block, 1);
            if (take >= 32) {
                for (i = 0; i < 8; i++) {
                    store_be32(out + done + 4 * i, st[i]);
                }
                done += 32;
            } else {
                u1 d[32];
                for (i = 0; i < 8; i++) {
                    store_be32(d + 4 * i, st[i]);
                }
                memcpy(out + done, d, take);
                done += take;
            }
        }
        return;
    }
    /* long input: first block is counter || in[0..59], then the rest */
    while (done < outlen) {
        uint32_t st[8];
        u1 first[64], d[32];
        size_t take = outlen - done, rest = inlen - 60, full = rest / 64;

        store_be32(first, nonce++);
        memcpy(first + 4, in, 60);
        memcpy(st, sm3_iv, sizeof(st));
        pqc_sm3_compress(st, first, 1);
        if (full) {
            pqc_sm3_compress(st, in + 60, full);
        }
        sm3_tail(st, in + 60 + 64 * full, rest - 64 * full, (uint64_t)inlen + 4);
        for (i = 0; i < 8; i++) {
            store_be32(d + 4 * i, st[i]);
        }
        if (take > 32) {
            take = 32;
        }
        memcpy(out + done, d, take);
        done += take;
    }
}

void sm3_extended(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen)
{
    /* the fast path reads `in` while it writes `out`; overlapping buffers
     * take the upstream code, which handles them */
    const uintptr_t a = (uintptr_t)out, b = (uintptr_t)in;
    const int overlap = outlen && inlen && a < b + inlen && b < a + outlen;

#ifndef PQM_NO_LEAN_SM3
    if (pqc_prims_selected() == PQC_PRIMS_OPT && !overlap) {
        sm3_extended_fast(out, outlen, in, inlen);
        return;
    }
#endif
    (void)overlap;
    pqm_sm3_extended_upstream(out, outlen, in, inlen);
}
