/* Hash layer of XMSS: same functions as hash.c of the reference code, with a
 * run-time choice (xmss_fast) between the reference versions and versions
 * written for this project.
 *
 * What the fast versions do differently:
 *  - Every input is short and of fixed length, so the padded SHA-256 blocks
 *    (or the SHAKE state) are built in place and sent straight to the
 *    compression function (or the permutation); no hash context, no copies.
 *  - With SHA-256 and n = 32, PRF(key, x) hashes toByte(3,32) || key || x.
 *    Its first block depends only on the key, so the state after that block
 *    is kept. The public seed is the key of two PRF calls in every F and
 *    three in every H, which turns 6 compressions per F into 4 and 9 per H
 *    into 6. The same holds for PRF_keygen with the secret seed.
 *  - On the device, hash_hw.c takes over PRF, F and H of the SHA-256 sets
 *    through the three xmss_hw_* hooks and talks to the SHA accelerator
 *    directly.
 */
#include <string.h>
#include "fips202.h"
#include "hash.h"
#include "hash_address.h"
#include "params.h"
#include "pqc_prims.h"
#include "sha2.h"
#include "utils.h"
#include "xmss_port.h"

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "the direct SHAKE path assumes a little-endian machine"
#endif

#define PAD_F 0
#define PAD_H 1
#define PAD_PRF 3
#define PAD_PRF_KEYGEN 4

int xmss_fast = 0;
int (*xmss_hw_prf)(const xmss_params *params, unsigned char *out, const unsigned char in[32],
                   const unsigned char *key);
int (*xmss_hw_thash_f)(const xmss_params *params, unsigned char *out, const unsigned char *in,
                       const unsigned char *pub_seed, uint32_t addr[8]);
int (*xmss_hw_thash_h)(const xmss_params *params, unsigned char *out, const unsigned char *in,
                       const unsigned char *pub_seed, uint32_t addr[8]);

