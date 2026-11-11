/* Stand-in for components/prims/opt/sha256_hw.h on the build machine: a
 * register-level model of the ESP32-C3 SHA accelerator, so that lms_hw.c
 * and ../../../xmss/pqc_suite/hash_hw.c can be exercised without the board.
 *
 * The model has the properties those files rely on (and that suites/hashes
 * checks on the real chip): the H and M registers keep their contents, a
 * start compresses the block as it is at that moment, and SHA_START loads
 * the SHA-256 initial value first. */
#ifndef PQC_SHA256_HW_H
#define PQC_SHA256_HW_H

#include <stddef.h>
#include <stdint.h>

extern uint32_t mock_sha_h[8], mock_sha_m[16];
extern unsigned long mock_sha_starts;
void mock_sha_reg_write(int reg);

#define PQC_SHA_H mock_sha_h
#define PQC_SHA_M mock_sha_m
#define SHA_START_REG 1
#define SHA_CONTINUE_REG 2
#define REG_WRITE(reg, value) mock_sha_reg_write(reg)

static inline void pqc_sha256_hw_init(void) {}
static inline void pqc_sha256_hw_wait(void) {}
static inline void pqc_sha256_hw_start(void) { mock_sha_reg_write(SHA_CONTINUE_REG); }
static inline void pqc_sha256_hw_run(void) { mock_sha_reg_write(SHA_CONTINUE_REG); }

static inline void pqc_sha256_hw_load(const uint32_t *state)
{
    for (int i = 0; i < 8; i++) {
        mock_sha_h[i] = state[i];
    }
}

static inline void pqc_sha256_hw_read(uint32_t *out, int nwords)
{
    for (int i = 0; i < nwords; i++) {
        out[i] = mock_sha_h[i];
    }
}

#endif /* PQC_SHA256_HW_H */
