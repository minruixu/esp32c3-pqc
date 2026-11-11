/* pqc_sha256_compress_opt(): SHA-256 compression on the ESP32-C3 SHA
 * accelerator.
 *
 * The compression itself takes the accelerator about 80 CPU cycles; what
 * costs is talking to it, 8 to 9 cycles per register access. This function
 * therefore:
 *  - loads the chaining value once per call and reads it back once, however
 *    many blocks the call has;
 *  - lets the accelerator load the initial value itself when the caller's
 *    chaining value is the SHA-256 initial value (the start of a message);
 *  - writes the next block while the previous one is being compressed (the
 *    accelerator takes its copy of the block when it starts);
 *  - converts the byte order of the chaining value inline. The message
 *    blocks need no conversion at all.
 *
 * Code that hashes many short messages with a fixed prefix can do better by
 * using sha256_hw.h directly: see suites/slhdsa.
 */
#include <string.h>
#include "esp_private/esp_crypto_lock_internal.h"
#include "hal/sha_ll.h"
#include "pqc_prims.h"
#include "sha256_hw.h"

static int hw_ready;

void pqc_sha256_hw_init(void)
{
    if (!hw_ready) {
        SHA_RCC_ATOMIC()
        {
            sha_ll_enable_bus_clock(true);
            sha_ll_reset_register();
        }
        hw_ready = 1;
    }
    REG_WRITE(SHA_MODE_REG, SHA_MODE_SHA256);
}

static inline __attribute__((always_inline)) uint32_t bswap32(uint32_t x)
{
    return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24);
}

static inline __attribute__((always_inline)) void write_block(const uint32_t *w)
{
    PQC_SHA_M[0] = w[0];
    PQC_SHA_M[1] = w[1];
    PQC_SHA_M[2] = w[2];
    PQC_SHA_M[3] = w[3];
    PQC_SHA_M[4] = w[4];
    PQC_SHA_M[5] = w[5];
    PQC_SHA_M[6] = w[6];
    PQC_SHA_M[7] = w[7];
    PQC_SHA_M[8] = w[8];
    PQC_SHA_M[9] = w[9];
    PQC_SHA_M[10] = w[10];
    PQC_SHA_M[11] = w[11];
    PQC_SHA_M[12] = w[12];
    PQC_SHA_M[13] = w[13];
    PQC_SHA_M[14] = w[14];
    PQC_SHA_M[15] = w[15];
}

void pqc_sha256_hw_short(uint8_t out[32], const uint8_t *msg, size_t len)
{
    uint32_t w[16], h[8];
    uint8_t *b = (uint8_t *)w;

    pqc_sha256_hw_init();
    memcpy(b, msg, len);
    b[len] = 0x80;
    memset(b + len + 1, 0, 59 - len);
    w[15] = bswap32((uint32_t)len * 8);
    write_block(w);
    REG_WRITE(SHA_START_REG, 1); /* starts from the SHA-256 initial value */
    pqc_sha256_hw_wait();
    pqc_sha256_hw_read(h, 8);
    memcpy(out, h, 32);
}

void pqc_sha256_compress_opt(uint32_t state[8], const uint8_t *blocks, size_t nblocks)
{
    static const uint32_t iv[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    uint32_t start_reg = SHA_CONTINUE_REG;
    uint32_t tmp[16];

    if (nblocks == 0) {
        return;
    }
    pqc_sha256_hw_init();
    if (state[0] == iv[0] && state[1] == iv[1] && state[2] == iv[2] && state[3] == iv[3] &&
        state[4] == iv[4] && state[5] == iv[5] && state[6] == iv[6] && state[7] == iv[7]) {
        start_reg = SHA_START_REG; /* the accelerator loads the initial value itself */
    } else {
        PQC_SHA_H[0] = bswap32(state[0]);
        PQC_SHA_H[1] = bswap32(state[1]);
        PQC_SHA_H[2] = bswap32(state[2]);
        PQC_SHA_H[3] = bswap32(state[3]);
        PQC_SHA_H[4] = bswap32(state[4]);
        PQC_SHA_H[5] = bswap32(state[5]);
        PQC_SHA_H[6] = bswap32(state[6]);
        PQC_SHA_H[7] = bswap32(state[7]);
    }
    do {
        const uint32_t *w;

        if (((uintptr_t)blocks & 3) == 0) {
            w = (const uint32_t *)(const void *)blocks;
        } else {
            memcpy(tmp, blocks, 64);
            w = tmp;
        }
        write_block(w); /* the previous block may still be in the works */
        pqc_sha256_hw_wait();
        REG_WRITE(start_reg, 1);
        start_reg = SHA_CONTINUE_REG;
        blocks += 64;
    } while (--nblocks);
    pqc_sha256_hw_wait();
    state[0] = bswap32(PQC_SHA_H[0]);
    state[1] = bswap32(PQC_SHA_H[1]);
    state[2] = bswap32(PQC_SHA_H[2]);
    state[3] = bswap32(PQC_SHA_H[3]);
    state[4] = bswap32(PQC_SHA_H[4]);
    state[5] = bswap32(PQC_SHA_H[5]);
    state[6] = bswap32(PQC_SHA_H[6]);
    state[7] = bswap32(PQC_SHA_H[7]);
}
