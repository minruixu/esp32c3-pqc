/* SLH-DSA with SHA-2 on the ESP32-C3: the tweakable hash functions F, PRF,
 * H and T_l run SHA-256 on the chip's SHA accelerator.
 *
 * Every one of these hashes is SHA-256(PK.seed || zeros || ADRSc || data),
 * where the first 64-byte block is the same for a whole key. Its chaining
 * value (the "midstate") is computed once per key, and each hash is then one
 * compression that starts from it.
 *
 * What makes the accelerator fast is not its compression (about 80 CPU
 * cycles) but how little is said to it: every register access costs 8 to 9
 * CPU cycles. So this file writes as few registers as possible:
 *
 *  - The midstate is written straight into the H registers; the accelerator
 *    keeps the chaining value in the byte order of the standard, so nothing
 *    is byte-swapped on the way in or out.
 *  - The message registers keep their contents between compressions. Only
 *    the words that differ from the previous block are written: within a
 *    WOTS+ chain of n = 16 bytes that is 5 words out of 16.
 *  - Words of the block that rarely change (the address, the zero padding,
 *    the length) are tracked in a copy in RAM and rewritten only on change.
 *  - Only the n bytes that are needed are read back, and they stay in CPU
 *    registers from one chain step to the next.
 *
 * A chain step for n = 16 is 8 + 5 register writes, one start, the wait and
 * 4 reads: about 250 CPU cycles instead of about 4200 for the compression
 * in portable C.
 *
 * For n = 24 and n = 32 the standard uses SHA-512 for H and T_l. The
 * accelerator has no SHA-512, so those run in software, on the compression
 * function in components/prims/opt/sha512_rv32imc.S; F and PRF use the
 * accelerator.
 *
 * One key at a time: the midstate and the register copy are static, like
 * the accelerator itself. Nothing here takes a lock.
 */
#include <string.h>
#include "plat_local.h"
#include "sha2_api.h"
#include "slh_adrs.h"
#include "slh_dsa.h"
#include "slh_var.h"
#include "sha256_hw.h"
#include "slh_c3.h"

#define M PQC_SHA_M
#define H PQC_SHA_H
#define INLINE static inline __attribute__((always_inline))
#define UNROLL _Pragma("GCC unroll 16")

typedef uint32_t __attribute__((may_alias)) u32a;

/* SHA-512 compression in assembly: components/prims/opt/sha512_rv32imc.S.
 * state and block: (high, low) pairs of native words; word j of a block of
 * bytes is the big-endian value of bytes 4j..4j+3. */
void pqc_sha512_compress_rv32(uint32_t state[16], const uint32_t block[32]);

static struct {
    uint32_t mid[8];          /* chaining value after the PK.seed block */
    uint32_t mid512[16];      /* the same for SHA-512, as native (high, low) pairs */
    uint32_t w512[32];        /* block of H for SHA-512: padding and length stay in place */
    uint32_t sh[16];          /* what the tracked M registers hold */
    uint32_t prf[9];          /* SK.seed laid out as the data of a PRF block */
    const slh_param_t *up;    /* the slhdsa-c parameter set this one extends */
} c3;

unsigned slh_c3_m_dirty;

slh_param_t slh_c3_sha2_128s, slh_c3_sha2_128f;
slh_param_t slh_c3_sha2_192s, slh_c3_sha2_192f;
slh_param_t slh_c3_sha2_256s, slh_c3_sha2_256f;

/* Length field of a hash over the PK.seed block plus `nbytes` more. */
#define LENW(nbytes) (((((64u + (nbytes)) * 8u) & 0xffu) << 24) | ((((64u + (nbytes)) * 8u) & 0xff00u) << 8))

/* ---------- message registers ---------- */

/* Someone else used the message registers: write the tracked copy back. */
static void resync(void)
{
    for (int i = 0; i < 16; i++) {
        M[i] = c3.sh[i];
    }
    slh_c3_m_dirty = 0;
}

