/* Puts the project's backend switch in front of slhdsa-c's two primitives.
 *
 * With the reference backend selected, slhdsa-c's own portable C runs, so
 * the "ref" variant is exactly the upstream code. With the optimized backend
 * selected, SHA-256 compression goes to the ESP32-C3 accelerator and
 * Keccak-f[1600] to the project's optimized permutation. */
#include <stdint.h>
#include "pqc_prims.h"

void sha2_256_compress_c(void *v);
void sha2_512_compress_c(void *v);
void keccak_f1600_c(uint64_t x[25]);

#ifndef PQC_HOST
#include "sha256_hw.h"
#include "slh_c3.h"
void pqc_sha512_compress_rv32(uint32_t state[16], const uint32_t block[32]);
#endif

static inline uint32_t bswap32(uint32_t x)
{
    return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24);
}

/* v points to slhdsa-c's layout: 8 words holding the chaining value as its
 * big-endian byte string, followed by the 64-byte block. */
void sha2_256_compress(void *v)
{
    uint32_t *sp = (uint32_t *)v;

    if (pqc_sha256_compress_fn == pqc_sha256_compress_ref) {
        sha2_256_compress_c(v);
        return;
    }
#ifndef PQC_HOST
    if (pqc_sha256_compress_fn == pqc_sha256_compress_opt) {
        /* The accelerator uses the same byte layout: no conversion at all. */
        pqc_sha256_hw_init();
        pqc_sha256_hw_load(sp);
        pqc_sha256_hw_block(sp + 8);
        pqc_sha256_hw_read(sp, 8);
        slh_c3_m_dirty = 1;
        return;
    }
#endif
    {
        uint32_t st[8];
        for (int i = 0; i < 8; i++) {
            st[i] = bswap32(sp[i]);
        }
        pqc_sha256_compress(st, (const uint8_t *)(sp + 8), 1);
        for (int i = 0; i < 8; i++) {
            sp[i] = bswap32(st[i]);
        }
    }
}

/* SHA-512 has no switch in pqc_prims.h and no hardware on the ESP32-C3. It
 * follows the SHA-256 selection: slhdsa-c's portable C for the reference
 * variant, the assembly version for the optimized one. v: 64 bytes of
 * chaining value in big-endian, then the 128-byte block. */
void sha2_512_compress(void *v)
{
#ifndef PQC_HOST
    if (pqc_sha256_compress_fn != pqc_sha256_compress_ref) {
        uint32_t *p = (uint32_t *)v;
        uint32_t st[16], w[32];
        for (int i = 0; i < 16; i++) {
            st[i] = bswap32(p[i]);
        }
        for (int i = 0; i < 32; i++) {
            w[i] = bswap32(p[16 + i]);
        }
        pqc_sha512_compress_rv32(st, w);
        for (int i = 0; i < 16; i++) {
            p[i] = bswap32(st[i]);
        }
        return;
    }
#endif
    sha2_512_compress_c(v);
}

void keccak_f1600(uint64_t x[25])
{
    if (pqc_keccakf1600_fn == pqc_keccakf1600_ref) {
        keccak_f1600_c(x);
    } else {
        pqc_keccakf1600(x);
    }
}
