/* HQC public-key encryption layer for the ESP32-C3. Included by
 * hqc_unit_opt.inc in place of the reference ref/gf2x.c and ref/hqc.c; it
 * defines the same three functions, hqc_pke_keygen/encrypt/decrypt, and
 * produces the same bytes.
 *
 * What is different from the reference code
 * -----------------------------------------
 * Every product in HQC has one operand of small, fixed, public weight: the
 * secret y (weight omega) or the ephemeral r2 (weight omega_r). The
 * reference code expands that operand to a dense bit vector and multiplies
 * two dense polynomials with Karatsuba over 64-bit words, ending in a
 * bit-serial schoolbook. Here the sparse operand stays a list of positions,
 * and the product is the sum of the dense operand shifted to each position:
 * weight * ceil(n/32) word operations instead of a full dense product, and a
 * work area of 2n bits instead of 18n bits.
 *
 * The sparse error vectors x, e, r1 are likewise never expanded: their bits
 * are flipped directly in the result.
 *
 * Side channels (read this before reusing the code elsewhere)
 * ----------------------------------------------------------
 * The reference multiplication runs the same instructions on the same
 * addresses whatever the secret is. This code runs the same *number and
 * kind* of instructions whatever the secret is (no secret-dependent branch,
 * loop count or early exit), but the secret positions decide which memory
 * words are touched and by how much words are shifted. On the ESP32-C3 that
 * does not change the running time: internal RAM is not cached, and shifts
 * take one cycle for any shift count; suite.c measures this on the device
 * (job "ct-mul"). On a processor with a data cache, or against an attacker
 * who can measure power or electromagnetic emissions, the access pattern is
 * a leak that the reference code does not have. */
#include <stdint.h>
#include <string.h>
#include "code.h"
#include "crypto_memset.h"
#include "hqc.h"
#include "parameters.h"
#include "symmetric.h"
#include "vector.h"

/* 32-bit view of the 64-bit word arrays used by the reference code. On a
 * little-endian machine bit i of a polynomial is bit (i & 31) of word i >> 5. */
typedef uint32_t __attribute__((may_alias)) c3_u32;

#define C3_W32 ((PARAM_N + 31) / 32)      /* 32-bit words of one polynomial */
#define C3_ACC_WORDS64 (2 * VEC_N_SIZE_64) /* work area of a product, 64-bit words */

#if (PARAM_N % 32) == 0
#error "the reduction below assumes that n is not a multiple of 32"
#endif

/* acc ^= b * X^p, without reduction. b has nwords words; acc is written at
 * words (p >> 5) .. (p >> 5) + nwords. On the ESP32-C3 this is the assembly
 * routine in hqc_c3_mul.S; the C version below is the same computation. */
#if defined(__riscv) && (__riscv_xlen == 32) && !defined(HQC_C3_NO_ASM)
void hqc_c3_shift_xor_asm(c3_u32 *acc, const c3_u32 *b, uint32_t p, uint32_t nwords);
#define c3_shift_xor hqc_c3_shift_xor_asm
#else
static void c3_shift_xor(c3_u32 *acc, const c3_u32 *b, uint32_t p, uint32_t nwords)
{
    c3_u32 *d = acc + (p >> 5);
    uint32_t s = p & 31;
    uint32_t carry = 0;

    for (uint32_t i = 0; i < nwords; i++) {
        uint32_t t = b[i];
        d[i] ^= (t << s) | carry;
        carry = (t >> 1) >> (31 - s); /* t >> (32 - s), and 0 when s is 0 */
    }
    d[nwords] ^= carry;
}
#endif

/* o = (sum over the support of X^position) * b mod (X^n - 1).
 * acc: work area of C3_ACC_WORDS64 64-bit words; the result is left in its
 * first VEC_N_SIZE_64 words, the rest is cleared. */
static void c3_sparse_mul(uint64_t *acc64, const uint32_t *support, uint32_t weight, const uint64_t *b64)
{
    c3_u32 *acc = (c3_u32 *)acc64;
    const c3_u32 *b = (const c3_u32 *)b64;
    const uint32_t r = PARAM_N & 31;

    memset(acc64, 0, C3_ACC_WORDS64 * sizeof(uint64_t));
    for (uint32_t j = 0; j < weight; j++) {
        c3_shift_xor(acc, b, support[j], C3_W32);
    }
    /* Fold bits n .. 2n-1 onto bits 0 .. n-1. */
    for (uint32_t i = 0; i < C3_W32; i++) {
        acc[i] ^= (acc[i + C3_W32 - 1] >> r) ^ (acc[i + C3_W32] << (32 - r));
    }
    acc[C3_W32 - 1] &= (1u << r) - 1;
    memset(acc + C3_W32, 0, (2 * C3_ACC_WORDS64 - C3_W32) * sizeof(uint32_t));
}

/* The multiplication on its own, for the timing test in suite.c. */
void hqc_c3_probe_mul(uint64_t *acc64, const uint32_t *support, uint32_t weight, const uint64_t *b64)
{
    c3_sparse_mul(acc64, support, weight, b64);
}

