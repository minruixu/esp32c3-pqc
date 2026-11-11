/* SLH-DSA with SHAKE: the tweakable hash functions F, PRF and H, and the
 * WOTS+ chain, written directly on the Keccak state.
 *
 * Each of them is SHAKE256(PK.seed || ADRS || data) with less than one rate
 * block of input, so it is exactly one Keccak-f[1600] call, and PK.seed,
 * ADRS and the data all start on 64-bit lane boundaries. slhdsa-c goes
 * through its incremental SHA-3 interface for F, PRF and H (clear the state,
 * absorb byte by byte, pad, squeeze byte by byte). Here the 25 lanes are
 * simply written and the first lanes read back. With a fast permutation
 * that bookkeeping is worth a few percent.
 *
 * The permutation is slhdsa-c's keccak_f1600(), that is, the project's
 * switchable one. Nothing here is specific to the ESP32-C3.
 */
#include <string.h>
#include "sha3_api.h"
#include "slh_adrs.h"
#include "slh_dsa.h"
#include "slh_var.h"
#include "slh_c3.h"

#define INLINE static inline __attribute__((always_inline))
#define UNROLL _Pragma("GCC unroll 25")

slh_param_t slh_fast_shake_128s, slh_fast_shake_128f;
slh_param_t slh_fast_shake_192s, slh_fast_shake_192f;
slh_param_t slh_fast_shake_256s, slh_fast_shake_256f;

/* Lanes after the data: the SHAKE padding byte, zeros, the end of the rate. */
INLINE void finish(uint64_t *st, unsigned used)
{
    const uint64_t last = UINT64_C(1) << 63;

    if (used == 16) {
        st[16] = last | 0x1F;
    } else {
        st[used] = 0x1F;
        UNROLL
        for (unsigned l = used + 1; l < 16; l++) {
            st[l] = 0;
        }
        st[16] = last;
    }
    UNROLL
    for (unsigned l = 17; l < 25; l++) {
        st[l] = 0;
    }
}

/* out = first nl lanes of Keccak(PK.seed || ADRS || d1 [|| d2]) */
INLINE void hash1(const slh_var_t *var, uint8_t *out, const uint8_t *d1, const uint8_t *d2,
                  const unsigned nl)
{
    uint64_t st[25];
    unsigned used = nl + 4 + nl;

    memcpy(st, var->pk_seed, 8 * nl);
    memcpy(st + nl, var->adrs->u8, 32);
    memcpy(st + nl + 4, d1, 8 * nl);
    if (d2 != NULL) {
        memcpy(st + 2 * nl + 4, d2, 8 * nl);
        used += nl;
    }
    finish(st, used);
    keccak_f1600(st);
    memcpy(out, st, 8 * nl);
}

/* Algorithm 5, chain: s steps from hash address i. */
INLINE void chain_n(slh_var_t *var, uint8_t *tmp, const uint8_t *x, uint32_t i, uint32_t s,
                    const unsigned nl)
{
    uint64_t st[25], seed[4], adrs[4];

    if (s == 0) {
        memmove(tmp, x, 8 * nl);
        return;
    }
    memcpy(seed, var->pk_seed, 8 * nl);
    memcpy(st, x, 8 * nl);
    for (uint32_t j = 0; j < s; j++) {
        adrs_set_hash_address(var, i + j);
        memcpy(adrs, var->adrs->u8, 32);
        UNROLL
        for (unsigned l = 0; l < nl; l++) {
            st[nl + 4 + l] = st[l]; /* the previous result is this step's data */
            st[l] = seed[l];
        }
        st[nl] = adrs[0];
        st[nl + 1] = adrs[1];
        st[nl + 2] = adrs[2];
        st[nl + 3] = adrs[3];
        finish(st, 2 * nl + 4);
        keccak_f1600(st);
    }
    memcpy(tmp, st, 8 * nl);
}

#define DEFINE_SET(NL)                                                                  \
    static void chain##NL(slh_var_t *var, uint8_t *tmp, const uint8_t *x, uint32_t i,   \
                          uint32_t s)                                                   \
    {                                                                                   \
        chain_n(var, tmp, x, i, s, NL);                                                 \
    }                                                                                   \
    static void f##NL(slh_var_t *var, uint8_t *h, const uint8_t *m1)                    \
    {                                                                                   \
        hash1(var, h, m1, NULL, NL);                                                    \
    }                                                                                   \
    static void prf##NL(slh_var_t *var, uint8_t *h)                                     \
    {                                                                                   \
        hash1(var, h, var->sk_seed, NULL, NL);                                          \
    }                                                                                   \
    static void h##NL(slh_var_t *var, uint8_t *h, const uint8_t *m1, const uint8_t *m2) \
    {                                                                                   \
        hash1(var, h, m1, m2, NL);                                                      \
    }                                                                                   \
    static void wots_chain##NL(slh_var_t *var, uint8_t *tmp, uint32_t s)                \
    {                                                                                   \
        adrs_set_type(var, ADRS_WOTS_PRF);                                              \
        adrs_set_tree_index(var, 0);                                                    \
        hash1(var, tmp, var->sk_seed, NULL, NL);                                        \
        adrs_set_type(var, ADRS_WOTS_HASH);                                             \
        chain_n(var, tmp, tmp, 0, s, NL);                                               \
    }                                                                                   \
    static void fors_hash##NL(slh_var_t *var, uint8_t *tmp, uint32_t s)                 \
    {                                                                                   \
        adrs_set_type(var, ADRS_FORS_PRF);                                              \
        adrs_set_tree_height(var, 0);                                                   \
        hash1(var, tmp, var->sk_seed, NULL, NL);                                        \
        if (s == 1) {                                                                   \
            adrs_set_type(var, ADRS_FORS_TREE);                                         \
            hash1(var, tmp, tmp, NULL, NL);                                             \
        }                                                                               \
    }                                                                                   \
    static void make##NL(slh_param_t *dst, const slh_param_t *up)                       \
    {                                                                                   \
        *dst = *up;                                                                     \
        dst->chain = chain##NL;                                                         \
        dst->wots_chain = wots_chain##NL;                                               \
        dst->fors_hash = fors_hash##NL;                                                 \
        dst->prf = prf##NL;                                                             \
        dst->h_f = f##NL;                                                               \
        dst->h_h = h##NL;                                                               \
    }

DEFINE_SET(2)
DEFINE_SET(3)
DEFINE_SET(4)

void slh_fast_shake_init(void)
{
    make2(&slh_fast_shake_128s, &slh_dsa_shake_128s);
    make2(&slh_fast_shake_128f, &slh_dsa_shake_128f);
    make3(&slh_fast_shake_192s, &slh_dsa_shake_192s);
    make3(&slh_fast_shake_192f, &slh_dsa_shake_192f);
    make4(&slh_fast_shake_256s, &slh_dsa_shake_256s);
    make4(&slh_fast_shake_256f, &slh_dsa_shake_256f);
}
