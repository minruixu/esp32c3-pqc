/* What the GmSSL sources need from the rest of GmSSL, provided here:
 * random bytes, the print helpers (not needed, so they do nothing), and the
 * SM3 and SHA-3 entry points with a run-time choice of implementation. */
#include <stdio.h>
#include <string.h>
#include "gmssl_rename.h"
#include <gmssl/rand.h>
#include <gmssl/sha3.h>
#include <gmssl/sm3.h>
#include "bench.h"
#include "gmssl_port.h"
#include "pqc_prims.h"
#ifndef PQC_HOST
#include "esp_timer.h"
#endif

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "the fast SHAKE path assumes a little-endian machine"
#endif

int gmssl_fast = 0;

/* ---------- random bytes: the harness generator ---------- */
int rand_bytes(uint8_t *buf, size_t len)
{
    randombytes(buf, len);
    return 1;
}

/* ---------- print helpers: silent ---------- */
int format_print(FILE *fp, int format, int indent, const char *str, ...)
{
    (void)fp;
    (void)format;
    (void)indent;
    (void)str;
    return 1;
}

int format_bytes(FILE *fp, int format, int indent, const char *str, const uint8_t *data, size_t datalen)
{
    (void)fp;
    (void)format;
    (void)indent;
    (void)str;
    (void)data;
    (void)datalen;
    return 1;
}

/* ---------- SM3 ---------- */
void gmssl_ref_sm3_compress_blocks(uint32_t digest[8], const uint8_t *data, size_t blocks);
void gmssl_ref_sm3_init(SM3_CTX *ctx);
void gmssl_ref_sm3_update(SM3_CTX *ctx, const uint8_t *data, size_t datalen);
void gmssl_ref_sm3_finish(SM3_CTX *ctx, uint8_t dgst[SM3_DIGEST_SIZE]);

/* Fast mode has two candidates for SM3: GmSSL's own code, and the leaner
 * buffering below around the SM3 backend of components/prims.
 * gmssl_port_calibrate() times the two compression functions on the device;
 * if GmSSL's is the quicker one (it is fully unrolled, and it was when this
 * was written), fast mode simply keeps GmSSL's SM3. */
static int fast_keeps_gmssl_sm3;

#define USE_GMSSL_SM3() (!gmssl_fast || fast_keeps_gmssl_sm3)

void gmssl_port_calibrate(void)
{
#ifndef PQC_HOST
    static uint8_t blocks[16 * 64];
    uint32_t st[8] = {0};
    int64_t t0, prims, own;

    t0 = esp_timer_get_time();
    for (int i = 0; i < 16; i++) {
        pqc_sm3_compress(st, blocks, 16);
    }
    prims = esp_timer_get_time() - t0;
    t0 = esp_timer_get_time();
    for (int i = 0; i < 16; i++) {
        gmssl_ref_sm3_compress_blocks(st, blocks, 16);
    }
    own = esp_timer_get_time() - t0;
    fast_keeps_gmssl_sm3 = own <= prims;
    bench_info(pqc_prims_selected() ? "gmssl-sm3-compress/opt" : "gmssl-sm3-compress/ref",
               "256 blocks: prims backend %ld us, GmSSL %ld us; fast mode uses %s", (long)prims, (long)own,
               fast_keeps_gmssl_sm3 ? "GmSSL" : "prims");
#endif
}

void sm3_compress_blocks(uint32_t digest[8], const uint8_t *data, size_t blocks)
{
    if (USE_GMSSL_SM3()) {
        gmssl_ref_sm3_compress_blocks(digest, data, blocks);
    } else {
        pqc_sm3_compress(digest, data, blocks);
    }
}

void sm3_init(SM3_CTX *ctx)
{
    static const uint32_t iv[8] = {0x7380166fu, 0x4914b2b9u, 0x172442d7u, 0xda8a0600u,
                                   0xa96f30bcu, 0x163138aau, 0xe38dee4du, 0xb0fb0e4eu};
    if (USE_GMSSL_SM3()) {
        gmssl_ref_sm3_init(ctx);
        return;
    }
    memcpy(ctx->digest, iv, sizeof(iv));
    ctx->nblocks = 0;
    ctx->num = 0;
}

