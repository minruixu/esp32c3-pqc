#include "pqc_prims.h"

/* Weak defaults: an optimized backend that has not been written yet is the
 * reference one. A strong definition elsewhere replaces these. */
__attribute__((weak)) void pqc_keccakf1600_opt(uint64_t *state)
{
    pqc_keccakf1600_ref(state);
}
__attribute__((weak)) void pqc_sha256_compress_opt(uint32_t state[8], const uint8_t *blocks, size_t nblocks)
{
    pqc_sha256_compress_ref(state, blocks, nblocks);
}
__attribute__((weak)) void pqc_sm3_compress_opt(uint32_t state[8], const uint8_t *blocks, size_t nblocks)
{
    pqc_sm3_compress_ref(state, blocks, nblocks);
}

void (*pqc_keccakf1600_fn)(uint64_t *) = pqc_keccakf1600_ref;
void (*pqc_sha256_compress_fn)(uint32_t *, const uint8_t *, size_t) = pqc_sha256_compress_ref;
void (*pqc_sm3_compress_fn)(uint32_t *, const uint8_t *, size_t) = pqc_sm3_compress_ref;

static int selected = PQC_PRIMS_REF;

void pqc_prims_select_ex(int keccak, int sha256, int sm3)
{
    pqc_keccakf1600_fn = keccak ? pqc_keccakf1600_opt : pqc_keccakf1600_ref;
    pqc_sha256_compress_fn = sha256 ? pqc_sha256_compress_opt : pqc_sha256_compress_ref;
    pqc_sm3_compress_fn = sm3 ? pqc_sm3_compress_opt : pqc_sm3_compress_ref;
}

void pqc_prims_select(int which)
{
    selected = which ? PQC_PRIMS_OPT : PQC_PRIMS_REF;
    pqc_prims_select_ex(selected, selected, selected);
}

int pqc_prims_selected(void) { return selected; }
