/* Shared primitives with switchable backends.
 *
 * Every algorithm in this project reaches Keccak-f[1600], the SHA-256
 * compression function and the SM3 compression function through the three
 * entry points below. pqc_prims_select() switches all of them between the
 * portable reference code (REF) and the ESP32-C3 optimized code (OPT), so
 * one image can time both and check that they produce the same bytes.
 *
 * An optimized backend is added by defining pqc_<name>_opt() in a new source
 * file; until then the weak default in prims.c forwards to the reference. */
#ifndef PQC_PRIMS_H
#define PQC_PRIMS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PQC_PRIMS_REF 0
#define PQC_PRIMS_OPT 1

/* Select the backend of all three primitives, or of each one separately. */
void pqc_prims_select(int which);
void pqc_prims_select_ex(int keccak, int sha256, int sm3);
int pqc_prims_selected(void); /* value of the last pqc_prims_select() */

/* Backends. state[] of the hash functions holds the chaining value as
 * native-endian 32-bit words; blocks are 64 bytes each. */
void pqc_keccakf1600_ref(uint64_t *state);
void pqc_keccakf1600_opt(uint64_t *state);
void pqc_sha256_compress_ref(uint32_t state[8], const uint8_t *blocks, size_t nblocks);
void pqc_sha256_compress_opt(uint32_t state[8], const uint8_t *blocks, size_t nblocks);
void pqc_sm3_compress_ref(uint32_t state[8], const uint8_t *blocks, size_t nblocks);
void pqc_sm3_compress_opt(uint32_t state[8], const uint8_t *blocks, size_t nblocks);

extern void (*pqc_keccakf1600_fn)(uint64_t *state);
extern void (*pqc_sha256_compress_fn)(uint32_t state[8], const uint8_t *blocks, size_t nblocks);
extern void (*pqc_sm3_compress_fn)(uint32_t state[8], const uint8_t *blocks, size_t nblocks);

/* Entry points used by the algorithms. */
static inline void pqc_keccakf1600(uint64_t *state) { pqc_keccakf1600_fn(state); }
static inline void pqc_sha256_compress(uint32_t state[8], const uint8_t *blocks, size_t nblocks)
{
    pqc_sha256_compress_fn(state, blocks, nblocks);
}
static inline void pqc_sm3_compress(uint32_t state[8], const uint8_t *blocks, size_t nblocks)
{
    pqc_sm3_compress_fn(state, blocks, nblocks);
}

#ifdef __cplusplus
}
#endif
#endif /* PQC_PRIMS_H */
