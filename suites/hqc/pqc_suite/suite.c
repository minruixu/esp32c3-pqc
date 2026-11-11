/* HQC-1/3/5 (HQC-128/192/256): known-answer tests and timings.
 *
 * Variants: "ref" is the official reference implementation, "opt" the
 * ESP32-C3 build (see NOTES.md).
 *
 * On the device the reference code only runs for HQC-1 in this image. The
 * reference code of HQC-3 needs 132 KB of stack, more than a heap block can
 * be, and runs in its own image on a static stack (suite hqc3ref, built from
 * this directory with HQC_REF3_ONLY). The reference code of HQC-5 needs
 * 211 KB of stack and does not fit this board's RAM next to the firmware;
 * it is only run on the host. */
#include <string.h>
#include "bench.h"
#include "hqc_api.h"
#include "kat_hqc.h"
#include "pqc_prims.h"
#include "sha2.h"
#ifndef PQC_HOST
#include "esp_heap_caps.h"
#endif

typedef int8_t (*keygen_fn)(uint8_t *ek, uint8_t *dk);
typedef int8_t (*encaps_fn)(uint8_t *ss, uint8_t *ct, const uint8_t *ek);
typedef int8_t (*decaps_fn)(uint8_t *ss, const uint8_t *dk, const uint8_t *ct);

typedef struct {
    const char *name;
    unsigned k;                 /* message bytes drawn by encapsulation */
    size_t pkb, ctb;
    const hqc_kat_t *kat;
    keygen_fn keygen[2];        /* [0] = ref, [1] = opt; NULL = not in this image */
    encaps_fn encaps[2];
    decaps_fn decaps[2];
    uint32_t stack[2];
    int (*probe)(void);         /* device-only measurements of the opt build */
} kem_t;

#if !defined(PQC_HOST) && !defined(HQC_REF3_ONLY)
int hqc1_probe(void);
int hqc3_probe(void);
int hqc5_probe(void);
#define PROBE(f) f
#else
#define PROBE(f) NULL
#endif

/* The reference code of HQC-3 and HQC-5 is part of the host build (so that
 * every label has a host value) but not of the main device image. */
#ifdef PQC_HOST
#define HQC_HOST_REF(f) f
#else
#define HQC_HOST_REF(f) NULL
#endif

#ifdef HQC_REF3_ONLY
/* Image "hqc3ref": only the reference code of HQC-3, run on a large static
 * stack (hqc_bigstack.c). */
#ifdef PQC_HOST
#define RUN(name, fn, iters, stack) bench_run(name, fn, NULL, iters, stack)
#else
int hqc_run_bigstack(const char *name, bench_fn fn, void *arg, int iters);
#define RUN(name, fn, iters, stack) hqc_run_bigstack(name, fn, NULL, iters)
#endif
static const kem_t kems[] = {
    {"hqc3", 24, HQC3_PK_BYTES, HQC3_CT_BYTES, hqc3_kat,
     {hqc3r_hqc_kem_keygen, NULL}, {hqc3r_hqc_kem_encaps, NULL}, {hqc3r_hqc_kem_decaps, NULL},
     {0, 0}, NULL},
};
#else
#define RUN(name, fn, iters, stack) bench_run(name, fn, NULL, iters, stack)
static const kem_t kems[] = {
    {"hqc1", 16, HQC1_PK_BYTES, HQC1_CT_BYTES, hqc1_kat,
     {hqc1r_hqc_kem_keygen, hqc1o_hqc_kem_keygen}, {hqc1r_hqc_kem_encaps, hqc1o_hqc_kem_encaps},
     {hqc1r_hqc_kem_decaps, hqc1o_hqc_kem_decaps}, {HQC_STACK_REF1, HQC_STACK_OPT1}, PROBE(hqc1_probe)},
    {"hqc3", 24, HQC3_PK_BYTES, HQC3_CT_BYTES, hqc3_kat,
     {HQC_HOST_REF(hqc3r_hqc_kem_keygen), hqc3o_hqc_kem_keygen},
     {HQC_HOST_REF(hqc3r_hqc_kem_encaps), hqc3o_hqc_kem_encaps},
     {HQC_HOST_REF(hqc3r_hqc_kem_decaps), hqc3o_hqc_kem_decaps}, {0, HQC_STACK_OPT3}, PROBE(hqc3_probe)},
    {"hqc5", 32, HQC5_PK_BYTES, HQC5_CT_BYTES, hqc5_kat,
     {HQC_HOST_REF(hqc5r_hqc_kem_keygen), hqc5o_hqc_kem_keygen},
     {HQC_HOST_REF(hqc5r_hqc_kem_encaps), hqc5o_hqc_kem_encaps},
     {HQC_HOST_REF(hqc5r_hqc_kem_decaps), hqc5o_hqc_kem_decaps}, {0, HQC_STACK_OPT5}, PROBE(hqc5_probe)},
};
#endif

