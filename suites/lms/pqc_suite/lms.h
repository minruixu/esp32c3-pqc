/* LMS and HSS hash-based signatures (RFC 8554, NIST SP 800-208) with the
 * SHA-256/256, SHA-256/192, SHAKE256/256 and SHAKE256/192 parameter sets
 * (RFC 9858), and with SM3 in the numbering GmSSL uses. Written for this
 * project: no heap, bounded stack, and signing keys that cache 5-level
 * subtrees so that a signature never has to rebuild the tree. */
#ifndef LMS_H
#define LMS_H

#include <stddef.h>
#include <stdint.h>

#define LMS_MAX_N 32          /* hash output bytes */
#define LMS_MAX_H 25          /* tree height accepted by verification */
#define LMS_SUB_H 5           /* height of a cached subtree */
#define LMS_SUB_NODES 64      /* heap-ordered subtree, index 0 unused */
#ifndef LMS_MAX_LAYERS
#define LMS_MAX_LAYERS 3      /* signing keys: tree height up to 5 * this */
#endif
#define HSS_MAX_LEVELS 3

typedef struct {
    uint32_t lms_type, ots_type;
    uint8_t shake;            /* hash: 0 SHA-256, 1 SHAKE256, 2 SM3 */
    uint8_t n;                /* hash output bytes: 24 or 32 */
    uint8_t w;                /* Winternitz parameter: 1, 2, 4, 8 */
    uint8_t h;                /* tree height */
    uint8_t ls;               /* checksum shift */
    uint16_t p;               /* Winternitz chains per one-time key */
} lms_param_t;

/* 0: every hash goes through the generic one-shot hash API.
 * 1: single-block hashes are built in place and sent straight to the
 *    compression function / permutation. Same outputs either way. */
extern int lms_fast;

/* Optional replacement for the Winternitz chain when lms_fast is set: steps
 * `from` .. `to`-1 on tmp. Returns 1 if it did the work, 0 to fall back.
 * lms_hw.c provides one for the SHA accelerator of the ESP32-C3. */
extern int (*lms_chain_accel)(const lms_param_t *P, const uint8_t *pre, unsigned i, unsigned from,
                              unsigned to, uint8_t *tmp);
int lms_chain_sha256_hw(const lms_param_t *P, const uint8_t *pre, unsigned i, unsigned from,
                        unsigned to, uint8_t *tmp);

int lms_param_init(lms_param_t *P, uint32_t lms_type, uint32_t ots_type);
size_t lms_pk_bytes(const lms_param_t *P);
size_t lms_sig_bytes(const lms_param_t *P);

/* ---- verification (stateless); 0 = valid ---- */
int lms_verify(const uint8_t *pk, size_t pklen, const uint8_t *msg, size_t msglen,
               const uint8_t *sig, size_t siglen);
int hss_verify(const uint8_t *pk, size_t pklen, const uint8_t *msg, size_t msglen,
               const uint8_t *sig, size_t siglen);

/* ---- signing key of one LMS tree ----
 * Long-term secret: seed, I and the counter q. Everything else is a cache
 * that lms_key_load() rebuilds from those three values. */
typedef struct {
    lms_param_t P;
    uint8_t I[16];
    uint8_t seed[LMS_MAX_N];
    uint32_t q;                              /* next unused one-time key */
    uint8_t layers;                          /* h / 5 */
    uint8_t nxt_ready[LMS_MAX_LAYERS];
    /* Called with the next value of q before a signature is released, so the
     * caller can store it durably first. May be NULL. Non-zero aborts. */
    int (*persist)(void *ctx, uint32_t next_q);
    void *persist_ctx;
    /* cur[l]: the subtree of layer l (0 = bottom) that holds leaf q.
     * nxt[l]: the subtree after it, built one leaf per signature. */
    uint8_t cur[LMS_MAX_LAYERS][LMS_SUB_NODES][LMS_MAX_N];
    uint8_t nxt[LMS_MAX_LAYERS - 1][LMS_SUB_NODES][LMS_MAX_N];
    struct {
        uint8_t node[LMS_SUB_H * (LMS_MAX_LAYERS - 2) + 1][LMS_MAX_N];
        uint8_t height[LMS_SUB_H * (LMS_MAX_LAYERS - 2) + 1];
        uint8_t top;
    } th[LMS_MAX_LAYERS - 1];
} lms_key_t;

/* Build the caches for a key positioned at one-time key q (0 for a new key).
 * Costs one pass over all 2^h leaves. Returns 0 on success. */
int lms_key_load(lms_key_t *k, uint32_t lms_type, uint32_t ots_type,
                 const uint8_t *seed, const uint8_t I[16], uint32_t q);
void lms_public_key(const lms_key_t *k, uint8_t *pk);
uint32_t lms_remaining(const lms_key_t *k);
/* Sign and advance q. Returns 0, or -1 when the key is used up. */
int lms_sign(lms_key_t *k, const uint8_t *msg, size_t msglen, uint8_t *sig, size_t *siglen);
/* Same with the message randomizer C given by the caller (known-answer tests). */
int lms_sign_c(lms_key_t *k, const uint8_t *C, const uint8_t *msg, size_t msglen,
               uint8_t *sig, size_t *siglen);
/* One leaf of the tree (one full one-time public key); exported for timing. */
void lms_leaf(const lms_key_t *k, uint32_t q, uint8_t *out);

/* ---- HSS: a chain of LMS trees, each signing the public key of the next ---- */
typedef struct {
    int levels;
    uint32_t lms_type[HSS_MAX_LEVELS], ots_type[HSS_MAX_LEVELS];
    lms_key_t *lv[HSS_MAX_LEVELS];            /* storage supplied by the caller */
    uint8_t *spk[HSS_MAX_LEVELS - 1];         /* sig[i] || pub[i+1], caller storage */
    size_t spk_len[HSS_MAX_LEVELS - 1];
} hss_key_t;

/* The caller fills levels, the type arrays, lv[] and spk[] (each spk[i] large
 * enough for an LMS signature of level i plus a public key of level i+1).
 * Lower trees get their seeds from the tree above, like in Cisco's hash-sigs. */
int hss_keygen(hss_key_t *h, const uint8_t *seed, const uint8_t I[16]);
void hss_public_key(const hss_key_t *h, uint8_t *pk);
size_t hss_pk_bytes(const hss_key_t *h);
size_t hss_sig_bytes(const hss_key_t *h);
int hss_sign(hss_key_t *h, const uint8_t *msg, size_t msglen, uint8_t *sig, size_t *siglen);

#endif /* LMS_H */
