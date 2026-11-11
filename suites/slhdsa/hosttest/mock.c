#include "pqc_prims.h"
#include "sha256_hw.h"

uint32_t mock_sha_h[8], mock_sha_m[16];
unsigned long mock_sha_starts;

static uint32_t bswap32(uint32_t x)
{
    return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24);
}

void mock_sha_compress(void)
{
    uint32_t st[8];

    for (int i = 0; i < 8; i++) {
        st[i] = bswap32(mock_sha_h[i]);
    }
    pqc_sha256_compress_ref(st, (const uint8_t *)mock_sha_m, 1);
    for (int i = 0; i < 8; i++) {
        mock_sha_h[i] = bswap32(st[i]);
    }
    mock_sha_starts++;
}

/* The assembly SHA-512 compression is not available here: the same
 * interface on top of slhdsa-c's portable C. */
void sha2_512_compress_c(void *v);

void pqc_sha512_compress_rv32(uint32_t state[16], const uint32_t block[32])
{
    uint64_t ctx[8 + 16];
    uint32_t *p = (uint32_t *)ctx;

    for (int i = 0; i < 16; i++) {
        p[i] = bswap32(state[i]);
    }
    for (int i = 0; i < 32; i++) {
        p[16 + i] = bswap32(block[i]);
    }
    sha2_512_compress_c(ctx);
    for (int i = 0; i < 16; i++) {
        state[i] = bswap32(p[i]);
    }
}