/* The host replays all 100 official vectors per level. On the device the
 * slow reference code replays one, the optimized code three. */
#ifdef PQC_HOST
#define KAT_VECTORS(v) 100
#else
#define KAT_VECTORS(v) ((v) ? HQC_KAT_DEVICE_VECTORS : 1)
#endif

static const kem_t *K;
static int V;
#ifdef HQC_REF3_ONLY
static uint8_t pk[HQC3_PK_BYTES], sk[HQC_SK_BYTES], ct[HQC3_CT_BYTES];
#else
static uint8_t pk[HQC5_PK_BYTES], sk[HQC_SK_BYTES], ct[HQC5_CT_BYTES];
#endif
static uint8_t ss_a[HQC_SS_BYTES], ss_b[HQC_SS_BYTES];

/* Official known-answer vectors: the DRBG outputs are replayed through
 * hqc_rng_script(), the results compared with the official pk, sk, ct, ss. */
static int job_kat(void *arg)
{
    (void)arg;
    for (int i = 0; i < KAT_VECTORS(V); i++) {
        const hqc_kat_t *t = &K->kat[i];
        uint8_t script[48], h[32];

        hqc_rng_script(t->seed_kem, 32);
        if (K->keygen[V](pk, sk) != 0) return 1;
        if (memcmp(sk, t->seed_kem, 32) != 0) return 2;
        sha256(h, pk, K->pkb);
        if (memcmp(h, t->pk_sha256, 16) != 0) return 3;

        memcpy(script, t->m, K->k);
        memcpy(script + K->k, t->salt, 16);
        hqc_rng_script(script, K->k + 16);
        if (K->encaps[V](ss_a, ct, pk) != 0) return 4;
        sha256(h, ct, K->ctb);
        if (memcmp(h, t->ct_sha256, 16) != 0) return 5;
        if (memcmp(ss_a, t->ss, HQC_SS_BYTES) != 0) return 6;

        if (K->decaps[V](ss_b, sk, ct) != 0) return 7;
        if (memcmp(ss_b, t->ss, HQC_SS_BYTES) != 0) return 8;
    }
    /* A corrupted ciphertext must not yield the real shared secret. */
    ct[3] ^= 0x20;
    if (K->decaps[V](ss_b, sk, ct) != 0) return 9;
    if (memcmp(ss_b, ss_a, HQC_SS_BYTES) == 0) return 10;
    return 0;
}

static int job_keygen(void *arg) { (void)arg; return K->keygen[V](pk, sk); }
static int job_encaps(void *arg) { (void)arg; return K->encaps[V](ss_a, ct, pk); }
static int job_decaps(void *arg)
{
    (void)arg;
    if (K->decaps[V](ss_b, sk, ct) != 0) return 1;
    return memcmp(ss_a, ss_b, HQC_SS_BYTES) != 0;
}

/* Decapsulation of a corrupted ciphertext: the KEM must return the
 * implicit-rejection key. Its fingerprint is compared with the reference
 * like every other output, and the time should equal that of "decaps". */