INLINE void sync(void)
{
    if (__builtin_expect(slh_c3_m_dirty != 0, 0)) {
        resync();
    }
}

/* Write a tracked word only if it changes. */
INLINE void put(unsigned i, uint32_t v)
{
    if (c3.sh[i] != v) {
        c3.sh[i] = v;
        M[i] = v;
    }
}

/* ADRSc occupies block bytes 0..21: words 0..4 and the low half of word 5.
 * Writes words 0..4 and returns the low half of word 5. */
INLINE uint32_t head(const slh_var_t *var)
{
    const uint32_t *a = var->adrs->u32;
    uint32_t a2 = a[2], a3 = a[3], a5 = a[5], a6 = a[6], a7 = a[7];

    put(0, (a[0] >> 24) | (a2 << 8));
    put(1, (a2 >> 24) | (a3 << 8));
    put(2, (a3 >> 24) | ((a[4] >> 24) << 8) | (a5 << 16));
    put(3, (a5 >> 16) | (a6 << 16));
    put(4, (a6 >> 16) | (a7 << 16));
    return a7 >> 16;
}

/* Zero words after the data, and the length. */
INLINE void tail(const unsigned first_zero, const uint32_t lenw)
{
    UNROLL
    for (unsigned i = first_zero; i < 15; i++) {
        put(i, 0);
    }
    put(15, lenw);
}

INLINE void load_mid(void)
{
    UNROLL
    for (unsigned i = 0; i < 8; i++) {
        H[i] = c3.mid[i];
    }
}

/* Data words x[0..nw-1] start at block byte 22, in the middle of word 5. */
INLINE void put_data(uint32_t lo5, const uint32_t *x, const unsigned nw)
{
    uint32_t prev = lo5;
    UNROLL
    for (unsigned k = 0; k < nw; k++) {
        uint32_t w = x[k];
        M[5 + k] = prev | (w << 16);
        prev = w >> 16;
    }
    M[5 + nw] = prev | 0x00800000u; /* two data bytes, then the 0x80 padding byte */
}

INLINE void read_out(uint32_t *x, const unsigned nw)
{
    UNROLL
    for (unsigned k = 0; k < nw; k++) {
        x[k] = H[k];
    }
}

INLINE void load_words(uint32_t *w, const uint8_t *p, const unsigned nw)
{
    if (((uintptr_t)p & 3) == 0) {
        const u32a *q = (const u32a *)(const void *)p;
        UNROLL
        for (unsigned k = 0; k < nw; k++) {
            w[k] = q[k];
        }
    } else {
        memcpy(w, p, 4 * nw);
    }
}

INLINE void store_words(uint8_t *p, const uint32_t *w, const unsigned nw)
{
    if (((uintptr_t)p & 3) == 0) {
        u32a *q = (u32a *)(void *)p;
        UNROLL
        for (unsigned k = 0; k < nw; k++) {
            q[k] = w[k];
        }
    } else {
        memcpy(p, w, 4 * nw);
    }
}

/* ---------- the hashes, on words ---------- */

/* F(PK.seed, ADRS, x): x in, result out, both as nw words. */
INLINE void f_words(const slh_var_t *var, uint32_t *x, const unsigned nw)
{
    uint32_t lo5;

    sync();
    lo5 = head(var);
    tail(6 + nw, LENW(22 + 4 * nw));
    load_mid();
    put_data(lo5, x, nw);
    pqc_sha256_hw_run();
    read_out(x, nw);
}

/* PRF(PK.seed, SK.seed, ADRS) */
INLINE void prf_words(const slh_var_t *var, uint32_t *x, const unsigned nw)
{
    uint32_t lo5;

    sync();
    lo5 = head(var);
    tail(6 + nw, LENW(22 + 4 * nw));
    load_mid();
    M[5] = c3.prf[0] | lo5;
    UNROLL
    for (unsigned k = 1; k <= nw; k++) {
        M[5 + k] = c3.prf[k];
    }
    pqc_sha256_hw_run();
    read_out(x, nw);
}

