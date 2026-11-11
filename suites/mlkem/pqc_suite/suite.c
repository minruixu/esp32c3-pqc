/* ML-KEM-512/768/1024: known-answer tests, timings of the three operations,
 * and timings of the internal routines that the ESP32-C3 backends replace.
 *
 *   ref  mlkem-native as shipped (portable C)
 *   opt  the same code with the Keccak and arithmetic backends of this
 *        project (see mlkem_opt.c)
 */
#include <string.h>
#include "bench.h"
#include "pqc_prims.h"
#include "mlkem_all.h"
#include "kat_vectors.h"
#include "c3_prof.h"

#define Q 3329

typedef struct {
    const char *name;
    size_t pkb, skb, ctb;
    int (*keypair)(uint8_t *pk, uint8_t *sk);
    int (*keypair_derand)(uint8_t *pk, uint8_t *sk, const uint8_t *coins);
    int (*enc)(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
    int (*enc_derand)(uint8_t *ct, uint8_t *ss, const uint8_t *pk, const uint8_t *coins);
    int (*dec)(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);
    const uint8_t *kat_pk, *kat_sk, *kat_ct, *kat_ss;
} kem_t;

/* internal routines, by their symbol names */
typedef struct {
    void (*ntt)(void *p);
    void (*intt)(void *p);
    void (*mulcache)(void *cache, const void *p);
    void (*reduce)(void *p);
    void (*tomont)(void *p);
    void (*basemul3)(void *r, const void *a, const void *b, const void *cache);
    void (*gen_matrix3)(void *a, const uint8_t *seed, int transposed);
} prim_t;

#define DECLARE(P)                                                                      \
    int P##512_keypair(uint8_t *, uint8_t *);                                            \
    int P##512_keypair_derand(uint8_t *, uint8_t *, const uint8_t *);                    \
    int P##512_enc(uint8_t *, uint8_t *, const uint8_t *);                               \
    int P##512_enc_derand(uint8_t *, uint8_t *, const uint8_t *, const uint8_t *);       \
    int P##512_dec(uint8_t *, const uint8_t *, const uint8_t *);                         \
    int P##768_keypair(uint8_t *, uint8_t *);                                            \
    int P##768_keypair_derand(uint8_t *, uint8_t *, const uint8_t *);                    \
    int P##768_enc(uint8_t *, uint8_t *, const uint8_t *);                               \
    int P##768_enc_derand(uint8_t *, uint8_t *, const uint8_t *, const uint8_t *);       \
    int P##768_dec(uint8_t *, const uint8_t *, const uint8_t *);                         \
    int P##1024_keypair(uint8_t *, uint8_t *);                                           \
    int P##1024_keypair_derand(uint8_t *, uint8_t *, const uint8_t *);                   \
    int P##1024_enc(uint8_t *, uint8_t *, const uint8_t *);                              \
    int P##1024_enc_derand(uint8_t *, uint8_t *, const uint8_t *, const uint8_t *);      \
    int P##1024_dec(uint8_t *, const uint8_t *, const uint8_t *);                        \
    void P##_poly_ntt(void *);                                                           \
    void P##_poly_invntt_tomont(void *);                                                 \
    void P##_poly_mulcache_compute(void *, const void *);                                \
    void P##_poly_reduce(void *);                                                        \
    void P##_poly_tomont(void *);                                                        \
    void P##768_polyvec_basemul_acc_montgomery_cached(void *, const void *, const void *, const void *); \
    void P##768_gen_matrix(void *, const uint8_t *, int);

DECLARE(mlkemopt)
void mlkem_poly_ntt(void *);
void mlkem_poly_invntt_tomont(void *);
void mlkem_poly_mulcache_compute(void *, const void *);
void mlkem_poly_reduce(void *);
void mlkem_poly_tomont(void *);
void mlkem768_polyvec_basemul_acc_montgomery_cached(void *, const void *, const void *, const void *);
void mlkem768_gen_matrix(void *, const uint8_t *, int);

#define KEM(P, L)                                                                    \
    {"mlkem" #L, MLKEM##L##_PUBLICKEYBYTES, MLKEM##L##_SECRETKEYBYTES,               \
     MLKEM##L##_CIPHERTEXTBYTES, P##L##_keypair, P##L##_keypair_derand, P##L##_enc,  \
     P##L##_enc_derand, P##L##_dec, test_vector_pk_##L, test_vector_sk_##L,          \
     test_vector_ct_##L, test_vector_ss_##L}
#define PRIM(P)                                                                       \
    {P##_poly_ntt, P##_poly_invntt_tomont, P##_poly_mulcache_compute, P##_poly_reduce, \
     P##_poly_tomont, P##768_polyvec_basemul_acc_montgomery_cached, P##768_gen_matrix}

static const kem_t kems[2][3] = {
    {KEM(mlkem, 512), KEM(mlkem, 768), KEM(mlkem, 1024)},
    {KEM(mlkemopt, 512), KEM(mlkemopt, 768), KEM(mlkemopt, 1024)},
};
static const prim_t prims[2] = {PRIM(mlkem), PRIM(mlkemopt)};
static const char *const variants[] = {"ref", "opt"};

#define STACK (32 * 1024)
#define ITERS 10

#if defined(PQC_HOST)
unsigned long pqc_perm_count; /* incremented in c3_sponge.c */
#define PERMS_RESET() (pqc_perm_count = 0)
#define PERMS_REPORT(alg, op, iters) \
    bench_info(bench_name(alg, "perms", op), "%lu Keccak permutations", pqc_perm_count / (iters))
#else
#define PERMS_RESET() ((void)0)
#define PERMS_REPORT(alg, op, iters) ((void)0)
#endif

static const kem_t *K;
static uint8_t pk[MLKEM1024_PUBLICKEYBYTES], sk[MLKEM1024_SECRETKEYBYTES];
static uint8_t ct[MLKEM1024_CIPHERTEXTBYTES], ss_a[MLKEM_BYTES], ss_b[MLKEM_BYTES];

/* Known-answer test against the vectors shipped with mlkem-native. */
static int job_kat(void *arg)
{
    uint8_t coins[2 * MLKEM_SYMBYTES];
    (void)arg;

    memcpy(coins, test_vector_d, MLKEM_SYMBYTES);
    memcpy(coins + MLKEM_SYMBYTES, test_vector_z, MLKEM_SYMBYTES);
    if (K->keypair_derand(pk, sk, coins) != 0) return 1;
    if (memcmp(pk, K->kat_pk, K->pkb) != 0) return 2;
    if (memcmp(sk, K->kat_sk, K->skb) != 0) return 3;
    if (K->enc_derand(ct, ss_a, K->kat_pk, test_vector_m) != 0) return 4;
    if (memcmp(ct, K->kat_ct, K->ctb) != 0) return 5;
    if (memcmp(ss_a, K->kat_ss, MLKEM_BYTES) != 0) return 6;
    if (K->dec(ss_b, K->kat_ct, K->kat_sk) != 0) return 7;
    if (memcmp(ss_b, K->kat_ss, MLKEM_BYTES) != 0) return 8;
    /* A corrupted ciphertext must not yield the real shared secret. */
    ct[0] ^= 1;
    if (K->dec(ss_b, ct, K->kat_sk) != 0) return 9;
    if (memcmp(ss_b, K->kat_ss, MLKEM_BYTES) == 0) return 10;
    return 0;
}

static int job_keygen(void *arg) { (void)arg; return K->keypair(pk, sk); }
static int job_encaps(void *arg) { (void)arg; return K->enc(ct, ss_a, pk); }
static int job_decaps(void *arg)
{
    (void)arg;
    if (K->dec(ss_b, ct, sk) != 0) return 1;
    return memcmp(ss_a, ss_b, MLKEM_BYTES) != 0;
}

/* ---------- internal routines ----------
 * Working buffers live on the job's stack, as they do inside the scheme. The
 * outputs are fingerprinted after reduction to [0, q): the two variants use
 * different internal representations of the same residues. */
#define ALIGNED __attribute__((aligned(16)))
#define PRIM_CALLS 100

static const prim_t *PR;
static int16_t src_poly[256] ALIGNED, src_vec[3 * 256] ALIGNED, src_a[3 * 256] ALIGNED;
static int16_t out_poly[256], out_big[9 * 256];
static const char *prim_op;

static void canon(int16_t *dst, const int16_t *x, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int32_t v = x[i] % Q;
        dst[i] = (int16_t)(v < 0 ? v + Q : v);
    }
}

static int job_prim(void *arg)
{
    int16_t w[256] ALIGNED, a[3 * 256] ALIGNED, b[3 * 256] ALIGNED, cache[3 * 128] ALIGNED;
    (void)arg;

    if (!strcmp(prim_op, "copy")) {
        for (int i = 0; i < PRIM_CALLS; i++) memcpy(w, src_poly, sizeof(w));
    } else if (!strcmp(prim_op, "ntt")) {
        for (int i = 0; i < PRIM_CALLS; i++) {
            memcpy(w, src_poly, sizeof(w));
            PR->ntt(w);
        }
        canon(out_poly, w, 256);
    } else if (!strcmp(prim_op, "invntt")) {
        for (int i = 0; i < PRIM_CALLS; i++) {
            memcpy(w, src_poly, sizeof(w));
            PR->intt(w);
        }
        canon(out_poly, w, 256);
    } else if (!strcmp(prim_op, "reduce")) {
        for (int i = 0; i < PRIM_CALLS; i++) {
            memcpy(w, src_vec, sizeof(w));
            PR->reduce(w);
        }
        canon(out_poly, w, 256);
    } else if (!strcmp(prim_op, "tomont")) {
        for (int i = 0; i < PRIM_CALLS; i++) {
            memcpy(w, src_poly, sizeof(w));
            PR->tomont(w);
        }
        canon(out_poly, w, 256);
    } else if (!strcmp(prim_op, "mulcache")) {
        memcpy(b, src_vec, sizeof(w));
        for (int i = 0; i < PRIM_CALLS; i++) PR->mulcache(cache, b);
        canon(out_poly, cache, 128);
        memset(out_poly + 128, 0, 128 * sizeof(int16_t));
    } else if (!strcmp(prim_op, "basemul_k3")) {
        memcpy(a, src_a, sizeof(a));
        memcpy(b, src_vec, sizeof(b));
        for (int v = 0; v < 3; v++) PR->mulcache(cache + 128 * v, b + 256 * v);
        for (int i = 0; i < PRIM_CALLS; i++) PR->basemul3(w, a, b, cache);
        canon(out_poly, w, 256);
    }
    return 0;
}

static int job_gen_matrix(void *arg)
{
    int16_t mat[9 * 256] ALIGNED;
    static const uint8_t seed[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    (void)arg;
    for (int i = 0; i < 10; i++) {
        PR->gen_matrix3(mat, seed, 0);
    }
    canon(out_big, mat, 9 * 256);
    return 0;
}

static void run_prims(const char *vn)
{
    static const char *const ops[] = {"copy", "ntt", "invntt", "basemul_k3", "mulcache", "reduce", "tomont"};
    char label[48];

    bench_rng_seed(3);
    randombytes((uint8_t *)src_vec, sizeof(src_vec));
    randombytes((uint8_t *)src_a, sizeof(src_a));
    for (int i = 0; i < 3 * 256; i++) {
        src_a[i] = (int16_t)((uint16_t)src_a[i] % Q);          /* [0, q) */
        src_vec[i] = (int16_t)(src_vec[i] % Q);                /* (-q, q) */
    }
    memcpy(src_poly, src_vec, sizeof(src_poly));
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        prim_op = ops[i];
        strcpy(label, ops[i]);
        strcat(label, "_x100");
        bench_run(bench_name("mlkem-prim", vn, label), job_prim, NULL, 3, 16 * 1024);
        if (i > 0) {
            bench_digest(bench_name("mlkem-prim", vn, ops[i]), out_poly, sizeof(out_poly));
        }
    }
    bench_run(bench_name("mlkem-prim", vn, "gen_matrix_k3_x10"), job_gen_matrix, NULL, 3, 16 * 1024);
    bench_digest(bench_name("mlkem-prim", vn, "gen_matrix_k3"), out_big, sizeof(out_big));
}

#if defined(PQC_PROFILE) && !defined(PQC_HOST)
/* -D PQC_PROFILE=1: where does the time go in the optimized ML-KEM-768? */
static int job_profile(void *arg)
{
    int (*fn)(void *) = (int (*)(void *))arg;
    c3_prof_start();
    for (int i = 0; i < 2500; i++) {
        fn(NULL);
    }
    c3_prof_stop();
    return 0;
}

static void run_profile(void)
{
    pqc_prims_select(PQC_PRIMS_OPT);
    K = &kems[1][1];
    bench_rng_seed(1);
    bench_run("mlkem768/opt/profile_keygen", job_profile, (void *)job_keygen, 1, STACK);
    c3_prof_report("mlkem768_keygen", 160);
    bench_run("mlkem768/opt/profile_encaps", job_profile, (void *)job_encaps, 1, STACK);
    c3_prof_report("mlkem768_encaps", 160);
    bench_run("mlkem768/opt/profile_decaps", job_profile, (void *)job_decaps, 1, STACK);
    c3_prof_report("mlkem768_decaps", 160);
}
#endif

void suite_main(void)
{
#if defined(PQC_PROFILE) && !defined(PQC_HOST)
    run_profile();
    return;
#endif
    for (int v = 0; v < 2; v++) {
        const char *vn = variants[v];
        pqc_prims_select(v);
        for (size_t i = 0; i < 3; i++) {
            K = &kems[v][i];
            if (!bench_selected(bench_name(K->name, vn, ""))) {
                continue;
            }
            bench_info(K->name, "pk=%u sk=%u ct=%u ss=%u", (unsigned)K->pkb, (unsigned)K->skb,
                       (unsigned)K->ctb, (unsigned)MLKEM_BYTES);
            bench_check(bench_name(K->name, vn, "kat"),
                        bench_run(bench_name(K->name, vn, "kat"), job_kat, NULL, 1, STACK) == 0);

            bench_rng_seed(1);
            PERMS_RESET();
            bench_run(bench_name(K->name, vn, "keygen"), job_keygen, NULL, ITERS, STACK);
            if (v) PERMS_REPORT(K->name, "keygen", ITERS);
            bench_rng_seed(2);
            PERMS_RESET();
            bench_run(bench_name(K->name, vn, "encaps"), job_encaps, NULL, ITERS, STACK);
            if (v) PERMS_REPORT(K->name, "encaps", ITERS);
            PERMS_RESET();
            bench_run(bench_name(K->name, vn, "decaps"), job_decaps, NULL, ITERS, STACK);
            if (v) PERMS_REPORT(K->name, "decaps", ITERS);

            bench_digest(bench_name(K->name, vn, "pk"), pk, K->pkb);
            bench_digest(bench_name(K->name, vn, "sk"), sk, K->skb);
            bench_digest(bench_name(K->name, vn, "ct"), ct, K->ctb);
            bench_digest(bench_name(K->name, vn, "ss"), ss_a, MLKEM_BYTES);
        }
        if (bench_selected(bench_name("mlkem-prim", vn, ""))) {
            PR = &prims[v];
            run_prims(vn);
        }
    }
}
