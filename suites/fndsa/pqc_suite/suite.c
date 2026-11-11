/* FN-DSA (Falcon) 512 and 1024: known-answer tests and timings.
 *
 * The library's *_temp functions are used throughout: all large temporaries
 * live in one caller-provided area (22n+31 bytes for key generation, 59n+31
 * for signing, 4n+31 for verification, n = 512 or 1024), so the task stacks
 * stay small and the RAM need of each operation is known exactly. */
#include <string.h>
#include "bench.h"
#include "fips202.h"
#include "fndsa.h"
#include "kat_fndsa.h"
#include "pqc_prims.h"

typedef struct {
    const char *name;
    unsigned logn;
    const uint8_t (*kat_hash)[32];
    const uint8_t *kat_vk, *kat_sig;
} alg_t;

static const alg_t algs[] = {
    {"fndsa512", FNDSA_LOGN_512, KAT_512_HASH, KAT_512_VK, KAT_512_SIG},
    {"fndsa1024", FNDSA_LOGN_1024, KAT_1024_HASH, KAT_1024_VK, KAT_1024_SIG},
};

/* The host checks all ten upstream vectors; the device checks the first few
 * (each one is a key generation, a signature and a verification). */
#ifdef PQC_HOST
#define KAT_VECTORS 10
#else
#define KAT_VECTORS 2
#endif

#define STACK (12 * 1024)
#define KGEN_TMP(logn) (((size_t)22 << (logn)) + 31)
#define SIGN_TMP(logn) (((size_t)59 << (logn)) + 31)
#define VRFY_TMP(logn) (((size_t)4 << (logn)) + 31)

/* On the device the "opt" variant signs with the ESP32-C3 build of the
 * signing code (fndsa_sign_c3.c). The host runs the unchanged library for
 * both variants, so `tools/pqc check` compares the two implementations. */
#ifndef PQC_HOST
size_t fndsa_c3_sign_temp(const void *sign_key, size_t sign_key_len, const void *ctx, size_t ctx_len,
                          const char *id, const void *hv, size_t hv_len, void *sig, size_t max_sig_len,
                          void *tmp, size_t tmp_len);
size_t fndsa_c3_sign_seeded_temp(const void *sign_key, size_t sign_key_len, const void *ctx,
                                 size_t ctx_len, const char *id, const void *hv, size_t hv_len,
                                 const void *seed, size_t seed_len, void *sig, size_t max_sig_len,
                                 void *tmp, size_t tmp_len);
uint32_t fndsa_c3_fpr_selftest(uint32_t count);
#define SIGN_TEMP (V ? fndsa_c3_sign_temp : fndsa_sign_temp)
#define SIGN_SEEDED_TEMP (V ? fndsa_c3_sign_seeded_temp : fndsa_sign_seeded_temp)
#else
#define SIGN_TEMP fndsa_sign_temp
#define SIGN_SEEDED_TEMP fndsa_sign_seeded_temp
#endif

static const alg_t *A;
static int V; /* 0 = ref, 1 = opt */
static uint8_t sk[FNDSA_SIGN_KEY_SIZE(10)], vk[FNDSA_VRFY_KEY_SIZE(10)];
static uint8_t sig[FNDSA_SIGNATURE_SIZE(10)];
static uint8_t tmp[((size_t)59 << 10) + 31];
static const uint8_t msg[] = "post-quantum hello from an ESP32-C3";
static const uint8_t ctx[] = "pqc_c3";

/* Upstream known-answer test (c-fn-dsa test_fndsa.c, inner_test_kat). */
static int job_kat(void *arg)
{
    unsigned logn = A->logn;
    size_t sk_len = FNDSA_SIGN_KEY_SIZE(logn), vk_len = FNDSA_VRFY_KEY_SIZE(logn);
    size_t sig_len = FNDSA_SIGNATURE_SIZE(logn);
    (void)arg;

    for (uint32_t j = 0; j < KAT_VECTORS; j++) {
        uint8_t seed[5] = {(uint8_t)logn, (uint8_t)j, (uint8_t)(j >> 8), (uint8_t)(j >> 16),
                           (uint8_t)(j >> 24)};
        uint8_t seeds[32 + 40], hv[32], h[32];
        const char *id = FNDSA_HASH_ID_RAW;
        const void *m = "message";
        size_t m_len = 7;
        sha3_256incctx sc;

        shake256(seeds, sizeof(seeds), seed, sizeof(seed));
        if (!fndsa_keygen_seeded_temp(logn, seeds, 32, sk, vk, tmp, KGEN_TMP(logn))) return 1;
        if (j & 1) {
            id = FNDSA_HASH_ID_SHA3_256;
            sha3_256(hv, (const uint8_t *)"message", 7);
            m = hv;
            m_len = 32;
        }
        if (SIGN_SEEDED_TEMP(sk, sk_len, "domain", 6, id, m, m_len, seeds + 32, 40, sig,
                             sig_len, tmp, SIGN_TMP(logn)) != sig_len) return 2;
        if (!fndsa_verify_temp(sig, sig_len, vk, vk_len, "domain", 6, id, m, m_len, tmp,
                               VRFY_TMP(logn))) return 3;
        sha3_256_inc_init(&sc);
        sha3_256_inc_absorb(&sc, sk, sk_len);
        sha3_256_inc_absorb(&sc, vk, vk_len);
        sha3_256_inc_absorb(&sc, sig, sig_len);
        sha3_256_inc_finalize(h, &sc);
        if (memcmp(h, A->kat_hash[j], 32) != 0) return 10 + (int)j;
    }

    /* Upstream verification vector, then the same with one bit flipped. */
    memcpy(sig, A->kat_sig, sig_len);
    if (!fndsa_verify_temp(sig, sig_len, A->kat_vk, vk_len, "context", 7, FNDSA_HASH_ID_RAW,
                           "message", 7, tmp, VRFY_TMP(logn))) return 4;
    sig[sig_len / 2] ^= 0x10;
    if (fndsa_verify_temp(sig, sig_len, A->kat_vk, vk_len, "context", 7, FNDSA_HASH_ID_RAW,
                          "message", 7, tmp, VRFY_TMP(logn))) return 5;
    return 0;
}