/* s steps of the WOTS+ chain from hash address i (i + s <= 65536). */
INLINE void chain_words(slh_var_t *var, uint32_t *x, uint32_t i, uint32_t s, const unsigned nw)
{
    uint32_t m[8], xw[8];

    adrs_set_hash_address(var, i);
    sync();
    (void)head(var);
    tail(6 + nw, LENW(22 + 4 * nw));
    UNROLL
    for (unsigned k = 0; k < 8; k++) {
        m[k] = c3.mid[k];
    }
    UNROLL
    for (unsigned k = 0; k < nw; k++) {
        xw[k] = x[k];
    }
    for (uint32_t v = i; v < i + s; v++) {
        /* low half of word 5: the two low bytes of the hash address */
        uint32_t prev = ((v & 0xffu) << 8) | (v >> 8);

        UNROLL
        for (unsigned k = 0; k < 8; k++) {
            H[k] = m[k];
        }
        UNROLL
        for (unsigned k = 0; k < nw; k++) {
            uint32_t w = xw[k];
            M[5 + k] = prev | (w << 16);
            prev = w >> 16;
        }
        M[5 + nw] = prev | 0x00800000u;
        pqc_sha256_hw_run();
        UNROLL
        for (unsigned k = 0; k < nw; k++) {
            xw[k] = H[k];
        }
    }
    UNROLL
    for (unsigned k = 0; k < nw; k++) {
        x[k] = xw[k];
    }
}

/* ---------- the functions slhdsa-c calls, one set per n ---------- */

#define DEFINE_SET(NW)                                                                    \
    static void chain##NW(slh_var_t *var, uint8_t *tmp, const uint8_t *x, uint32_t i,     \
                          uint32_t s)                                                     \
    {                                                                                     \
        uint32_t w[NW];                                                                   \
        if (s == 0) {                                                                     \
            memmove(tmp, x, 4 * NW);                                                      \
            return;                                                                       \
        }                                                                                 \
        if (i + s > 0x10000u) {                                                           \
            c3.up->chain(var, tmp, x, i, s); /* never the case in SLH-DSA */              \
            return;                                                                       \
        }                                                                                 \
        load_words(w, x, NW);                                                             \
        chain_words(var, w, i, s, NW);                                                    \
        store_words(tmp, w, NW);                                                          \
    }                                                                                     \
    static void prf##NW(slh_var_t *var, uint8_t *h)                                       \
    {                                                                                     \
        uint32_t w[NW];                                                                   \
        prf_words(var, w, NW);                                                            \
        store_words(h, w, NW);                                                            \
    }                                                                                     \
    static void f##NW(slh_var_t *var, uint8_t *h, const uint8_t *m1)                      \
    {                                                                                     \
        uint32_t w[NW];                                                                   \
        load_words(w, m1, NW);                                                            \
        f_words(var, w, NW);                                                              \
        store_words(h, w, NW);                                                            \
    }                                                                                     \
    /* WOTS+ secret value, then s chain steps */                                          \
    static void wots_chain##NW(slh_var_t *var, uint8_t *tmp, uint32_t s)                  \
    {                                                                                     \
        uint32_t w[NW];                                                                   \
        adrs_set_type(var, ADRS_WOTS_PRF);                                                \
        adrs_set_tree_index(var, 0);                                                      \
        prf_words(var, w, NW);                                                            \
        adrs_set_type(var, ADRS_WOTS_HASH);                                               \
        adrs_set_tree_index(var, 0);                                                      \
        if (s != 0) {                                                                     \
            chain_words(var, w, 0, s, NW);                                                \
        }                                                                                 \
        store_words(tmp, w, NW);                                                          \
    }                                                                                     \
    /* FORS secret value, hashed once more if s == 1 */                                   \
    static void fors_hash##NW(slh_var_t *var, uint8_t *tmp, uint32_t s)                   \
    {                                                                                     \
        uint32_t w[NW];                                                                   \
        adrs_set_type(var, ADRS_FORS_PRF);                                                \
        adrs_set_tree_height(var, 0);                                                     \
        prf_words(var, w, NW);                                                            \
        if (s == 1) {                                                                     \
            adrs_set_type(var, ADRS_FORS_TREE);                                           \
            f_words(var, w, NW);                                                          \
        }                                                                                 \
        store_words(tmp, w, NW);                                                          \
    }