void sm3_update(SM3_CTX *ctx, const uint8_t *data, size_t datalen)
{
    size_t have;

    if (USE_GMSSL_SM3()) {
        gmssl_ref_sm3_update(ctx, data, datalen);
        return;
    }
    have = ctx->num;
    if (have) {
        size_t take = SM3_BLOCK_SIZE - have;
        if (take > datalen) {
            take = datalen;
        }
        memcpy(ctx->block + have, data, take);
        data += take;
        datalen -= take;
        ctx->num = have + take;
        if (ctx->num < SM3_BLOCK_SIZE) {
            return;
        }
        pqc_sm3_compress(ctx->digest, ctx->block, 1);
        ctx->nblocks++;
        ctx->num = 0;
    }
    if (datalen >= SM3_BLOCK_SIZE) {
        size_t blocks = datalen / SM3_BLOCK_SIZE;
        pqc_sm3_compress(ctx->digest, data, blocks);
        ctx->nblocks += blocks;
        data += blocks * SM3_BLOCK_SIZE;
        datalen -= blocks * SM3_BLOCK_SIZE;
    }
    if (datalen) {
        memcpy(ctx->block, data, datalen);
        ctx->num = datalen;
    }
}

void sm3_finish(SM3_CTX *ctx, uint8_t dgst[SM3_DIGEST_SIZE])
{
    uint64_t bits;
    size_t have;

    if (USE_GMSSL_SM3()) {
        gmssl_ref_sm3_finish(ctx, dgst);
        return;
    }
    have = ctx->num;
    bits = (ctx->nblocks * SM3_BLOCK_SIZE + have) << 3;
    ctx->block[have++] = 0x80;
    if (have > 56) {
        memset(ctx->block + have, 0, SM3_BLOCK_SIZE - have);
        pqc_sm3_compress(ctx->digest, ctx->block, 1);
        have = 0;
    }
    memset(ctx->block + have, 0, 56 - have);
    for (int i = 0; i < 8; i++) {
        ctx->block[56 + i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    pqc_sm3_compress(ctx->digest, ctx->block, 1);
    for (int i = 0; i < 8; i++) {
        dgst[4 * i] = (uint8_t)(ctx->digest[i] >> 24);
        dgst[4 * i + 1] = (uint8_t)(ctx->digest[i] >> 16);
        dgst[4 * i + 2] = (uint8_t)(ctx->digest[i] >> 8);
        dgst[4 * i + 3] = (uint8_t)ctx->digest[i];
    }
}

#ifdef GMSSL_WITH_KYBER
/* ---------- SHA-3 and SHAKE ---------- */
void gmssl_ref_sha3_256(const uint8_t *in, size_t inlen, uint8_t out[SHA3_256_DIGEST_SIZE]);
void gmssl_ref_sha3_512(const uint8_t *in, size_t inlen, uint8_t out[SHA3_512_DIGEST_SIZE]);
void gmssl_ref_shake128_init(SHAKE_CTX *ctx);
void gmssl_ref_shake256_init(SHAKE_CTX *ctx);
void gmssl_ref_shake_update(SHAKE_CTX *ctx, const uint8_t *in, size_t inlen);
void gmssl_ref_shake_finish(SHAKE_CTX *ctx);
void gmssl_ref_shake_squeeze(SHAKE_CTX *ctx, uint8_t *out, size_t outlen);
void gmssl_ref_shake128(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen);
void gmssl_ref_shake256(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen);

/* Fast sponge on the same context: the state is used as a byte array, num
 * is the position inside the current rate block, block[] is not used. */
static void fast_init(SHAKE_CTX *ctx, size_t rate)
{
    memset(ctx->state, 0, sizeof(ctx->state));
    ctx->rate = rate;
    ctx->num = 0;
    ctx->squeezing = 0;
}

static void fast_absorb(SHAKE_CTX *ctx, const uint8_t *in, size_t inlen)
{
    uint8_t *s = (uint8_t *)ctx->state;
    size_t pos = ctx->num, rate = ctx->rate;

    while (inlen) {
        size_t take = rate - pos;
        if (take > inlen) {
            take = inlen;
        }
        for (size_t i = 0; i < take; i++) {
            s[pos + i] ^= in[i];
        }
        in += take;
        inlen -= take;
        pos += take;
        if (pos == rate) {
            pqc_keccakf1600(ctx->state);
            pos = 0;
        }
    }
    ctx->num = pos;
}

static void fast_finish(SHAKE_CTX *ctx, uint8_t suffix)
{
    uint8_t *s = (uint8_t *)ctx->state;

    s[ctx->num] ^= suffix;
    s[ctx->rate - 1] ^= 0x80;
    ctx->num = ctx->rate; /* nothing squeezed from this state yet */
    ctx->squeezing = 1;
}

static void fast_squeeze(SHAKE_CTX *ctx, uint8_t *out, size_t outlen)
{
    const uint8_t *s = (const uint8_t *)ctx->state;
    size_t pos = ctx->num, rate = ctx->rate;

    while (outlen) {
        size_t take;
        if (pos == rate) {
            pqc_keccakf1600(ctx->state);
            pos = 0;
        }
        take = rate - pos;
        if (take > outlen) {
            take = outlen;
        }
        memcpy(out, s + pos, take);
        out += take;
        outlen -= take;
        pos += take;
    }
    ctx->num = pos;
}

static void fast_digest(size_t rate, uint8_t suffix, const uint8_t *in, size_t inlen, uint8_t *out,
                        size_t outlen)
{
    SHAKE_CTX ctx;

    fast_init(&ctx, rate);
    fast_absorb(&ctx, in, inlen);
    fast_finish(&ctx, suffix);
    fast_squeeze(&ctx, out, outlen);
}

void sha3_256(const uint8_t *in, size_t inlen, uint8_t out[SHA3_256_DIGEST_SIZE])
{
    if (gmssl_fast) {
        fast_digest(136, 0x06, in, inlen, out, SHA3_256_DIGEST_SIZE);
    } else {
        gmssl_ref_sha3_256(in, inlen, out);
    }
}

void sha3_512(const uint8_t *in, size_t inlen, uint8_t out[SHA3_512_DIGEST_SIZE])
{
    if (gmssl_fast) {
        fast_digest(72, 0x06, in, inlen, out, SHA3_512_DIGEST_SIZE);
    } else {
        gmssl_ref_sha3_512(in, inlen, out);
    }
}

void shake128_init(SHAKE_CTX *ctx)
{
    if (gmssl_fast) {
        fast_init(ctx, 168);
    } else {
        gmssl_ref_shake128_init(ctx);
    }
}

void shake256_init(SHAKE_CTX *ctx)
{
    if (gmssl_fast) {
        fast_init(ctx, 136);
    } else {
        gmssl_ref_shake256_init(ctx);
    }
}

void shake_update(SHAKE_CTX *ctx, const uint8_t *in, size_t inlen)
{
    if (gmssl_fast) {
        fast_absorb(ctx, in, inlen);
    } else {
        gmssl_ref_shake_update(ctx, in, inlen);
    }
}

void shake_finish(SHAKE_CTX *ctx)
{
    if (gmssl_fast) {
        fast_finish(ctx, 0x1f);
    } else {
        gmssl_ref_shake_finish(ctx);
    }
}

void shake_squeeze(SHAKE_CTX *ctx, uint8_t *out, size_t outlen)
{
    if (gmssl_fast) {
        fast_squeeze(ctx, out, outlen);
    } else {
        gmssl_ref_shake_squeeze(ctx, out, outlen);
    }
}

void shake128(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen)
{
    if (gmssl_fast) {
        fast_digest(168, 0x1f, in, inlen, out, outlen);
    } else {
        gmssl_ref_shake128(in, inlen, out, outlen);
    }
}

void shake256(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen)
{
    if (gmssl_fast) {
        fast_digest(136, 0x1f, in, inlen, out, outlen);
    } else {
        gmssl_ref_shake256(in, inlen, out, outlen);
    }
}
#endif /* GMSSL_WITH_KYBER */