static int job_keygen(void *arg)
{
    (void)arg;
    return !fndsa_keygen_temp(A->logn, sk, vk, tmp, KGEN_TMP(A->logn));
}

static int job_sign(void *arg)
{
    size_t sig_len = FNDSA_SIGNATURE_SIZE(A->logn);
    (void)arg;
    return SIGN_TEMP(sk, FNDSA_SIGN_KEY_SIZE(A->logn), ctx, sizeof(ctx) - 1,
                     FNDSA_HASH_ID_RAW, msg, sizeof(msg) - 1, sig, sig_len, tmp,
                     SIGN_TMP(A->logn)) != sig_len;
}

static int job_verify(void *arg)
{
    (void)arg;
    return !fndsa_verify_temp(sig, FNDSA_SIGNATURE_SIZE(A->logn), vk, FNDSA_VRFY_KEY_SIZE(A->logn),
                              ctx, sizeof(ctx) - 1, FNDSA_HASH_ID_RAW, msg, sizeof(msg) - 1, tmp,
                              VRFY_TMP(A->logn));
}

#ifndef PQC_HOST
/* fpr_c3.c against c-fn-dsa's emulated floating point, 20000 operand pairs. */
static int job_fpr_selftest(void *arg)
{
    (void)arg;
    return fndsa_c3_fpr_selftest(20000) != 0;
}
#endif

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};

    for (V = 0; V < 2; V++) {
        const char *vn = variants[V];
        /* FN-DSA only uses Keccak; the other primitives stay on the
         * reference backend. */
        pqc_prims_select_ex(V, PQC_PRIMS_REF, PQC_PRIMS_REF);
#ifndef PQC_HOST
        if (V == 1 && bench_selected("fndsa/opt/fpr-selftest")) {
            bench_check("fndsa/opt/fpr-selftest",
                        bench_run("fndsa/opt/fpr-selftest", job_fpr_selftest, NULL, 1, STACK) == 0);
        }
#endif
        for (size_t i = 0; i < sizeof(algs) / sizeof(algs[0]); i++) {
            unsigned logn;
            A = &algs[i];
            logn = A->logn;
            if (!bench_selected(bench_name(A->name, vn, ""))) {
                continue;
            }
            bench_info(A->name, "sk=%u vk=%u sig=%u tmp_keygen=%u tmp_sign=%u tmp_verify=%u",
                       (unsigned)FNDSA_SIGN_KEY_SIZE(logn), (unsigned)FNDSA_VRFY_KEY_SIZE(logn),
                       (unsigned)FNDSA_SIGNATURE_SIZE(logn), (unsigned)KGEN_TMP(logn),
                       (unsigned)SIGN_TMP(logn), (unsigned)VRFY_TMP(logn));
            bench_check(bench_name(A->name, vn, "kat"),
                        bench_run(bench_name(A->name, vn, "kat"), job_kat, NULL, 1, STACK) == 0);

            bench_rng_seed(1);
            bench_run(bench_name(A->name, vn, "keygen"), job_keygen, NULL, logn == 9 ? 4 : 2, STACK);
            bench_rng_seed(2);
            bench_run(bench_name(A->name, vn, "sign"), job_sign, NULL, 10, STACK);
            bench_run(bench_name(A->name, vn, "verify"), job_verify, NULL, 10, STACK);

            bench_digest(bench_name(A->name, vn, "sk"), sk, FNDSA_SIGN_KEY_SIZE(logn));
            bench_digest(bench_name(A->name, vn, "vk"), vk, FNDSA_VRFY_KEY_SIZE(logn));
            bench_digest(bench_name(A->name, vn, "sig"), sig, FNDSA_SIGNATURE_SIZE(logn));
        }
    }
}