DEFINE_SET(4)
DEFINE_SET(6)
DEFINE_SET(8)

/* ---------- several hashes at once: WOTS+ public keys and FORS leaves ---------- */

#define NW 4
#include "slh_sha2_c3_tmpl.h"
#undef NW
#define NW 6
#include "slh_sha2_c3_tmpl.h"
#undef NW
#define NW 8
#include "slh_sha2_c3_tmpl.h"
#undef NW

/* ---------- n = 16 only: H and T_l are SHA-256 too ---------- */

/* H(PK.seed, ADRS, m1 || m2): 22 + 32 bytes, still one block. */
static void h4(slh_var_t *var, uint8_t *h, const uint8_t *m1, const uint8_t *m2)
{
    uint32_t w[8], lo5;

    load_words(w, m1, 4);
    load_words(w + 4, m2, 4);
    sync();
    lo5 = head(var);
    put(14, 0);
    put(15, LENW(22 + 32));
    load_mid();
    put_data(lo5, w, 8);
    /* words 10..13 now hold data; make the next F or PRF zero them again */
    c3.sh[10] = c3.sh[11] = c3.sh[12] = c3.sh[13] = 0xffffffffu;
    pqc_sha256_hw_run();
    read_out(w, 4);
    store_words(h, w, 4);
}

/* T_l(PK.seed, ADRS, m): any length, several blocks. The next block is
 * written while the accelerator is still busy with the previous one. */
static void tl4(slh_var_t *var, uint8_t *h, const uint8_t *m, size_t m_sz)
{
    uint32_t blk[16], w[4];
    uint8_t *b = (uint8_t *)blk;
    size_t pos = 22, total = 22 + m_sz;

    sync();
    adrsc_22(var, b);
    load_mid();
    while (m_sz > 0) {
        size_t take = 64 - pos;
        if (take > m_sz) {
            take = m_sz;
        }
        memcpy(b + pos, m, take);
        pos += take;
        m += take;
        m_sz -= take;
        if (pos == 64) {
            UNROLL
            for (unsigned k = 0; k < 16; k++) {
                M[k] = blk[k];
            }
            pqc_sha256_hw_run_nowait();
            pos = 0;
        }
    }
    b[pos++] = 0x80;
    if (pos > 56) {
        memset(b + pos, 0, 64 - pos);
        UNROLL
        for (unsigned k = 0; k < 16; k++) {
            M[k] = blk[k];
        }
        pqc_sha256_hw_run_nowait();
        pos = 0;
    }
    memset(b + pos, 0, 60 - pos);
    blk[15] = __builtin_bswap32((uint32_t)((64 + total) * 8));
    UNROLL
    for (unsigned k = 0; k < 16; k++) {
        M[k] = blk[k];
    }
    pqc_sha256_hw_run_nowait();
    memcpy(c3.sh, blk, sizeof(blk)); /* the registers hold this block now */
    pqc_sha256_hw_wait();
    read_out(w, 4);
    store_words(h, w, 4);
}

/* ---------- n = 24 and 32: H and T_l are SHA-512, in software ---------- */

static inline uint32_t bswap32(uint32_t x)
{
    return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24);
}

/* The first n bytes of the chaining value, as a byte string. */
INLINE void out512(uint8_t *h, const uint32_t *st, size_t n)
{
    uint32_t w[8];
    for (size_t i = 0; i < n / 4; i++) {
        w[i] = bswap32(st[i]);
    }
    memcpy(h, w, n);
}

