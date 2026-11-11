/* Device-only measurements inside the ESP32-C3 build of HQC:
 *
 *  - "ct-mul": the polynomial multiplication is timed with interrupts off for
 *    supports chosen to stress the secret-dependent parts (random positions,
 *    all shift counts 0, all shift counts 31, lowest and highest positions).
 *    The cycle counts must not differ: see the side-channel note at the top
 *    of hqc_c3_pke.c.
 *  - "profile": cycle counts of the building blocks, to show where the time
 *    of key generation, encapsulation and decapsulation goes.
 *
 * The functions measured here are internal to the per-level builds; their
 * prototypes are repeated below with the prefixes from hqc_rename.h. */
#include <stdint.h>
#include <string.h>
#include "bench.h"
#include "esp_cpu.h"
#include "fips202.h"
#include "freertos/FreeRTOS.h"
#include "hqc_api.h"

#define CT_TRIALS 24
#define CT_TOLERANCE_CYCLES 0

#define NOW() ((uint32_t)esp_cpu_get_cycle_count())

#define HQC_PROBE(P, NAME, N, N1, N2, KBYTES, OMEGA, OMEGA_R)                                       \
    void P##hqc_pke_keygen(uint8_t *ek, uint8_t *dk, uint8_t *seed);                                \
    void P##hqc_pke_encrypt(uint64_t *u, uint64_t *v, const uint8_t *ek, const uint64_t *m,         \
                            const uint8_t *theta);                                                  \
    uint8_t P##hqc_pke_decrypt(uint64_t *m, const uint8_t *dk, const uint64_t *u, const uint64_t *v); \
    void P##hash_h(uint8_t *out, const uint8_t *ek);                                                \
    void P##hash_j(uint8_t *out, const uint8_t *h_ek, const uint8_t *sigma, const uint64_t *u,      \
                   const uint64_t *v, const uint8_t *salt);                                         \
    void P##code_encode(uint64_t *em, const uint64_t *m);                                           \
    void P##reed_muller_decode(uint64_t *msg, const uint64_t *cdw);                                 \
    void P##reed_solomon_decode(uint64_t *msg, const uint64_t *cdw);                                \
    void P##xof_init(shake256incctx *ctx, const uint8_t *seed, uint32_t seed_size);                 \
    void P##vect_set_random(shake256incctx *ctx, uint64_t *v);                                      \
    void P##c3_probe_mul(uint64_t *acc, const uint32_t *support, uint32_t weight, const uint64_t *b); \
                                                                                                    \
    int NAME##_probe(void)                                                                          \
    {                                                                                               \
        enum { W64 = ((N) + 63) / 64, V64 = ((N1) * (N2) + 63) / 64, R64 = ((N1) + 7) / 8 };        \
        uint64_t b[W64], u[W64], v[V64], acc[2 * W64], rs[R64], m[4] = {0}, m2[4] = {0};            \
        uint8_t ek[32 + ((N) + 7) / 8], seed[32], theta[32], dk[32], hh[32], salt[16] = {0};        \
        uint32_t sup[OMEGA_R];                                                                      \
        shake256incctx xof;                                                                         \
        uint32_t cmin = UINT32_MAX, cmax = 0, t0;                                                   \
        uint32_t c_xof, c_hash_h, c_keygen, c_encrypt, c_decrypt, c_encode, c_rm, c_rs, c_hash_j;   \
                                                                                                    \
        bench_rng_seed(7);                                                                          \
        randombytes((uint8_t *)b, sizeof(b));                                                       \
        for (int t = 0; t < CT_TRIALS; t++) {                                                       \
            unsigned long st;                                                                       \
            uint32_t c;                                                                             \
            for (uint32_t j = 0; j < (OMEGA_R); j++) {                                              \
                uint32_t r;                                                                         \
                randombytes((uint8_t *)&r, sizeof(r));                                              \
                switch (t) {                                                                        \
                case 0: sup[j] = 32 * j; break;                 /* every shift count 0 */         \
                case 1: sup[j] = 32 * j + 31; break;            /* every shift count 31 */        \
                case 2: sup[j] = j; break;                      /* lowest positions */             \
                case 3: sup[j] = (N) - 1 - j; break;            /* highest positions */            \
                case 4: sup[j] = 0; break;                      /* one position, repeated */       \
                case 5: sup[j] = (N) - 1; break;                                                    \
                default: sup[j] = r % (N); break;               /* random */                       \
                }                                                                                   \
            }                                                                                       \
            st = portSET_INTERRUPT_MASK_FROM_ISR();                                                 \
            t0 = NOW();                                                                             \
            P##c3_probe_mul(acc, sup, (OMEGA_R), b);                                                \
            c = NOW() - t0;                                                                         \
            portCLEAR_INTERRUPT_MASK_FROM_ISR(st);                                                  \
            if (c < cmin) cmin = c;                                                                 \
            if (c > cmax) cmax = c;                                                                 \
        }                                                                                           \
        bench_info(#NAME "/opt/ct-mul", "weight=%u trials=%u cycles_min=%lu cycles_max=%lu",        \
                   (unsigned)(OMEGA_R), (unsigned)CT_TRIALS, (unsigned long)cmin,                   \
                   (unsigned long)cmax);                                                            \
                                                                                                    \
        randombytes(seed, 32);                                                                      \
        randombytes(theta, 32);                                                                     \
        randombytes((uint8_t *)m, (KBYTES));                                                        \
        t0 = NOW(); P##hqc_pke_keygen(ek, dk, seed); c_keygen = NOW() - t0;                         \
        t0 = NOW(); P##xof_init(&xof, ek, 32); P##vect_set_random(&xof, b); c_xof = NOW() - t0;     \
        t0 = NOW(); P##hash_h(hh, ek); c_hash_h = NOW() - t0;                                       \
        memset(v, 0, sizeof(v));                                                                    \
        t0 = NOW(); P##hqc_pke_encrypt(u, v, ek, m, theta); c_encrypt = NOW() - t0;                 \
        t0 = NOW(); P##hqc_pke_decrypt(m2, dk, u, v); c_decrypt = NOW() - t0;                       \
        if (memcmp(m, m2, (KBYTES)) != 0) return 2;                                                 \
        t0 = NOW(); P##hash_j(hh, hh, (const uint8_t *)m, u, v, salt); c_hash_j = NOW() - t0;       \
        memset(v, 0, sizeof(v));                                                                    \
        t0 = NOW(); P##code_encode(v, m); c_encode = NOW() - t0;                                    \
        memset(rs, 0, sizeof(rs));                                                                  \
        t0 = NOW(); P##reed_muller_decode(rs, v); c_rm = NOW() - t0;                                \
        memset(m2, 0, sizeof(m2));                                                                  \
        t0 = NOW(); P##reed_solomon_decode(m2, rs); c_rs = NOW() - t0;                              \
        if (memcmp(m, m2, (KBYTES)) != 0) return 3;                                                 \
        bench_info(#NAME "/opt/profile-1", "cycles: mul=%lu expand_h=%lu hash_h=%lu hash_j=%lu",    \
                   (unsigned long)cmin, (unsigned long)c_xof, (unsigned long)c_hash_h,              \
                   (unsigned long)c_hash_j);                                                        \
        bench_info(#NAME "/opt/profile-2", "cycles: pke_keygen=%lu pke_encrypt=%lu pke_decrypt=%lu", \
                   (unsigned long)c_keygen, (unsigned long)c_encrypt, (unsigned long)c_decrypt);    \
        bench_info(#NAME "/opt/profile-3", "cycles: code_encode=%lu rm_decode=%lu rs_decode=%lu",   \
                   (unsigned long)c_encode, (unsigned long)c_rm, (unsigned long)c_rs);              \
        return (cmax - cmin) > CT_TOLERANCE_CYCLES;                                                 \
    }

HQC_PROBE(hqc1o_, hqc1, 17669, 46, 384, 16, 67, 76)
HQC_PROBE(hqc3o_, hqc3, 35851, 56, 640, 24, 101, 114)
HQC_PROBE(hqc5o_, hqc5, 57637, 90, 640, 32, 132, 150)