static const uint32_t iv256[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                  0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

void addr_to_bytes(unsigned char *bytes, const uint32_t addr[8])
{
    for (int i = 0; i < 8; i++) {
        put32(bytes + 4 * i, addr[i]);
    }
}

/* ---------- one-shot hash of a short input, built in place ---------- */

/* SHA-256 of in[0..len), len <= 183, first n bytes to out */
static void sha256_direct(unsigned char *out, unsigned n, const unsigned char *in, size_t len)
{
    unsigned char blk[192];
    uint32_t st[8];
    size_t nb = (len + 9 + 63) / 64;

    memcpy(blk, in, len);
    memset(blk + len, 0, 64 * nb - len);
    blk[len] = 0x80;
    blk[64 * nb - 2] = (unsigned char)(len >> 5);
    blk[64 * nb - 1] = (unsigned char)(len << 3);
    memcpy(st, iv256, sizeof(st));
    pqc_sha256_compress(st, blk, nb);
    for (unsigned i = 0; i < n / 4; i++) {
        put32(out + 4 * i, st[i]);
    }
}

/* Returns 0 if the direct path handled it, -1 if the caller must fall back. */
static int hash_direct(const xmss_params *params, unsigned char *out, const unsigned char *in,
                       size_t len)
{
    if (params->func == XMSS_SHA2 && params->n <= 32 && len <= 183) {
        sha256_direct(out, params->n, in, len);
        return 0;
    }
    if (params->func == XMSS_SHAKE128 || params->func == XMSS_SHAKE256) {
        size_t rate = (params->func == XMSS_SHAKE128) ? 168 : 136;

        if (len < rate) {
            uint64_t st[25];
            unsigned char *b = (unsigned char *)st;

            memcpy(b, in, len);
            memset(b + len, 0, sizeof(st) - len);
            b[len] = 0x1f;
            b[rate - 1] ^= 0x80;
            pqc_keccakf1600(st);
            memcpy(out, b, params->n);
            return 0;
        }
    }
    return -1;
}

/* ---------- SHA-256 state after the key-only first block of PRF ---------- */
typedef struct {
    uint32_t key[8];
    uint32_t st[8];
    unsigned char tag;
    unsigned char valid;
} mid_t;

#define MIDS 3 /* public seed, secret seed (PRF_keygen), secret PRF key */
static mid_t mids[MIDS];
static unsigned mid_next;

void xmss_hash_cache_clear(void)
{
    memset(mids, 0, sizeof(mids));
}

static const uint32_t *midstate(const unsigned char *key, unsigned tag)
{
    uint32_t kw[8];
    unsigned char blk[64];
    mid_t *m;

    memcpy(kw, key, 32);
    for (unsigned i = 0; i < MIDS; i++) {
        m = &mids[i];
        if (m->valid && m->tag == tag &&
            ((kw[0] ^ m->key[0]) | (kw[1] ^ m->key[1]) | (kw[2] ^ m->key[2]) | (kw[3] ^ m->key[3]) |
             (kw[4] ^ m->key[4]) | (kw[5] ^ m->key[5]) | (kw[6] ^ m->key[6]) | (kw[7] ^ m->key[7])) == 0) {
            return m->st;
        }
    }
    m = &mids[mid_next];
    mid_next = (mid_next + 1) % MIDS;
    memset(blk, 0, 32);
    blk[31] = (unsigned char)tag;
    memcpy(blk + 32, key, 32);
    memcpy(m->st, iv256, sizeof(iv256));
    pqc_sha256_compress(m->st, blk, 1);
    memcpy(m->key, kw, sizeof(kw));
    m->tag = (unsigned char)tag;
    m->valid = 1;
    return m->st;
}

/* Second (and last) block of PRF for SHA-256, n = 32: the 32-byte input
 * followed by the padding of a 96-byte message. */
static void prf_block_init(unsigned char blk[64], const unsigned char in[32])
{
    memcpy(blk, in, 32);
    memset(blk + 32, 0, 32);
    blk[32] = 0x80;
    blk[62] = 0x03; /* 96 bytes = 0x300 bits */
}

/* ---------- the hash layer ---------- */
int prf(const xmss_params *params, unsigned char *out, const unsigned char in[32],
        const unsigned char *key)
{
    unsigned char buf[64 + 64 + 32];

    if (!xmss_fast) {
        return xmss_ref_prf(params, out, in, key);
    }
    if (xmss_hw_prf && xmss_hw_prf(params, out, in, key)) {
        return 0;
    }
    if (params->func == XMSS_SHA2 && params->n == 32) {
        uint32_t st[8];
        unsigned char blk[64];

        memcpy(st, midstate(key, PAD_PRF), sizeof(st));
        prf_block_init(blk, in);
        pqc_sha256_compress(st, blk, 1);
        for (unsigned i = 0; i < 8; i++) {
            put32(out + 4 * i, st[i]);
        }
        return 0;
    }
    ull_to_bytes(buf, params->padding_len, PAD_PRF);
    memcpy(buf + params->padding_len, key, params->n);
    memcpy(buf + params->padding_len + params->n, in, 32);
    if (hash_direct(params, out, buf, params->padding_len + params->n + 32) == 0) {
        return 0;
    }
    return xmss_ref_prf(params, out, in, key);
}

int prf_keygen(const xmss_params *params, unsigned char *out, const unsigned char *in,
               const unsigned char *key)
{
    unsigned char buf[64 + 2 * 64 + 32];
    size_t len = params->padding_len + 2 * params->n + 32;

    if (!xmss_fast) {
        return xmss_ref_prf_keygen(params, out, in, key);
    }
    if (params->func == XMSS_SHA2 && params->n == 32) {
        /* toByte(4,32) || key is the cached block; in (64 bytes) and the
         * padding of a 128-byte message are the other two. */
        uint32_t st[8];
        unsigned char blk[128];

        memcpy(st, midstate(key, PAD_PRF_KEYGEN), sizeof(st));
        memcpy(blk, in, 64);
        memset(blk + 64, 0, 64);
        blk[64] = 0x80;
        blk[126] = 0x04; /* 128 bytes = 0x400 bits */
        pqc_sha256_compress(st, blk, 2);
        for (unsigned i = 0; i < 8; i++) {
            put32(out + 4 * i, st[i]);
        }
        return 0;
    }
    ull_to_bytes(buf, params->padding_len, PAD_PRF_KEYGEN);
    memcpy(buf + params->padding_len, key, params->n);
    memcpy(buf + params->padding_len + params->n, in, params->n + 32);
    if (hash_direct(params, out, buf, len) == 0) {
        return 0;
    }
    return xmss_ref_prf_keygen(params, out, in, key);
}

int hash_message(const xmss_params *params, unsigned char *out, const unsigned char *R,
                 const unsigned char *root, unsigned long long idx, unsigned char *m_with_prefix,
                 unsigned long long mlen)
{
    /* Once per signature and of arbitrary length: the reference code. */
    return xmss_ref_hash_message(params, out, R, root, idx, m_with_prefix, mlen);
}

int thash_h(const xmss_params *params, unsigned char *out, const unsigned char *in,
            const unsigned char *pub_seed, uint32_t addr[8])
{
    unsigned char buf[64 + 3 * 64];
    unsigned char bitmask[2 * 64];
    unsigned char addr_as_bytes[32];
    unsigned n = params->n, pl = params->padding_len, i;

    if (!xmss_fast) {
        return xmss_ref_thash_h(params, out, in, pub_seed, addr);
    }
    if (xmss_hw_thash_h && xmss_hw_thash_h(params, out, in, pub_seed, addr)) {
        return 0;
    }
    if (params->func == XMSS_SHA2 && n == 32) {
        const uint32_t *mid = midstate(pub_seed, PAD_PRF);
        uint32_t k[8], m0[8], m1[8], st[8];
        unsigned char pb[64], blk[192];

        set_key_and_mask(addr, 0);
        addr_to_bytes(addr_as_bytes, addr);
        prf_block_init(pb, addr_as_bytes);
        memcpy(k, mid, sizeof(k));
        pqc_sha256_compress(k, pb, 1);
        pb[31] = 1;
        memcpy(m0, mid, sizeof(m0));
        pqc_sha256_compress(m0, pb, 1);
        pb[31] = 2;
        memcpy(m1, mid, sizeof(m1));
        pqc_sha256_compress(m1, pb, 1);
        set_key_and_mask(addr, 2);

        /* toByte(1,32) || key || (in xor mask), 128 bytes, then padding */
        memset(blk, 0, 32);
        blk[31] = PAD_H;
        for (i = 0; i < 8; i++) {
            put32(blk + 32 + 4 * i, k[i]);
            put32(blk + 64 + 4 * i, m0[i]);
            put32(blk + 96 + 4 * i, m1[i]);
        }
        for (i = 0; i < 64; i++) {
            blk[64 + i] ^= in[i];
        }
        memset(blk + 128, 0, 64);
        blk[128] = 0x80;
        blk[190] = 0x04; /* 128 bytes = 0x400 bits */
        memcpy(st, iv256, sizeof(st));
        pqc_sha256_compress(st, blk, 3);
        for (i = 0; i < 8; i++) {
            put32(out + 4 * i, st[i]);
        }
        return 0;
    }

    ull_to_bytes(buf, pl, PAD_H);
    set_key_and_mask(addr, 0);
    addr_to_bytes(addr_as_bytes, addr);
    prf(params, buf + pl, addr_as_bytes, pub_seed);
    set_key_and_mask(addr, 1);
    addr_to_bytes(addr_as_bytes, addr);
    prf(params, bitmask, addr_as_bytes, pub_seed);
    set_key_and_mask(addr, 2);
    addr_to_bytes(addr_as_bytes, addr);
    prf(params, bitmask + n, addr_as_bytes, pub_seed);
    for (i = 0; i < 2 * n; i++) {
        buf[pl + n + i] = in[i] ^ bitmask[i];
    }
    if (hash_direct(params, out, buf, pl + 3 * n) == 0) {
        return 0;
    }
    /* fall back to the generic hash for the last step */
    if (params->func == XMSS_SHA2) {
        sha512(out, buf, pl + 3 * n);
    } else if (params->func == XMSS_SHAKE128) {
        shake128(out, n, buf, pl + 3 * n);
    } else {
        shake256(out, n, buf, pl + 3 * n);
    }
    return 0;
}

int thash_f(const xmss_params *params, unsigned char *out, const unsigned char *in,
            const unsigned char *pub_seed, uint32_t addr[8])
{
    unsigned char buf[64 + 2 * 64];
    unsigned char bitmask[64];
    unsigned char addr_as_bytes[32];
    unsigned n = params->n, pl = params->padding_len, i;

    if (!xmss_fast) {
        return xmss_ref_thash_f(params, out, in, pub_seed, addr);
    }
    if (xmss_hw_thash_f && xmss_hw_thash_f(params, out, in, pub_seed, addr)) {
        return 0;
    }
    if (params->func == XMSS_SHA2 && n == 32) {
        const uint32_t *mid = midstate(pub_seed, PAD_PRF);
        uint32_t k[8], m[8], st[8];
        unsigned char pb[64], blk[128];

        set_key_and_mask(addr, 0);
        addr_to_bytes(addr_as_bytes, addr);
        prf_block_init(pb, addr_as_bytes);
        memcpy(k, mid, sizeof(k));
        pqc_sha256_compress(k, pb, 1);
        pb[31] = 1;
        memcpy(m, mid, sizeof(m));
        pqc_sha256_compress(m, pb, 1);
        set_key_and_mask(addr, 1);

        /* toByte(0,32) || key || (in xor mask), 96 bytes, then padding */
        memset(blk, 0, 32);
        for (i = 0; i < 8; i++) {
            put32(blk + 32 + 4 * i, k[i]);
            put32(blk + 64 + 4 * i, m[i]);
        }
        for (i = 0; i < 32; i++) {
            blk[64 + i] ^= in[i];
        }
        memset(blk + 96, 0, 32);
        blk[96] = 0x80;
        blk[126] = 0x03; /* 96 bytes = 0x300 bits */
        memcpy(st, iv256, sizeof(st));
        pqc_sha256_compress(st, blk, 2);
        for (i = 0; i < 8; i++) {
            put32(out + 4 * i, st[i]);
        }
        return 0;
    }

    ull_to_bytes(buf, pl, PAD_F);
    set_key_and_mask(addr, 0);
    addr_to_bytes(addr_as_bytes, addr);
    prf(params, buf + pl, addr_as_bytes, pub_seed);
    set_key_and_mask(addr, 1);
    addr_to_bytes(addr_as_bytes, addr);
    prf(params, bitmask, addr_as_bytes, pub_seed);
    for (i = 0; i < n; i++) {
        buf[pl + n + i] = in[i] ^ bitmask[i];
    }
    if (hash_direct(params, out, buf, pl + 2 * n) == 0) {
        return 0;
    }
    if (params->func == XMSS_SHA2) {
        sha512(out, buf, pl + 2 * n);
    } else if (params->func == XMSS_SHAKE128) {
        shake128(out, n, buf, pl + 2 * n);
    } else {
        shake256(out, n, buf, pl + 2 * n);
    }
    return 0;
}
