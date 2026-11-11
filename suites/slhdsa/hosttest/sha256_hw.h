/* Stand-in for components/prims/opt/sha256_hw.h on the build machine: a
 * register-level model of the ESP32-C3 SHA accelerator, so that the code in
 * ../pqc_suite/slh_sha2_c3.c can be exercised without the board.
 *
 * It models what that code relies on and what suites/hashes checks on the
 * real chip: the chaining value and the message block are registers that
 * keep their contents, and a start compresses the block as it is at that
 * moment (later writes to the message registers do not affect it). */
#ifndef PQC_SHA256_HW_H
#define PQC_SHA256_HW_H

#include <stddef.h>
#include <stdint.h>

extern uint32_t mock_sha_h[8], mock_sha_m[16];
extern unsigned long mock_sha_starts, mock_sha_m_writes;
void mock_sha_compress(void);

#define PQC_SHA_H mock_sha_h
#define PQC_SHA_M mock_sha_m

static inline void pqc_sha256_hw_init(void) {}
static inline void pqc_sha256_hw_start(void) { mock_sha_compress(); }
static inline void pqc_sha256_hw_wait(void) {}
static inline void pqc_sha256_hw_run(void) { mock_sha_compress(); }
static inline void pqc_sha256_hw_run_nowait(void) { mock_sha_compress(); }

static inline void pqc_sha256_hw_load(const uint32_t *state)
{
    for (int i = 0; i < 8; i++) {
        mock_sha_h[i] = state[i];
    }
}

static inline void pqc_sha256_hw_block(const uint32_t *block)
{
    for (int i = 0; i < 16; i++) {
        mock_sha_m[i] = block[i];
    }
    mock_sha_compress();
}

static inline void pqc_sha256_hw_read(uint32_t *out, int nwords)
{
    for (int i = 0; i < nwords; i++) {
        out[i] = mock_sha_h[i];
    }
}

#endif /* PQC_SHA256_HW_H */
