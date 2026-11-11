/* Connects c-fn-dsa to this project: Keccak-f[1600] through pqc_prims.h,
 * random bytes from the harness. */
#include <stddef.h>
#include <stdint.h>
#include "bench.h"
#include "pqc_prims.h"

void fndsa_sha3_process_block(uint64_t *A, unsigned r)
{
    (void)r;
    pqc_keccakf1600(A);
}

int fndsa_sysrng(void *dst, size_t len)
{
    return randombytes((uint8_t *)dst, len) == 0;
}