/* v ^= sum over the support of X^position. */
static void c3_sparse_add(uint64_t *v64, const uint32_t *support, uint32_t weight)
{
    c3_u32 *v = (c3_u32 *)v64;

    for (uint32_t j = 0; j < weight; j++) {
        v[support[j] >> 5] ^= 1u << (support[j] & 31);
    }
}

void hqc_pke_keygen(uint8_t *ek_pke, uint8_t *dk_pke, uint8_t *seed)
{
    uint8_t keypair_seed[2 * SEED_BYTES] = {0};
    uint8_t *seed_dk = keypair_seed;
    uint8_t *seed_ek = keypair_seed + SEED_BYTES;
    shake256_xof_ctx xof = {0};
    uint32_t y[PARAM_OMEGA], x[PARAM_OMEGA];
    uint64_t h[VEC_N_SIZE_64];
    uint64_t acc[C3_ACC_WORDS64];

    hash_i(keypair_seed, seed);

    xof_init(&xof, seed_dk, SEED_BYTES);
    vect_generate_random_support1(&xof, y, PARAM_OMEGA);
    vect_generate_random_support1(&xof, x, PARAM_OMEGA);

    xof_init(&xof, seed_ek, SEED_BYTES);
    h[VEC_N_SIZE_64 - 1] = 0;
    vect_set_random(&xof, h);

    /* s = x + h.y */
    c3_sparse_mul(acc, y, PARAM_OMEGA, h);
    c3_sparse_add(acc, x, PARAM_OMEGA);

    memcpy(ek_pke, seed_ek, SEED_BYTES);
    memcpy(ek_pke + SEED_BYTES, acc, VEC_N_SIZE_BYTES);
    memcpy(dk_pke, seed_dk, SEED_BYTES);

    memset_zero(keypair_seed, sizeof keypair_seed);
    memset_zero(x, sizeof x);
    memset_zero(y, sizeof y);
    memset_zero(&xof, sizeof xof);
}

void hqc_pke_encrypt(uint64_t *u, uint64_t *v, const uint8_t *ek_pke, const uint64_t *m, const uint8_t *theta)
{
    shake256_xof_ctx xof = {0};
    uint32_t r2[PARAM_OMEGA_R], e[PARAM_OMEGA_E], r1[PARAM_OMEGA_R];
    uint64_t hs[VEC_N_SIZE_64];
    uint64_t acc[C3_ACC_WORDS64];

    xof_init(&xof, theta, SEED_BYTES);
    vect_generate_random_support2(&xof, r2, PARAM_OMEGA_R);
    vect_generate_random_support2(&xof, e, PARAM_OMEGA_E);
    vect_generate_random_support2(&xof, r1, PARAM_OMEGA_R);

    /* u = r1 + r2.h, with h expanded from the seed in the public key */
    xof_init(&xof, ek_pke, SEED_BYTES);
    hs[VEC_N_SIZE_64 - 1] = 0;
    vect_set_random(&xof, hs);
    c3_sparse_mul(acc, r2, PARAM_OMEGA_R, hs);
    c3_sparse_add(acc, r1, PARAM_OMEGA_R);
    memcpy(u, acc, VEC_N_SIZE_64 * sizeof(uint64_t));

    /* v = C.encode(m) + Truncate(s.r2 + e) */
    hs[VEC_N_SIZE_64 - 1] = 0;
    memcpy(hs, ek_pke + SEED_BYTES, VEC_N_SIZE_BYTES);
    c3_sparse_mul(acc, r2, PARAM_OMEGA_R, hs);
    c3_sparse_add(acc, e, PARAM_OMEGA_E);
    vect_truncate(acc);
    code_encode(v, m);
    vect_add(v, v, acc, VEC_N1N2_SIZE_64);

    memset_zero(r1, sizeof r1);
    memset_zero(r2, sizeof r2);
    memset_zero(e, sizeof e);
    memset_zero(acc, sizeof acc);
    memset_zero(&xof, sizeof xof);
}

uint8_t hqc_pke_decrypt(uint64_t *m, const uint8_t *dk_pke, const uint64_t *u, const uint64_t *v)
{
    shake256_xof_ctx xof = {0};
    uint32_t y[PARAM_OMEGA];
    uint64_t acc[C3_ACC_WORDS64];

    xof_init(&xof, dk_pke, SEED_BYTES);
    vect_generate_random_support1(&xof, y, PARAM_OMEGA);

    /* m = C.decode(v - Truncate(u.y)) */
    c3_sparse_mul(acc, y, PARAM_OMEGA, u);
    vect_truncate(acc);
    vect_add(acc, v, acc, VEC_N1N2_SIZE_64);
    code_decode(m, acc);

    memset_zero(y, sizeof y);
    memset_zero(acc, sizeof acc);
    memset_zero(&xof, sizeof xof);
    return 0;
}
