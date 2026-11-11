/* SM3 hash, written for this project from GB/T 32905-2016. */
#include <string.h>
#include "pqc_prims.h"
#include "pqc_sm3.h"

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define P0(x) ((x) ^ ROTL((x), 9) ^ ROTL((x), 17))
#define P1(x) ((x) ^ ROTL((x), 15) ^ ROTL((x), 23))

static uint32_t load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void store_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

void pqc_sm3_compress_ref(uint32_t state[8], const uint8_t *blocks, size_t nblocks)
{
    uint32_t w[68];

    while (nblocks--) {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        int j;

        for (j = 0; j < 16; j++) {
            w[j] = load_be32(blocks + 4 * j);
        }
        for (j = 16; j < 68; j++) {
            uint32_t x = w[j - 16] ^ w[j - 9] ^ ROTL(w[j - 3], 15);
            w[j] = P1(x) ^ ROTL(w[j - 13], 7) ^ w[j - 6];
        }
        for (j = 0; j < 64; j++) {
            uint32_t t = (j < 16) ? 0x79cc4519u : 0x7a879d8au;
            uint32_t a12 = ROTL(a, 12);
            uint32_t tj = (j & 31) ? ROTL(t, j & 31) : t;
            uint32_t ss1 = a12 + e + tj;
            uint32_t ss2, tt1, tt2;

            ss1 = ROTL(ss1, 7);
            ss2 = ss1 ^ a12;
            if (j < 16) {
                tt1 = (a ^ b ^ c) + d + ss2 + (w[j] ^ w[j + 4]);
                tt2 = (e ^ f ^ g) + h + ss1 + w[j];
            } else {
                tt1 = ((a & b) | (a & c) | (b & c)) + d + ss2 + (w[j] ^ w[j + 4]);
                tt2 = ((e & f) | (~e & g)) + h + ss1 + w[j];
            }
            d = c;
            c = ROTL(b, 9);
            b = a;
            a = tt1;
            h = g;
            g = ROTL(f, 19);
            f = e;
            e = P0(tt2);
        }
        state[0] ^= a; state[1] ^= b; state[2] ^= c; state[3] ^= d;
        state[4] ^= e; state[5] ^= f; state[6] ^= g; state[7] ^= h;
        blocks += 64;
    }
}

void pqc_sm3_init(pqc_sm3_ctx *ctx)
{
    static const uint32_t iv[8] = {0x7380166f, 0x4914b2b9, 0x172442d7, 0xda8a0600,
                                   0xa96f30bc, 0x163138aa, 0xe38dee4d, 0xb0fb0e4e};
    memcpy(ctx->state, iv, sizeof(iv));
    ctx->nbytes = 0;
}

void pqc_sm3_update(pqc_sm3_ctx *ctx, const uint8_t *in, size_t inlen)
{
    size_t have = (size_t)(ctx->nbytes & 63);

    ctx->nbytes += inlen;
    if (have) {
        size_t take = 64 - have;
        if (take > inlen) {
            take = inlen;
        }
        memcpy(ctx->buf + have, in, take);
        in += take;
        inlen -= take;
        if (have + take < 64) {
            return;
        }
        pqc_sm3_compress(ctx->state, ctx->buf, 1);
    }
    if (inlen >= 64) {
        pqc_sm3_compress(ctx->state, in, inlen / 64);
        in += inlen & ~(size_t)63;
        inlen &= 63;
    }
    memcpy(ctx->buf, in, inlen);
}

void pqc_sm3_final(pqc_sm3_ctx *ctx, uint8_t out[32])
{
    size_t have = (size_t)(ctx->nbytes & 63);
    uint64_t bits = ctx->nbytes << 3;
    int i;

    ctx->buf[have++] = 0x80;
    if (have > 56) {
        memset(ctx->buf + have, 0, 64 - have);
        pqc_sm3_compress(ctx->state, ctx->buf, 1);
        have = 0;
    }
    memset(ctx->buf + have, 0, 56 - have);
    store_be32(ctx->buf + 56, (uint32_t)(bits >> 32));
    store_be32(ctx->buf + 60, (uint32_t)bits);
    pqc_sm3_compress(ctx->state, ctx->buf, 1);
    for (i = 0; i < 8; i++) {
        store_be32(out + 4 * i, ctx->state[i]);
    }
}

void pqc_sm3(uint8_t out[32], const uint8_t *in, size_t inlen)
{
    pqc_sm3_ctx ctx;
    pqc_sm3_init(&ctx);
    pqc_sm3_update(&ctx, in, inlen);
    pqc_sm3_final(&ctx, out);
}
