/* SM3 hash (GB/T 32905-2016). The compression function is
 * pqc_sm3_compress() from pqc_prims.h, so it follows the backend switch. */
#ifndef PQC_SM3_H
#define PQC_SM3_H

#include <stddef.h>
#include <stdint.h>

#define PQC_SM3_DIGEST_BYTES 32
#define PQC_SM3_BLOCK_BYTES 64

typedef struct {
    uint32_t state[8];
    uint64_t nbytes;     /* bytes absorbed so far */
    uint8_t buf[64];     /* partial block */
} pqc_sm3_ctx;

void pqc_sm3_init(pqc_sm3_ctx *ctx);
void pqc_sm3_update(pqc_sm3_ctx *ctx, const uint8_t *in, size_t inlen);
void pqc_sm3_final(pqc_sm3_ctx *ctx, uint8_t out[32]);
void pqc_sm3(uint8_t out[32], const uint8_t *in, size_t inlen);

#endif /* PQC_SM3_H */