static int job_reject(void *arg)
{
    int8_t rc;
    (void)arg;
    ct[K->ctb / 2] ^= 0x04;
    rc = K->decaps[V](ss_b, sk, ct);
    ct[K->ctb / 2] ^= 0x04;
    if (rc != 0) return 1;
    return memcmp(ss_a, ss_b, HQC_SS_BYTES) == 0;
}

static int job_probe(void *arg) { (void)arg; return K->probe(); }
/* Optimized building blocks against the reference ones (hqc_selftest.c).
 * Needs both codes in one program: the device image, or a host build with
 * -D HQC_HOST_OPT=1. */
#if !defined(HQC_REF3_ONLY) && (!defined(PQC_HOST) || defined(HQC_HOST_OPT))
#define HQC_HAVE_SELFTEST 1
int hqc_gf_selftest(void);
int hqc_code_selftest(uint32_t rounds);
static int job_gf_selftest(void *arg) { (void)arg; return hqc_gf_selftest(); }
#ifdef PQC_HOST
static int job_code_selftest(void *arg) { (void)arg; return hqc_code_selftest(200000); }
#else
static int job_code_selftest(void *arg) { (void)arg; return hqc_code_selftest(8); }
#endif
#endif

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};

#ifndef PQC_HOST
    /* The largest task stack that can be created in this image. */
    bench_info("heap", "free=%u largest_block=%u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
#endif
    for (V = 0; V < 2; V++) {
        const char *vn = variants[V];
        int iters = V ? 10 : 3;

        /* HQC only uses Keccak. SHA-256 is used here for the fingerprints
         * of the known-answer test and stays on the reference backend. */
        pqc_prims_select_ex(V, PQC_PRIMS_REF, PQC_PRIMS_REF);
#ifdef HQC_HAVE_SELFTEST
        if (V == 1 && bench_selected("hqc/opt/selftest")) {
            bench_check("hqc/opt/selftest-gf",
                        bench_run("hqc/opt/selftest-gf", job_gf_selftest, NULL, 1, 4096) == 0);
            bench_check("hqc/opt/selftest-code",
                        bench_run("hqc/opt/selftest-code", job_code_selftest, NULL, 1, 24000) == 0);
        }
#endif
        for (size_t i = 0; i < sizeof(kems) / sizeof(kems[0]); i++) {
            K = &kems[i];
            if (K->keygen[V] == NULL || !bench_selected(bench_name(K->name, vn, ""))) {
                continue;
            }
            bench_info(K->name, "pk=%u sk=%u ct=%u ss=%u", (unsigned)K->pkb, (unsigned)HQC_SK_BYTES,
                       (unsigned)K->ctb, (unsigned)HQC_SS_BYTES);
            bench_check(bench_name(K->name, vn, "kat"),
                        RUN(bench_name(K->name, vn, "kat"), job_kat, 1, K->stack[V]) == 0);

            bench_rng_seed(1);
            RUN(bench_name(K->name, vn, "keygen"), job_keygen, iters, K->stack[V]);
            bench_rng_seed(2);
            RUN(bench_name(K->name, vn, "encaps"), job_encaps, iters, K->stack[V]);
            RUN(bench_name(K->name, vn, "decaps"), job_decaps, iters, K->stack[V]);
            RUN(bench_name(K->name, vn, "reject"), job_reject, iters, K->stack[V]);
            bench_digest(bench_name(K->name, vn, "ss_reject"), ss_b, HQC_SS_BYTES);

            if (V == 1 && K->probe != NULL) {
                /* Timing of the multiplication for different secrets, and a
                 * cycle-count profile (INFO lines); see hqc_probe.c. */
                bench_check(bench_name(K->name, vn, "ct-mul"),
                            bench_run(bench_name(K->name, vn, "probe"), job_probe, NULL, 1,
                                      K->stack[V]) == 0);
            }

            bench_digest(bench_name(K->name, vn, "pk"), pk, K->pkb);
            bench_digest(bench_name(K->name, vn, "sk"), sk, HQC_SK_BYTES);
            bench_digest(bench_name(K->name, vn, "ct"), ct, K->ctb);
            bench_digest(bench_name(K->name, vn, "ss"), ss_a, HQC_SS_BYTES);
        }
    }
}
