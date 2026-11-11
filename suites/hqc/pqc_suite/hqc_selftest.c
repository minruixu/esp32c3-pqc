/* Self-tests of the building blocks that the ESP32-C3 build of HQC replaces:
 * each one runs the optimized function and the reference function on the
 * same inputs and counts the differences. Used on the device, and on the
 * host when it is built with -D HQC_HOST_OPT=1. */
#include <stdint.h>
#include <string.h>
#include "bench.h"
#include "hqc_api.h"

uint16_t hqc1r_gf_mul(uint16_t a, uint16_t b);
uint16_t hqc1o_gf_mul(uint16_t a, uint16_t b);

/* All 65536 products in GF(2^8). */
int hqc_gf_selftest(void)
{
    uint32_t bad = 0;

    for (uint32_t a = 0; a < 256; a++) {
        for (uint32_t b = 0; b < 256; b++) {
            bad += (uint32_t)(hqc1r_gf_mul((uint16_t)a, (uint16_t)b) != hqc1o_gf_mul((uint16_t)a, (uint16_t)b));
        }
    }
    return bad != 0;
}

/* Reed-Muller decoding and Reed-Solomon decoding of random words, for the
 * three parameter sets. Random words are far from any codeword, so ties in
 * the peak search and uncorrectable patterns are well represented; the two
 * implementations must still agree. */
#define HQC_CODE_SELFTEST(L, N1, N2, KBYTES)                                               \
    void hqc##L##r_reed_muller_decode(uint64_t *msg, const uint64_t *cdw);                 \
    void hqc##L##o_reed_muller_decode(uint64_t *msg, const uint64_t *cdw);                 \
    void hqc##L##r_reed_solomon_decode(uint64_t *msg, uint64_t *cdw);                      \
    void hqc##L##o_reed_solomon_decode(uint64_t *msg, uint64_t *cdw);                      \
    void hqc##L##r_reed_solomon_encode(uint64_t *cdw, const uint64_t *msg);                \
    void hqc##L##o_reed_solomon_encode(uint64_t *cdw, const uint64_t *msg);                \
    static uint32_t code_selftest_##L(uint32_t rounds)                                     \
    {                                                                                      \
        enum { V64 = ((N1) * (N2) + 63) / 64, R64 = ((N1) + 7) / 8, M64 = ((KBYTES) + 7) / 8 }; \
        static uint64_t cdw[V64];                                                          \
        uint64_t ra[R64], rb[R64], rc[R64], ma[M64 + 1], mb[M64 + 1];                      \
        uint32_t bad = 0;                                                                  \
                                                                                           \
        for (uint32_t r = 0; r < rounds; r++) {                                            \
            /* Reed-Muller: random bits in, N1 bytes out. */                               \
            randombytes((uint8_t *)cdw, sizeof(cdw));                                      \
            memset(ra, 0, sizeof(ra));                                                     \
            memset(rb, 0, sizeof(rb));                                                     \
            hqc##L##r_reed_muller_decode(ra, cdw);                                         \
            hqc##L##o_reed_muller_decode(rb, cdw);                                         \
            bad += (uint32_t)(memcmp(ra, rb, (N1)) != 0);                                  \
            /* Reed-Solomon: encode a random message, add 0..N1 symbol errors. */         \
            memset(ma, 0, sizeof(ma));                                                     \
            randombytes((uint8_t *)ma, (KBYTES));                                          \
            memset(ra, 0, sizeof(ra));                                                     \
            memset(rb, 0, sizeof(rb));                                                     \
            hqc##L##r_reed_solomon_encode(ra, ma);                                         \
            hqc##L##o_reed_solomon_encode(rb, ma);                                         \
            bad += (uint32_t)(memcmp(ra, rb, (N1)) != 0);                                  \
            {                                                                              \
                uint8_t e[4];                                                              \
                uint32_t nerr;                                                             \
                randombytes(e, 1);                                                         \
                nerr = (r & 1) ? e[0] % ((N1) + 1) : e[0] % ((((N1) - (KBYTES)) / 2) + 2); \
                for (uint32_t k = 0; k < nerr; k++) {                                      \
                    randombytes(e, 2);                                                     \
                    ((uint8_t *)ra)[e[0] % (N1)] ^= e[1];                                  \
                }                                                                          \
            }                                                                              \
            memcpy(rb, ra, sizeof(ra));                                                    \
            memcpy(rc, ra, sizeof(ra));                                                    \
            memset(ma, 0, sizeof(ma));                                                     \
            memset(mb, 0, sizeof(mb));                                                     \
            hqc##L##r_reed_solomon_decode(ma, rb);                                         \
            hqc##L##o_reed_solomon_decode(mb, rc);                                         \
            bad += (uint32_t)(memcmp(ma, mb, (KBYTES)) != 0);                              \
        }                                                                                  \
        return bad;                                                                        \
    }

HQC_CODE_SELFTEST(1, 46, 384, 16)
HQC_CODE_SELFTEST(3, 56, 640, 24)
HQC_CODE_SELFTEST(5, 90, 640, 32)

int hqc_code_selftest(uint32_t rounds)
{
    bench_rng_seed(11);
    return (code_selftest_1(rounds) + code_selftest_3(rounds) + code_selftest_5(rounds)) != 0;
}
