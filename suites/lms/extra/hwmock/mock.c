#include "pqc_prims.h"
#include "sha256_hw.h"

uint32_t mock_sha_h[8], mock_sha_m[16];
unsigned long mock_sha_starts;

static uint32_t bswap32(uint32_t x)
{
    return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24);
}

void mock_sha_reg_write(int reg)
{
    static const uint32_t iv[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    uint32_t st[8];

    for (int i = 0; i < 8; i++) {
        st[i] = (reg == SHA_START_REG) ? iv[i] : bswap32(mock_sha_h[i]);
    }
    pqc_sha256_compress_ref(st, (const uint8_t *)mock_sha_m, 1);
    for (int i = 0; i < 8; i++) {
        mock_sha_h[i] = bswap32(st[i]);
    }
    mock_sha_starts++;
}
