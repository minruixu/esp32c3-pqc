/* FIPS 202 for the optimized build of mlkem-native, through its
 * MLK_CONFIG_FIPS202_CUSTOM_HEADER interface: the sponge of c3_sponge.c on
 * top of the Keccak-f[1600] of components/prims. Used together with
 * MLK_CONFIG_SERIAL_FIPS202_ONLY: a single-issue core gains nothing from
 * four-way batching, which also runs unused permutations whenever a batch is
 * not full. */
#ifndef PQC_C3_FIPS202_MLKEM_H
#define PQC_C3_FIPS202_MLKEM_H

#include "c3_sponge.h"

#define SHAKE128_RATE C3_SHAKE128_RATE
#define SHAKE256_RATE C3_SHAKE256_RATE
#define SHA3_256_RATE C3_SHA3_256_RATE
#define SHA3_512_RATE C3_SHA3_512_RATE
#define SHA3_256_HASHBYTES 32
#define SHA3_512_HASHBYTES 64

typedef struct {
    c3_sponge sp;
} mlk_shake128ctx;

static inline void mlk_shake128_init(mlk_shake128ctx *state)
{
    c3_sponge_init(&state->sp);
}

static inline void mlk_shake128_absorb_once(mlk_shake128ctx *state, const uint8_t *input, size_t inlen)
{
    c3_sponge_absorb(&state->sp, C3_SHAKE128_RATE, input, inlen);
    c3_sponge_finalize(&state->sp, C3_SHAKE128_RATE, C3_PAD_SHAKE);
}

static inline void mlk_shake128_squeezeblocks(uint8_t *output, size_t nblocks, mlk_shake128ctx *state)
{
    c3_sponge_squeeze(&state->sp, C3_SHAKE128_RATE, output, nblocks * C3_SHAKE128_RATE);
}

static inline void mlk_shake128_release(mlk_shake128ctx *state)
{
    c3_sponge_wipe(&state->sp);
}

static inline void mlk_shake256(uint8_t *output, size_t outlen, const uint8_t *input, size_t inlen)
{
    c3_sponge_hash(C3_SHAKE256_RATE, C3_PAD_SHAKE, output, outlen, input, inlen);
}

static inline void mlk_sha3_256(uint8_t *output, const uint8_t *input, size_t inlen)
{
    c3_sponge_hash(C3_SHA3_256_RATE, C3_PAD_SHA3, output, 32, input, inlen);
}

static inline void mlk_sha3_512(uint8_t *output, const uint8_t *input, size_t inlen)
{
    c3_sponge_hash(C3_SHA3_512_RATE, C3_PAD_SHA3, output, 64, input, inlen);
}

#endif /* PQC_C3_FIPS202_MLKEM_H */
