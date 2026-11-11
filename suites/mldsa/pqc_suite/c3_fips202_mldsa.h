/* FIPS 202 for the optimized build of mldsa-native, through its
 * MLD_CONFIG_FIPS202_CUSTOM_HEADER interface: the word-oriented sponge of
 * suites/mlkem/pqc_suite/c3_sponge.c on top of the Keccak-f[1600] of
 * components/prims. Used together with MLD_CONFIG_SERIAL_FIPS202_ONLY: a
 * single-issue core gains nothing from four-way batching. */
#ifndef PQC_C3_FIPS202_MLDSA_H
#define PQC_C3_FIPS202_MLDSA_H

#include "c3_sponge.h"

#define SHAKE128_RATE C3_SHAKE128_RATE
#define SHAKE256_RATE C3_SHAKE256_RATE
#define SHA3_256_RATE C3_SHA3_256_RATE
#define SHA3_512_RATE C3_SHA3_512_RATE
#define SHA3_256_HASHBYTES 32
#define SHA3_512_HASHBYTES 64

typedef struct {
    c3_sponge sp;
} mld_shake128ctx;

typedef struct {
    c3_sponge sp;
} mld_shake256ctx;

static inline void mld_shake128_init(mld_shake128ctx *state) { c3_sponge_init(&state->sp); }
static inline void mld_shake128_absorb(mld_shake128ctx *state, const uint8_t *in, size_t inlen)
{
    c3_sponge_absorb(&state->sp, C3_SHAKE128_RATE, in, inlen);
}
static inline void mld_shake128_finalize(mld_shake128ctx *state)
{
    c3_sponge_finalize(&state->sp, C3_SHAKE128_RATE, C3_PAD_SHAKE);
}
static inline void mld_shake128_squeeze(uint8_t *out, size_t outlen, mld_shake128ctx *state)
{
    c3_sponge_squeeze(&state->sp, C3_SHAKE128_RATE, out, outlen);
}
static inline void mld_shake128_release(mld_shake128ctx *state) { c3_sponge_wipe(&state->sp); }

static inline void mld_shake256_init(mld_shake256ctx *state) { c3_sponge_init(&state->sp); }
static inline void mld_shake256_absorb(mld_shake256ctx *state, const uint8_t *in, size_t inlen)
{
    c3_sponge_absorb(&state->sp, C3_SHAKE256_RATE, in, inlen);
}
static inline void mld_shake256_finalize(mld_shake256ctx *state)
{
    c3_sponge_finalize(&state->sp, C3_SHAKE256_RATE, C3_PAD_SHAKE);
}
static inline void mld_shake256_squeeze(uint8_t *out, size_t outlen, mld_shake256ctx *state)
{
    c3_sponge_squeeze(&state->sp, C3_SHAKE256_RATE, out, outlen);
}
static inline void mld_shake256_release(mld_shake256ctx *state) { c3_sponge_wipe(&state->sp); }

static inline void mld_shake256(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen)
{
    c3_sponge_hash(C3_SHAKE256_RATE, C3_PAD_SHAKE, out, outlen, in, inlen);
}

#endif /* PQC_C3_FIPS202_MLDSA_H */
