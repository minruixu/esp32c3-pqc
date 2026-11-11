/* LMS and HSS hash-based signatures; see lms.h. Follows RFC 8554 and
 * RFC 9858 section by section; names are the ones used there. */
#include <string.h>
#include "fips202.h"
#include "lms.h"
#include "pqc_prims.h"
#include "pqc_sm3.h"
#include "sha2.h"

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "the direct SHAKE path assumes a little-endian machine"
#endif
#if LMS_MAX_LAYERS < 2
#error "LMS_MAX_LAYERS must be at least 2"
#endif

#define D_PBLC 0x8080
#define D_MESG 0x8181
#define D_LEAF 0x8282
#define D_INTR 0x8383

int lms_fast = 0;
int (*lms_chain_accel)(const lms_param_t *P, const uint8_t *pre, unsigned i, unsigned from,
                       unsigned to, uint8_t *tmp);

#define H_SHA256 0
#define H_SHAKE 1
#define H_SM3 2

static const uint32_t sha256_iv[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
static const uint32_t sm3_iv[8] = {0x7380166fu, 0x4914b2b9u, 0x172442d7u, 0xda8a0600u,
                                   0xa96f30bcu, 0x163138aau, 0xe38dee4du, 0xb0fb0e4eu};

/* SHA-256 and SM3 share the block size, the padding and the shape of the
 * compression function, so one code path serves both. */
static void md_init(unsigned hash, uint32_t st[8])
{
    memcpy(st, hash == H_SM3 ? sm3_iv : sha256_iv, 32);
}

static void md_compress(unsigned hash, uint32_t st[8], const uint8_t *blocks, size_t nblocks)
{
    if (hash == H_SM3) {
        pqc_sm3_compress(st, blocks, nblocks);
    } else {
        pqc_sha256_compress(st, blocks, nblocks);
    }
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void put_u16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static uint32_t get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* ------------------------------------------------------------------ */
/* Parameter sets (RFC 8554 tables 1 and 2, RFC 9858 tables 1 and 2)   */
/* ------------------------------------------------------------------ */
int lms_param_init(lms_param_t *P, uint32_t lms_type, uint32_t ots_type)
{
    static const uint16_t p32[4] = {265, 133, 67, 34}, p24[4] = {200, 101, 51, 26};
    static const uint8_t ls32[4] = {7, 6, 4, 0}, ls24[4] = {8, 6, 4, 0};
    unsigned fam_lms, fam_ots, wi;

    if (lms_type >= 0x05 && lms_type <= 0x09 && ots_type == 0x0e) {
        /* LMS with SM3, W=8, as numbered by GmSSL (LMS_SM3_M32_H5.. with
         * LMOTS_SM3_N32_W8). The pair cannot be mistaken for an RFC 9858
         * set: there, type 0x0e only goes with the SHAKE256/192 trees. */
        P->lms_type = lms_type;
        P->ots_type = ots_type;
        P->shake = H_SM3;
        P->n = 32;
        P->h = (uint8_t)(5 + 5 * (lms_type - 5));
        P->w = 8;
        P->p = 34;
        P->ls = 0;
        return 0;
    }
    if (lms_type < 0x05 || lms_type > 0x18 || ots_type < 0x01 || ots_type > 0x10) {
        return -1;
    }
    fam_lms = (lms_type - 5) / 5;  /* 0: SHA-256/256, 1: SHA-256/192, 2: SHAKE/256, 3: SHAKE/192 */
    fam_ots = (ots_type - 1) / 4;
    if (fam_lms != fam_ots) {
        return -1;                 /* tree and one-time key must use the same hash */
    }
    wi = (ots_type - 1) % 4;
    P->lms_type = lms_type;
    P->ots_type = ots_type;
    P->shake = (uint8_t)(fam_lms >= 2 ? H_SHAKE : H_SHA256);
    P->n = (uint8_t)((fam_lms & 1) ? 24 : 32);
    P->h = (uint8_t)(5 + 5 * ((lms_type - 5) % 5));
    P->w = (uint8_t)(1u << wi);
    P->p = (P->n == 32) ? p32[wi] : p24[wi];
    P->ls = (P->n == 32) ? ls32[wi] : ls24[wi];
    return 0;
}

size_t lms_pk_bytes(const lms_param_t *P) { return 24u + P->n; }

static size_t ots_sig_bytes(const lms_param_t *P) { return 4u + (size_t)P->n * (P->p + 1u); }

size_t lms_sig_bytes(const lms_param_t *P)
{
    return 4u + ots_sig_bytes(P) + 4u + (size_t)P->h * P->n;
}

/* ------------------------------------------------------------------ */
/* Hashing                                                             */
/* ------------------------------------------------------------------ */

/* Incremental hash for the few long inputs (message, one-time public key). */
typedef struct {
    uint8_t shake, n;
    union {
        struct {
            uint32_t st[8];
            uint8_t buf[64];
            size_t total;
        } sha;
        shake256incctx xof;
    } u;
} lhash_t;

static void lhash_init(lhash_t *c, const lms_param_t *P)
{
    c->shake = P->shake;
    c->n = P->n;
    if (c->shake == H_SHAKE) {
        shake256_inc_init(&c->u.xof);
    } else {
        md_init(c->shake, c->u.sha.st);
        c->u.sha.total = 0;
    }
}

static void lhash_update(lhash_t *c, const uint8_t *in, size_t len)
{
    size_t have;

    if (c->shake == H_SHAKE) {
        shake256_inc_absorb(&c->u.xof, in, len);
        return;
    }
    have = c->u.sha.total & 63;
    c->u.sha.total += len;
    if (have) {
        size_t take = 64 - have;
        if (take > len) {
            take = len;
        }
        memcpy(c->u.sha.buf + have, in, take);
        in += take;
        len -= take;
        if (have + take < 64) {
            return;
        }
        md_compress(c->shake, c->u.sha.st, c->u.sha.buf, 1);
    }
    if (len >= 64) {
        md_compress(c->shake, c->u.sha.st, in, len / 64);
        in += len & ~(size_t)63;
        len &= 63;
    }
    memcpy(c->u.sha.buf, in, len);
}

static void lhash_final(lhash_t *c, uint8_t *out)
{
    size_t have;
    unsigned i;

    if (c->shake == H_SHAKE) {
        shake256_inc_finalize(&c->u.xof);
        shake256_inc_squeeze(out, c->n, &c->u.xof);
        return;
    }
    have = c->u.sha.total & 63;
    c->u.sha.buf[have++] = 0x80;
    if (have > 56) {
        memset(c->u.sha.buf + have, 0, 64 - have);
        md_compress(c->shake, c->u.sha.st, c->u.sha.buf, 1);
        have = 0;
    }
    memset(c->u.sha.buf + have, 0, 60 - have);
    put_u32(c->u.sha.buf + 60, (uint32_t)(c->u.sha.total << 3));
    c->u.sha.buf[59] = (uint8_t)(c->u.sha.total >> 29);
    md_compress(c->shake, c->u.sha.st, c->u.sha.buf, 1);
    for (i = 0; i < c->n / 4u; i++) {
        put_u32(out + 4 * i, c->u.sha.st[i]);
    }
}

/* out = first n bytes of H(in[0..len)), for len <= 119. All the hashes on
 * the hot paths are of this kind: 47 or 55 bytes in a Winternitz chain, 46
 * or 54 for a leaf, 70 or 86 for an inner node. */
static void hash_short(const lms_param_t *P, uint8_t *out, const uint8_t *in, size_t len)
{
    if (!lms_fast) {
        if (P->shake == H_SHAKE) {
            shake256(out, P->n, in, len);
        } else if (P->shake == H_SM3) {
            pqc_sm3(out, in, len);
        } else {
            uint8_t d[32];
            sha256(d, in, len);
            memcpy(out, d, P->n);
        }
        return;
    }
    if (P->shake == H_SHAKE) {
        uint64_t st[25];
        uint8_t *b = (uint8_t *)st;

        memcpy(b, in, len);
        memset(b + len, 0, sizeof(st) - len);
        b[len] = 0x1f;
        b[135] ^= 0x80;
        pqc_keccakf1600(st);
        memcpy(out, b, P->n);
    } else {
        uint8_t blk[128];
        uint32_t st[8];
        size_t nb = (len < 56) ? 1 : 2;
        unsigned i;

        memcpy(blk, in, len);
        memset(blk + len, 0, 64 * nb - len);
        blk[len] = 0x80;
        put_u16(blk + 64 * nb - 2, (unsigned)(len << 3));
        md_init(P->shake, st);
        md_compress(P->shake, st, blk, nb);
        for (i = 0; i < P->n / 4u; i++) {
            put_u32(out + 4 * i, st[i]);
        }
    }
}

/* Winternitz chain (RFC 8554, Algorithm 1): steps `from` .. `to`-1 applied to
 * tmp, for one-time key (I, q) and digit i. pre = I || u32str(q). */
static void ots_chain(const lms_param_t *P, const uint8_t *pre, unsigned i, unsigned from,
                      unsigned to, uint8_t *tmp)
{
    unsigned n = P->n, j, k;

    if (from >= to) {
        return;
    }
    if (lms_fast && lms_chain_accel && lms_chain_accel(P, pre, i, from, to, tmp)) {
        return;
    }
    if (!lms_fast) {
        uint8_t buf[23 + LMS_MAX_N];

        memcpy(buf, pre, 20);
        put_u16(buf + 20, i);
        for (j = from; j < to; j++) {
            buf[22] = (uint8_t)j;
            memcpy(buf + 23, tmp, n);
            hash_short(P, tmp, buf, 23 + n);
        }
        return;
    }
    if (P->shake == H_SHAKE) {
        /* One permutation per step. Only j and the running value change. */
        uint64_t st[25];
        uint8_t *b = (uint8_t *)st;
        uint8_t head[24];

        memcpy(head, pre, 20);
        put_u16(head + 20, i);
        for (j = from; j < to; j++) {
            memset(st, 0, sizeof(st));
            memcpy(b, head, 22);
            b[22] = (uint8_t)j;
            memcpy(b + 23, tmp, n);
            b[23 + n] = 0x1f;
            b[135] = 0x80;
            pqc_keccakf1600(st);
            memcpy(tmp, b, n);
        }
    } else {
        /* The 23 + n byte input and its padding are exactly one SHA-256 (or
         * SM3) block. Build it once; per step write j and the previous output. */
        uint8_t blk[64];
        uint32_t st[8];

        memcpy(blk, pre, 20);
        put_u16(blk + 20, i);
        memcpy(blk + 23, tmp, n);
        memset(blk + 23 + n, 0, 64 - 23 - n);
        blk[23 + n] = 0x80;
        put_u16(blk + 62, (23 + n) << 3);
        for (j = from; j < to; j++) {
            blk[22] = (uint8_t)j;
            md_init(P->shake, st);
            md_compress(P->shake, st, blk, 1);
            for (k = 0; k < n / 4; k++) {
                put_u32(blk + 23 + 4 * k, st[k]);
            }
        }
        memcpy(tmp, blk + 23, n);
    }
}

/* ------------------------------------------------------------------ */
/* LM-OTS                                                              */
/* ------------------------------------------------------------------ */
static unsigned coef(const uint8_t *S, unsigned i, unsigned w)
{
    return ((1u << w) - 1) & ((unsigned)S[(i * w) / 8] >> (8 - (w * (i % (8 / w)) + w)));
}

/* Q || Cksm(Q) (RFC 8554 section 4.4) */
static void append_checksum(const lms_param_t *P, uint8_t *Q)
{
    unsigned sum = 0, i, digits = (P->n * 8u) / P->w;

    for (i = 0; i < digits; i++) {
        sum += ((1u << P->w) - 1) - coef(Q, i, P->w);
    }
    put_u16(Q + P->n, (sum << P->ls) & 0xffff);
}

/* Q = H(I || u32str(q) || u16str(D_MESG) || C || message), with checksum */
static void message_digest(const lms_param_t *P, const uint8_t *pre, const uint8_t *C,
                           const uint8_t *msg, size_t msglen, uint8_t *Q)
{
    lhash_t c;
    uint8_t d[2];

    put_u16(d, D_MESG);
    lhash_init(&c, P);
    lhash_update(&c, pre, 20);
    lhash_update(&c, d, 2);
    lhash_update(&c, C, P->n);
    lhash_update(&c, msg, msglen);
    lhash_final(&c, Q);
    append_checksum(P, Q);
}

/* Secret values come from the seed (RFC 8554 Appendix A):
 * H(I || u32str(q) || u16str(i) || u8str(0xff) || SEED). Index 0xfffd gives
 * the signature randomizer and 0xfffe / 0xffff the seed and I of a child
 * tree, as in Cisco's hash-sigs. */
static void derive(const lms_param_t *P, const uint8_t *pre, unsigned i, const uint8_t *seed,
                   uint8_t *out)
{
    uint8_t buf[23 + LMS_MAX_N];

    memcpy(buf, pre, 20);
    put_u16(buf + 20, i);
    buf[22] = 0xff;
    memcpy(buf + 23, seed, P->n);
    hash_short(P, out, buf, 23u + P->n);
}

/* K: the one-time public key of leaf q (RFC 8554 section 4.3) */
static void ots_public(const lms_key_t *k, uint32_t q, uint8_t *K)
{
    const lms_param_t *P = &k->P;
    uint8_t pre[22], tmp[LMS_MAX_N];
    lhash_t c;
    unsigned i;

    memcpy(pre, k->I, 16);
    put_u32(pre + 16, q);
    put_u16(pre + 20, D_PBLC);
    lhash_init(&c, P);
    lhash_update(&c, pre, 22);
    for (i = 0; i < P->p; i++) {
        derive(P, pre, i, k->seed, tmp);
        ots_chain(P, pre, i, 0, (1u << P->w) - 1, tmp);
        lhash_update(&c, tmp, P->n);
    }
    lhash_final(&c, K);
}

/* LM-OTS signature (RFC 8554 Algorithm 3), written to sig */
static void ots_sign(const lms_key_t *k, uint32_t q, const uint8_t *C, const uint8_t *msg,
                     size_t msglen, uint8_t *sig)
{
    const lms_param_t *P = &k->P;
    uint8_t pre[20], Q[LMS_MAX_N + 2];
    unsigned i, n = P->n;

    memcpy(pre, k->I, 16);
    put_u32(pre + 16, q);
    put_u32(sig, P->ots_type);
    memcpy(sig + 4, C, n);
    message_digest(P, pre, C, msg, msglen, Q);
    for (i = 0; i < P->p; i++) {
        uint8_t *y = sig + 4 + n + (size_t)i * n;
        derive(P, pre, i, k->seed, y);
        ots_chain(P, pre, i, 0, coef(Q, i, P->w), y);
    }
}

/* Kc: the public key candidate of a signature (RFC 8554 Algorithm 4b) */
static void ots_candidate(const lms_param_t *P, const uint8_t *I, uint32_t q, const uint8_t *msg,
                          size_t msglen, const uint8_t *sig, uint8_t *Kc)
{
    uint8_t pre[22], Q[LMS_MAX_N + 2], tmp[LMS_MAX_N];
    lhash_t c;
    unsigned i, n = P->n;

    memcpy(pre, I, 16);
    put_u32(pre + 16, q);
    message_digest(P, pre, sig + 4, msg, msglen, Q);
    put_u16(pre + 20, D_PBLC);
    lhash_init(&c, P);
    lhash_update(&c, pre, 22);
    for (i = 0; i < P->p; i++) {
        memcpy(tmp, sig + 4 + n + (size_t)i * n, n);
        ots_chain(P, pre, i, coef(Q, i, P->w), (1u << P->w) - 1, tmp);
        lhash_update(&c, tmp, n);
    }
    lhash_final(&c, Kc);
}

/* ------------------------------------------------------------------ */
/* LMS tree                                                            */
/* ------------------------------------------------------------------ */

/* T[r] = H(I || u32str(r) || u16str(D_LEAF) || K) */
static void leaf_hash(const lms_param_t *P, const uint8_t *I, uint32_t r, const uint8_t *K,
                      uint8_t *out)
{
    uint8_t buf[22 + LMS_MAX_N];

    memcpy(buf, I, 16);
    put_u32(buf + 16, r);
    put_u16(buf + 20, D_LEAF);
    memcpy(buf + 22, K, P->n);
    hash_short(P, out, buf, 22u + P->n);
}

/* T[r] = H(I || u32str(r) || u16str(D_INTR) || T[2r] || T[2r+1]) */
static void inner_hash(const lms_param_t *P, const uint8_t *I, uint32_t r, const uint8_t *left,
                       const uint8_t *right, uint8_t *out)
{
    uint8_t buf[22 + 2 * LMS_MAX_N];

    memcpy(buf, I, 16);
    put_u32(buf + 16, r);
    put_u16(buf + 20, D_INTR);
    memcpy(buf + 22, left, P->n);
    memcpy(buf + 22 + P->n, right, P->n);
    hash_short(P, out, buf, 22u + 2u * P->n);
}

void lms_leaf(const lms_key_t *k, uint32_t q, uint8_t *out)
{
    uint8_t K[LMS_MAX_N];

    ots_public(k, q, K);
    leaf_hash(&k->P, k->I, ((uint32_t)1 << k->P.h) + q, K, out);
}

int lms_verify(const uint8_t *pk, size_t pklen, const uint8_t *msg, size_t msglen,
               const uint8_t *sig, size_t siglen)
{
    lms_param_t P;
    uint8_t tmp[LMS_MAX_N];
    const uint8_t *I, *path;
    uint32_t q, node;
    unsigned i, n, diff = 0;

    if (pklen < 8 || lms_param_init(&P, get_u32(pk), get_u32(pk + 4)) != 0) {
        return -1;
    }
    n = P.n;
    if (pklen != lms_pk_bytes(&P) || siglen != lms_sig_bytes(&P)) {
        return -1;
    }
    I = pk + 8;
    q = get_u32(sig);
    if (get_u32(sig + 4) != P.ots_type || get_u32(sig + 4 + ots_sig_bytes(&P)) != P.lms_type ||
        q >= ((uint32_t)1 << P.h)) {
        return -1;
    }
    path = sig + 8 + ots_sig_bytes(&P);

    ots_candidate(&P, I, q, msg, msglen, sig + 4, tmp);
    node = ((uint32_t)1 << P.h) + q;
    leaf_hash(&P, I, node, tmp, tmp);
    for (i = 0; i < P.h; i++) {
        if (node & 1) {
            inner_hash(&P, I, node >> 1, path + (size_t)i * n, tmp, tmp);
        } else {
            inner_hash(&P, I, node >> 1, tmp, path + (size_t)i * n, tmp);
        }
        node >>= 1;
    }
    for (i = 0; i < n; i++) {
        diff |= tmp[i] ^ pk[24 + i];
    }
    return diff ? -1 : 0;
}

int hss_verify(const uint8_t *pk, size_t pklen, const uint8_t *msg, size_t msglen,
               const uint8_t *sig, size_t siglen)
{
    lms_param_t P;
    const uint8_t *key = pk + 4;
    size_t keylen = pklen - 4, off = 4;
    uint32_t L, nspk, i;

    if (pklen < 12 || siglen < 4) {
        return -1;
    }
    L = get_u32(pk);
    nspk = get_u32(sig);
    if (L < 1 || L > 8 || nspk != L - 1) {
        return -1;
    }
    for (i = 0; i < nspk; i++) {
        size_t sl, pl;
        lms_param_t Pn;

        if (keylen < 8 || lms_param_init(&P, get_u32(key), get_u32(key + 4)) != 0) {
            return -1;
        }
        sl = lms_sig_bytes(&P);
        if (siglen < off + sl + 8 ||
            lms_param_init(&Pn, get_u32(sig + off + sl), get_u32(sig + off + sl + 4)) != 0) {
            return -1;
        }
        pl = lms_pk_bytes(&Pn);
        if (siglen < off + sl + pl) {
            return -1;
        }
        /* sig[i] signs pub[i+1] under the current key */
        if (lms_verify(key, keylen, sig + off + sl, pl, sig + off, sl) != 0) {
            return -1;
        }
        key = sig + off + sl;
        keylen = pl;
        off += sl + pl;
    }
    return lms_verify(key, keylen, msg, msglen, sig + off, siglen - off);
}

/* ------------------------------------------------------------------ */
/* Signing key: cached subtrees                                        */
/*                                                                     */
/* The tree is cut into layers of 5 levels. A node at height t (0 =    */
/* leaves) and index x within that height has number 2^(h-t) + x. In   */
/* layer l it sits at local depth d = 5l + 5 - t of subtree x >> d, at */
/* heap position 2^d + (x mod 2^d).                                    */
/* ------------------------------------------------------------------ */
static void cache_store(lms_key_t *k, unsigned t, uint32_t x, const uint8_t *node)
{
    int l;

    for (l = (int)(t / LMS_SUB_H); l >= 0 && l >= (int)(t / LMS_SUB_H) - 1; l--) {
        unsigned d;
        uint32_t s, local, here;

        if (l >= k->layers || t < (unsigned)l * LMS_SUB_H ||
            t > (unsigned)l * LMS_SUB_H + LMS_SUB_H) {
            continue;
        }
        d = (unsigned)l * LMS_SUB_H + LMS_SUB_H - t;
        s = x >> d;
        local = ((uint32_t)1 << d) + (x & (((uint32_t)1 << d) - 1));
        here = k->q >> (((unsigned)l + 1) * LMS_SUB_H);
        if (s == here) {
            memcpy(k->cur[l][local], node, k->P.n);
        } else if (l < k->layers - 1 && s == here + 1) {
            memcpy(k->nxt[l][local], node, k->P.n);
        }
    }
}

/* Node (t, x) by recursion over its leaves, filling the caches on the way. */
static void tree_node(lms_key_t *k, unsigned t, uint32_t x, uint8_t *out)
{
    if (t == 0) {
        lms_leaf(k, x, out);
    } else {
        uint8_t left[LMS_MAX_N], right[LMS_MAX_N];

        tree_node(k, t - 1, 2 * x, left);
        tree_node(k, t - 1, 2 * x + 1, right);
        inner_hash(&k->P, k->I, ((uint32_t)1 << (k->P.h - t)) + x, left, right, out);
    }
    cache_store(k, t, x, out);
}

/* Inner nodes of subtree s of layer l from its 32 cached leaves. */
static void subtree_finish(const lms_key_t *k, unsigned l, uint32_t s, uint8_t cache[][LMS_MAX_N])
{
    unsigned kx, d = LMS_SUB_H - 1;

    for (kx = LMS_SUB_NODES / 2 - 1; kx >= 1; kx--) {
        unsigned t;
        uint32_t x;

        while (kx < (1u << d)) {
            d--;
        }
        t = l * LMS_SUB_H + LMS_SUB_H - d;
        x = (s << d) + (kx - (1u << d));
        inner_hash(&k->P, k->I, ((uint32_t)1 << (k->P.h - t)) + x, cache[2 * kx], cache[2 * kx + 1],
                   cache[kx]);
    }
}

int lms_key_load(lms_key_t *k, uint32_t lms_type, uint32_t ots_type, const uint8_t *seed,
                 const uint8_t I[16], uint32_t q)
{
    uint8_t root[LMS_MAX_N];
    unsigned l;

    if (lms_param_init(&k->P, lms_type, ots_type) != 0 || k->P.h > LMS_SUB_H * LMS_MAX_LAYERS ||
        q > ((uint32_t)1 << k->P.h)) {
        return -1;
    }
    k->layers = (uint8_t)(k->P.h / LMS_SUB_H);
    memcpy(k->I, I, 16);
    memcpy(k->seed, seed, k->P.n);
    k->q = (q == ((uint32_t)1 << k->P.h)) ? q - 1 : q; /* position of the caches */
    tree_node(k, k->P.h, 0, root);
    for (l = 0; l + 1 < k->layers; l++) {
        uint32_t s = k->q >> ((l + 1) * LMS_SUB_H);
        k->nxt_ready[l] = (uint8_t)(s + 1 < ((uint32_t)1 << (k->P.h - (l + 1) * LMS_SUB_H)));
        k->th[l].top = 0;
    }
    k->q = q;
    return 0;
}

void lms_public_key(const lms_key_t *k, uint8_t *pk)
{
    put_u32(pk, k->P.lms_type);
    put_u32(pk + 4, k->P.ots_type);
    memcpy(pk + 8, k->I, 16);
    memcpy(pk + 24, k->cur[k->layers - 1][1], k->P.n);
}

uint32_t lms_remaining(const lms_key_t *k) { return ((uint32_t)1 << k->P.h) - k->q; }

/* After leaf q has been used: add one leaf to the next subtree of every
 * layer, and switch to it when q moves across a subtree boundary. */
static void advance(lms_key_t *k)
{
    const lms_param_t *P = &k->P;
    uint32_t q = k->q;
    unsigned l;

    for (l = 0; l + 1 < k->layers; l++) {
        unsigned sh = (l + 1) * LMS_SUB_H, target = l * LMS_SUB_H;
        uint32_t mask = ((uint32_t)1 << sh) - 1, s = q >> sh, t = q & mask;
        uint32_t q2 = ((s + 1) << sh) + t;
        unsigned top;

        if (k->nxt_ready[l] || s + 1 >= ((uint32_t)1 << (P->h - sh))) {
            continue;
        }
        /* treehash: push leaf q2, merge equal heights up to the target height */
        top = k->th[l].top;
        lms_leaf(k, q2, k->th[l].node[top]);
        k->th[l].height[top] = 0;
        top++;
        while (top >= 2 && k->th[l].height[top - 1] == k->th[l].height[top - 2]) {
            unsigned ht = k->th[l].height[top - 1] + 1u;
            inner_hash(P, k->I, ((uint32_t)1 << (P->h - ht)) + (q2 >> ht), k->th[l].node[top - 2],
                       k->th[l].node[top - 1], k->th[l].node[top - 2]);
            k->th[l].height[top - 2] = (uint8_t)ht;
            top--;
        }
        if (top == 1 && k->th[l].height[0] == target) {
            memcpy(k->nxt[l][LMS_SUB_NODES / 2 + (t >> target)], k->th[l].node[0], P->n);
            top = 0;
        }
        k->th[l].top = (uint8_t)top;
        if (t == mask) {
            subtree_finish(k, l, s + 1, k->nxt[l]);
            k->nxt_ready[l] = 1;
        }
    }
    k->q = ++q;
    if (q < ((uint32_t)1 << P->h)) {
        for (l = 0; l + 1 < k->layers; l++) {
            if ((q & (((uint32_t)1 << ((l + 1) * LMS_SUB_H)) - 1)) == 0) {
                memcpy(k->cur[l], k->nxt[l], sizeof(k->cur[l]));
                k->nxt_ready[l] = 0;
                k->th[l].top = 0;
            }
        }
    }
}

int lms_sign_c(lms_key_t *k, const uint8_t *C, const uint8_t *msg, size_t msglen, uint8_t *sig,
               size_t *siglen)
{
    const lms_param_t *P = &k->P;
    uint32_t q = k->q;
    uint8_t *p = sig;
    unsigned i, n = P->n;

    if (q >= ((uint32_t)1 << P->h)) {
        return -1;
    }
    if (k->persist && k->persist(k->persist_ctx, q + 1) != 0) {
        return -2;
    }
    put_u32(p, q);
    p += 4;
    ots_sign(k, q, C, msg, msglen, p);
    p += ots_sig_bytes(P);
    put_u32(p, P->lms_type);
    p += 4;
    for (i = 0; i < P->h; i++) {
        unsigned l = i / LMS_SUB_H, d = LMS_SUB_H - (i - l * LMS_SUB_H);
        uint32_t x = (q >> i) ^ 1;
        memcpy(p, k->cur[l][((uint32_t)1 << d) + (x & (((uint32_t)1 << d) - 1))], n);
        p += n;
    }
    *siglen = (size_t)(p - sig);
    advance(k);
    return 0;
}

int lms_sign(lms_key_t *k, const uint8_t *msg, size_t msglen, uint8_t *sig, size_t *siglen)
{
    uint8_t pre[20], C[LMS_MAX_N];

    memcpy(pre, k->I, 16);
    put_u32(pre + 16, k->q);
    derive(&k->P, pre, 0xfffd, k->seed, C);
    return lms_sign_c(k, C, msg, msglen, sig, siglen);
}

/* ------------------------------------------------------------------ */
/* HSS                                                                 */
/* ------------------------------------------------------------------ */

/* New tree at `level` (>= 1), with its public key signed by the tree above. */
static int hss_refresh(hss_key_t *h, int level)
{
    lms_key_t *parent, *child;
    uint8_t pre[20], cseed[LMS_MAX_N], ci[LMS_MAX_N], pk[24 + LMS_MAX_N];
    size_t sl, pl;

    if (level == 0) {
        return -1; /* the top tree is used up: the HSS key is exhausted */
    }
    parent = h->lv[level - 1];
    child = h->lv[level];
    if (lms_remaining(parent) == 0 && hss_refresh(h, level - 1) != 0) {
        return -1;
    }
    memcpy(pre, parent->I, 16);
    put_u32(pre + 16, parent->q);
    memset(cseed, 0, sizeof(cseed));
    derive(&parent->P, pre, 0xfffe, parent->seed, cseed);
    derive(&parent->P, pre, 0xffff, parent->seed, ci);
    if (lms_key_load(child, h->lms_type[level], h->ots_type[level], cseed, ci, 0) != 0) {
        return -1;
    }
    lms_public_key(child, pk);
    pl = lms_pk_bytes(&child->P);
    if (lms_sign(parent, pk, pl, h->spk[level - 1], &sl) != 0) {
        return -1;
    }
    memcpy(h->spk[level - 1] + sl, pk, pl);
    h->spk_len[level - 1] = sl + pl;
    return 0;
}

int hss_keygen(hss_key_t *h, const uint8_t *seed, const uint8_t I[16])
{
    int i;

    if (h->levels < 1 || h->levels > HSS_MAX_LEVELS ||
        lms_key_load(h->lv[0], h->lms_type[0], h->ots_type[0], seed, I, 0) != 0) {
        return -1;
    }
    for (i = 1; i < h->levels; i++) {
        if (hss_refresh(h, i) != 0) {
            return -1;
        }
    }
    return 0;
}

size_t hss_pk_bytes(const hss_key_t *h) { return 4 + lms_pk_bytes(&h->lv[0]->P); }

void hss_public_key(const hss_key_t *h, uint8_t *pk)
{
    put_u32(pk, (uint32_t)h->levels);
    lms_public_key(h->lv[0], pk + 4);
}

size_t hss_sig_bytes(const hss_key_t *h)
{
    size_t n = 4;
    int i;

    for (i = 0; i < h->levels; i++) {
        n += lms_sig_bytes(&h->lv[i]->P);
        if (i > 0) {
            n += lms_pk_bytes(&h->lv[i]->P);
        }
    }
    return n;
}

int hss_sign(hss_key_t *h, const uint8_t *msg, size_t msglen, uint8_t *sig, size_t *siglen)
{
    int L = h->levels, i;
    size_t off = 4, n;

    if (lms_remaining(h->lv[L - 1]) == 0 && hss_refresh(h, L - 1) != 0) {
        return -1;
    }
    put_u32(sig, (uint32_t)(L - 1));
    for (i = 0; i < L - 1; i++) {
        memcpy(sig + off, h->spk[i], h->spk_len[i]);
        off += h->spk_len[i];
    }
    if (lms_sign(h->lv[L - 1], msg, msglen, sig + off, &n) != 0) {
        return -1;
    }
    *siglen = off + n;
    return 0;
}
