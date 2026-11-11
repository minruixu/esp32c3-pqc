/* SHA-256 on the ESP32-C3 SHA accelerator: register-level access.
 *
 * The accelerator keeps one chaining value in its H registers. Read or
 * written as 32-bit words on this little-endian CPU, those registers hold
 * the chaining value as its big-endian byte string, so a digest can be
 * copied out as is and a message block can be copied in as is. This header
 * calls that representation "be words".
 *
 * Nothing here takes a lock. A program that also uses the accelerator from
 * other tasks (for example through Mbed TLS) must serialize access itself.
 */
#ifndef PQC_SHA256_HW_H
#define PQC_SHA256_HW_H

#include <stddef.h>
#include <stdint.h>
#include "soc/hwcrypto_reg.h"
#include "soc/soc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PQC_SHA_H ((volatile uint32_t *)(SHA_H_BASE))
#define PQC_SHA_M ((volatile uint32_t *)(SHA_TEXT_BASE))

/* Enable the accelerator and select SHA-256. Cheap to call again. */
void pqc_sha256_hw_init(void);

/* SHA-256 of a message of at most 55 bytes (it fits one block with its
 * padding), without going through a hash context: about 2.4 microseconds
 * instead of 9.5 through sha256() with the optimized backend. This is the
 * shape of one step of a hash chain, for example in LMS. */
void pqc_sha256_hw_short(uint8_t out[32], const uint8_t *msg, size_t len);

/* Start compressing the block that is in the M registers into the H
 * registers; wait until the accelerator is idle. */
static inline __attribute__((always_inline)) void pqc_sha256_hw_start(void)
{
    REG_WRITE(SHA_CONTINUE_REG, 1);
}

static inline __attribute__((always_inline)) void pqc_sha256_hw_wait(void)
{
    while (REG_READ(SHA_BUSY_REG)) {
    }
}

/* Compress one block and wait for the result. */
static inline __attribute__((always_inline)) void pqc_sha256_hw_run(void)
{
    pqc_sha256_hw_start();
    pqc_sha256_hw_wait();
}

/* For several blocks in a row: wait for the previous block, start this one
 * and return at once. The accelerator copies the M registers when it starts,
 * so the next block can be written while it works (hashes/hw_probe.c checks
 * this on the device). Call pqc_sha256_hw_wait() before reading H. */
static inline __attribute__((always_inline)) void pqc_sha256_hw_run_nowait(void)
{
    pqc_sha256_hw_wait();
    pqc_sha256_hw_start();
}

/* state: 8 be words; block: 16 words (64 bytes), word-aligned. */
static inline __attribute__((always_inline)) void pqc_sha256_hw_load(const uint32_t *state)
{
#pragma GCC unroll 8
    for (int i = 0; i < 8; i++) {
        PQC_SHA_H[i] = state[i];
    }
}

static inline __attribute__((always_inline)) void pqc_sha256_hw_block(const uint32_t *block)
{
#pragma GCC unroll 16
    for (int i = 0; i < 16; i++) {
        PQC_SHA_M[i] = block[i];
    }
    pqc_sha256_hw_run();
}

static inline __attribute__((always_inline)) void pqc_sha256_hw_read(uint32_t *out, int nwords)
{
#pragma GCC unroll 8
    for (int i = 0; i < nwords; i++) {
        out[i] = PQC_SHA_H[i];
    }
}

#ifdef __cplusplus
}
#endif
#endif /* PQC_SHA256_HW_H */