/* H(PK.seed, ADRS, m1 || m2): one block after the stored midstate. The
 * block is put together from words: ADRSc and the data as a stream of
 * little-endian words (the data starts in the middle of word 5), each then
 * turned into the big-endian word SHA-512 wants. Padding and length are
 * already in c3.w512. */
static void h512(slh_var_t *var, uint8_t *h, const uint8_t *m1, const uint8_t *m2)
{
    const unsigned nw = var->prm->n / 4, last = 5 + 2 * nw;
    const uint32_t *a = var->adrs->u32;
    const uint32_t a2 = a[2], a3 = a[3], a5 = a[5], a6 = a[6], a7 = a[7];
    uint32_t st[16], x[16], prev;
    uint32_t *w = c3.w512;

    load_words(x, m1, nw);
    load_words(x + nw, m2, nw);
    for (unsigned k = 0; k < 16; k++) {
        st[k] = c3.mid512[k];
    }
    w[0] = bswap32((a[0] >> 24) | (a2 << 8));
    w[1] = bswap32((a2 >> 24) | (a3 << 8));
    w[2] = bswap32((a3 >> 24) | ((a[4] >> 24) << 8) | (a5 << 16));
    w[3] = bswap32((a5 >> 16) | (a6 << 16));
    w[4] = bswap32((a6 >> 16) | (a7 << 16));
    prev = a7 >> 16;
    for (unsigned k = 0; k < 2 * nw; k++) {
        w[5 + k] = bswap32(prev | (x[k] << 16));
        prev = x[k] >> 16;
    }
    w[last] = bswap32(prev | 0x00800000u);
    pqc_sha512_compress_rv32(st, w);
    out512(h, st, 4 * nw);
}

/* T_l(PK.seed, ADRS, m): any length. */
static void tl512(slh_var_t *var, uint8_t *h, const uint8_t *m, size_t m_sz)
{
    uint32_t st[16], w[32], blkw[32];
    uint8_t *blk = (uint8_t *)blkw;
    size_t pos = 22;
    const uint64_t bits = (uint64_t)(128 + 22 + m_sz) * 8;

    memcpy(st, c3.mid512, sizeof(st));
    adrsc_22(var, blk);
    while (m_sz > 0) {
        size_t take;
        if (pos == 0 && m_sz >= 128) { /* a whole block: straight from the input */
            for (unsigned k = 0; k < 32; k++) {
                w[k] = ((uint32_t)m[4 * k] << 24) | ((uint32_t)m[4 * k + 1] << 16) |
                       ((uint32_t)m[4 * k + 2] << 8) | m[4 * k + 3];
            }
            pqc_sha512_compress_rv32(st, w);
            m += 128;
            m_sz -= 128;
            continue;
        }
        take = 128 - pos;
        if (take > m_sz) {
            take = m_sz;
        }
        memcpy(blk + pos, m, take);
        pos += take;
        m += take;
        m_sz -= take;
        if (pos == 128) {
            for (unsigned k = 0; k < 32; k++) {
                w[k] = bswap32(blkw[k]);
            }
            pqc_sha512_compress_rv32(st, w);
            pos = 0;
        }
    }
    blk[pos++] = 0x80;
    if (pos > 112) {
        memset(blk + pos, 0, 128 - pos);
        for (unsigned k = 0; k < 32; k++) {
            w[k] = bswap32(blkw[k]);
        }
        pqc_sha512_compress_rv32(st, w);
        pos = 0;
    }
    memset(blk + pos, 0, 128 - pos);
    for (unsigned k = 0; k < 30; k++) {
        w[k] = bswap32(blkw[k]);
    }
    w[30] = (uint32_t)(bits >> 32);
    w[31] = (uint32_t)bits;
    pqc_sha512_compress_rv32(st, w);
    out512(h, st, var->prm->n);
}

/* ---------- key context ---------- */

static void c3_mk_var(slh_var_t *var, const uint8_t *pk, const uint8_t *sk, const slh_param_t *prm)
{
    const unsigned nw = prm->n / 4;
    uint32_t s[8], prev = 0;

    c3.up = (prm->n == 16)   ? &slh_dsa_sha2_128s
            : (prm->n == 24) ? &slh_dsa_sha2_192s
                             : &slh_dsa_sha2_256s;
    pqc_sha256_hw_init();
    c3.up->mk_var(var, pk, sk, prm); /* slhdsa-c: copies the key, computes the midstates */
    memcpy(c3.mid, var->sha2_256_pk_seed.s, 32);
    if (prm->n > 16) {
        const u32a *p = (const u32a *)(const void *)var->sha2_512_pk_seed.s;
        for (unsigned k = 0; k < 16; k++) {
            c3.mid512[k] = bswap32(p[k]);
        }
        /* block of H: words after the data are zero, the last is the length */
        memset(c3.w512, 0, sizeof(c3.w512));
        c3.w512[31] = (128 + 22 + 2 * prm->n) * 8;
    }

    memset(s, 0, sizeof(s));
    if (sk != NULL) {
        memcpy(s, var->sk_seed, prm->n);
    }
    for (unsigned k = 0; k < nw; k++) {
        c3.prf[k] = prev | (s[k] << 16);
        prev = s[k] >> 16;
    }
    c3.prf[nw] = prev | 0x00800000u;

    memset(c3.sh, 0, sizeof(c3.sh));
    slh_c3_m_dirty = 1; /* have the registers match the copy before first use */
}

static void make(slh_param_t *dst, const slh_param_t *up)
{
    *dst = *up;
    dst->mk_var = c3_mk_var;
    switch (up->n) {
    case 16:
        dst->chain = chain4;
        dst->wots_chain = wots_chain4;
        dst->fors_hash = fors_hash4;
        dst->prf = prf4;
        dst->h_f = f4;
        dst->h_h = h4;
        dst->h_t = tl4;
        dst->wots_pkgen = wots_pkgen4;
        dst->fors_leaf2 = fors_leaf24;
        dst->wots_chains = wots_chains4;
        break;
    case 24:
        dst->chain = chain6;
        dst->wots_chain = wots_chain6;
        dst->fors_hash = fors_hash6;
        dst->prf = prf6;
        dst->h_f = f6;
        dst->h_h = h512;
        dst->h_t = tl512;
        dst->wots_pkgen = wots_pkgen6;
        dst->fors_leaf2 = fors_leaf26;
        dst->wots_chains = wots_chains6;
        break;
    default:
        dst->chain = chain8;
        dst->wots_chain = wots_chain8;
        dst->fors_hash = fors_hash8;
        dst->prf = prf8;
        dst->h_f = f8;
        dst->h_h = h512;
        dst->h_t = tl512;
        dst->wots_pkgen = wots_pkgen8;
        dst->fors_leaf2 = fors_leaf28;
        dst->wots_chains = wots_chains8;
        break;
    }
}

void slh_c3_init(void)
{
    make(&slh_c3_sha2_128s, &slh_dsa_sha2_128s);
    make(&slh_c3_sha2_128f, &slh_dsa_sha2_128f);
    make(&slh_c3_sha2_192s, &slh_dsa_sha2_192s);
    make(&slh_c3_sha2_192f, &slh_dsa_sha2_192f);
    make(&slh_c3_sha2_256s, &slh_dsa_sha2_256s);
    make(&slh_c3_sha2_256f, &slh_dsa_sha2_256f);
}

void slh_c3_wipe(void)
{
    volatile uint32_t *p = (volatile uint32_t *)c3.prf;
    for (unsigned i = 0; i < sizeof(c3.prf) / 4; i++) {
        p[i] = 0;
    }
    for (int i = 5; i < 14; i++) {
        M[i] = 0; /* the last PRF block held secret key words */
    }
    slh_c3_m_dirty = 1;
}
