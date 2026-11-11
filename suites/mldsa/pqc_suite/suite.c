/* ML-DSA-44/65/87: known-answer tests and timings.
 *
 *   ref     mldsa-native as shipped (portable C)
 *   opt     the same code with the fast Keccak of this project and
 *           mldsa-native's RV32IM arithmetic backend (see mldsa_opt_config.h)
 *   lowram  opt plus MLD_CONFIG_REDUCE_RAM (built as the suite mldsa_lowram)
 */
#include <string.h>
#include "bench.h"
#include "pqc_prims.h"
#include "mldsa_all.h"
#include "kat_vectors.h"
#include "c3_prof.h"

typedef struct {
    const char *name;
    size_t pkb, skb, sigb;
    uint32_t stack;
    int (*keypair)(uint8_t *pk, uint8_t *sk);
    int (*keypair_internal)(uint8_t *pk, uint8_t *sk, const uint8_t *seed);
    int (*signature)(uint8_t *sig, const uint8_t *m, size_t mlen, const uint8_t *ctx, size_t ctxlen,
                     const uint8_t *sk);
    int (*signature_internal)(uint8_t *sig, const uint8_t *m, size_t mlen, const uint8_t *pre,
                              size_t prelen, const uint8_t *rnd, const uint8_t *sk, int externalmu);
    int (*verify)(const uint8_t *sig, const uint8_t *m, size_t mlen, const uint8_t *ctx, size_t ctxlen,
                  const uint8_t *pk);
    size_t (*prefix)(uint8_t *pre, const uint8_t *ph, size_t phlen, const uint8_t *ctx, size_t ctxlen,
                     int hashalg);
    const uint8_t *kat_pk, *kat_sk, *kat_sig;
} dsa_t;

#define DECLARE1(P, L)                                                                               \
    int P##L##_keypair(uint8_t *, uint8_t *);                                                         \
    int P##L##_keypair_internal(uint8_t *, uint8_t *, const uint8_t *);                               \
    int P##L##_signature(uint8_t *, const uint8_t *, size_t, const uint8_t *, size_t, const uint8_t *); \
    int P##L##_signature_internal(uint8_t *, const uint8_t *, size_t, const uint8_t *, size_t,        \
                                  const uint8_t *, const uint8_t *, int);                             \
    int P##L##_verify(const uint8_t *, const uint8_t *, size_t, const uint8_t *, size_t, const uint8_t *); \
    size_t P##L##_prepare_domain_separation_prefix(uint8_t *, const uint8_t *, size_t, const uint8_t *, \
                                                   size_t, int);
#define DECLARE(P) DECLARE1(P, 44) DECLARE1(P, 65) DECLARE1(P, 87)

#define DSA(P, L, STK)                                                                    \
    {"mldsa" #L, MLDSA##L##_PUBLICKEYBYTES, MLDSA##L##_SECRETKEYBYTES, MLDSA##L##_BYTES,  \
     STK, P##L##_keypair, P##L##_keypair_internal, P##L##_signature,                      \
     P##L##_signature_internal, P##L##_verify, P##L##_prepare_domain_separation_prefix,   \
     test_vector_pk_##L, test_vector_sk_##L, test_vector_sig_##L}

#if defined(PQC_MLDSA_LOWRAM)
DECLARE(mldsalr)
#define NVAR 1
#define NLEV 3
#define LS (24 * 1024)
static const dsa_t dsas[NVAR][NLEV] = {{DSA(mldsalr, 44, LS), DSA(mldsalr, 65, LS), DSA(mldsalr, 87, LS)}};
static const char *const variants[NVAR] = {"lowram"};
static const int prims_of[NVAR] = {PQC_PRIMS_OPT};
#elif defined(PQC_MLDSA_ONLY_87)
DECLARE1(mldsaopt, 87)
#define NVAR 2
#define NLEV 1
#define S87 (24 * 1024)   /* the large temporaries are on the heap, see mldsa_heap_alloc.h */
static const dsa_t dsas[NVAR][NLEV] = {{DSA(mldsa, 87, S87)}, {DSA(mldsaopt, 87, S87)}};
static const char *const variants[NVAR] = {"ref", "opt"};
static const int prims_of[NVAR] = {PQC_PRIMS_REF, PQC_PRIMS_OPT};
#else
DECLARE(mldsaopt)
#define NVAR 2
#define NLEV 2
#define S44 (56 * 1024)
#define S65 (84 * 1024)
static const dsa_t dsas[NVAR][NLEV] = {
    {DSA(mldsa, 44, S44), DSA(mldsa, 65, S65)},
    {DSA(mldsaopt, 44, S44), DSA(mldsaopt, 65, S65)},
};
static const char *const variants[NVAR] = {"ref", "opt"};
static const int prims_of[NVAR] = {PQC_PRIMS_REF, PQC_PRIMS_OPT};
#endif

#if defined(PQC_HOST)
unsigned long pqc_perm_count; /* incremented in c3_sponge.c */
#define PERMS_RESET() (pqc_perm_count = 0)
#define PERMS_REPORT(alg, op, iters) \
    bench_info(bench_name(alg, "perms", op), "%lu Keccak permutations", pqc_perm_count / (iters))
#else
#define PERMS_RESET() ((void)0)
#define PERMS_REPORT(alg, op, iters) ((void)0)
#endif

static const dsa_t *D;
static uint8_t pk[MLDSA87_PUBLICKEYBYTES], sk[MLDSA87_SECRETKEYBYTES], sig[MLDSA87_BYTES];
static const uint8_t msg[] = "post-quantum hello from an ESP32-C3, signed with ML-DSA";
static const uint8_t ctx[] = "pqc_c3";

/* Known-answer test against the vectors shipped with mldsa-native. */
static int job_kat(void *arg)
{
    uint8_t pre[MLD_DOMAIN_SEPARATION_MAX_BYTES];
    size_t pre_len;
    (void)arg;

    if (D->keypair_internal(pk, sk, test_vector_rnd) != 0) return 1;
    if (memcmp(pk, D->kat_pk, D->pkb) != 0) return 2;
    if (memcmp(sk, D->kat_sk, D->skb) != 0) return 3;
    pre_len = D->prefix(pre, NULL, 0, (const uint8_t *)TEST_VECTOR_CTX, TEST_VECTOR_CTX_LEN,
                        MLD_PREHASH_NONE);
    if (pre_len == 0) return 4;
    if (D->signature_internal(sig, (const uint8_t *)TEST_VECTOR_MSG, TEST_VECTOR_MSG_LEN, pre, pre_len,
                              test_vector_rnd, D->kat_sk, 0) != 0) return 5;
    if (memcmp(sig, D->kat_sig, D->sigb) != 0) return 6;
    if (D->verify(D->kat_sig, (const uint8_t *)TEST_VECTOR_MSG, TEST_VECTOR_MSG_LEN,
                  (const uint8_t *)TEST_VECTOR_CTX, TEST_VECTOR_CTX_LEN, D->kat_pk) != 0) return 7;
    /* A corrupted signature must be rejected. */
    sig[10] ^= 1;
    if (D->verify(sig, (const uint8_t *)TEST_VECTOR_MSG, TEST_VECTOR_MSG_LEN,
                  (const uint8_t *)TEST_VECTOR_CTX, TEST_VECTOR_CTX_LEN, D->kat_pk) == 0) return 8;
    return 0;
}

static int job_keygen(void *arg) { (void)arg; return D->keypair(pk, sk); }
static int job_sign(void *arg)
{
    (void)arg;
    return D->signature(sig, msg, sizeof(msg) - 1, ctx, sizeof(ctx) - 1, sk);
}
static int job_verify(void *arg)
{
    (void)arg;
    return D->verify(sig, msg, sizeof(msg) - 1, ctx, sizeof(ctx) - 1, pk);
}

#if defined(PQC_PROFILE) && !defined(PQC_HOST)
/* -D PQC_PROFILE=1: where does the time go in the last variant of the first
 * parameter set of this image? */
static int profile_n;
static int job_profile(void *arg)
{
    int (*fn)(void *) = (int (*)(void *))arg;
    c3_prof_start();
    for (int i = 0; i < profile_n; i++) {
        fn(NULL);
    }
    c3_prof_stop();
    return 0;
}

static void run_profile(void)
{
    char label[40];
    pqc_prims_select(prims_of[NVAR - 1]);
    D = &dsas[NVAR - 1][NLEV - 1];
    bench_rng_seed(1);
    profile_n = 600;
    bench_run(bench_name(D->name, variants[NVAR - 1], "profile_keygen"), job_profile, (void *)job_keygen, 1, D->stack);
    strcpy(label, D->name); strcat(label, "_keygen");
    c3_prof_report(label, 200);
    profile_n = 400;
    bench_run(bench_name(D->name, variants[NVAR - 1], "profile_sign"), job_profile, (void *)job_sign, 1, D->stack);
    strcpy(label, D->name); strcat(label, "_sign");
    c3_prof_report(label, 220);
    profile_n = 600;
    bench_run(bench_name(D->name, variants[NVAR - 1], "profile_verify"), job_profile, (void *)job_verify, 1, D->stack);
    strcpy(label, D->name); strcat(label, "_verify");
    c3_prof_report(label, 200);
}
#endif

#if !defined(PQC_MLDSA_LOWRAM) && !defined(PQC_MLDSA_ONLY_87)
/* ---------- the transforms on their own ----------
 *   ref  the C code of mldsa-native
 *   up   mldsa-native's RV32IM assembly (device only)
 *   opt  the assembly of this project (the C code on the build machine)
 * The outputs are fingerprinted after reduction to [0, q): the variants use
 * different representatives of the same residues. */
#define MLDSA_Q 8380417
#define ALIGNED __attribute__((aligned(16)))
void mldsa_poly_ntt(void *);
void mldsa_poly_invntt_tomont(void *);
void mldsa_poly_pointwise_montgomery(void *, const void *);
void mldsaopt_poly_ntt(void *);
void mldsaopt_poly_invntt_tomont(void *);
void mldsaopt_poly_pointwise_montgomery(void *, const void *);
#if !defined(PQC_HOST)
void mldsaopt_ntt_rv32im_fastmul_asm(int32_t *, const int32_t *);
void mldsaopt_intt_rv32im_fastmul_asm(int32_t *, const int32_t *);
extern const int32_t mldsaopt_rv32im_ntt_zetas[];
static void up_ntt(void *p) { mldsaopt_ntt_rv32im_fastmul_asm(p, mldsaopt_rv32im_ntt_zetas); }
static void up_intt(void *p) { mldsaopt_intt_rv32im_fastmul_asm(p, mldsaopt_rv32im_ntt_zetas); }
void mldsaopt_poly_pointwise_montgomery_rv32im_asm(int32_t *, const int32_t *);
static void up_pointwise(void *a, const void *b) { mldsaopt_poly_pointwise_montgomery_rv32im_asm(a, b); }
#endif

typedef struct {
    const char *name;
    void (*ntt)(void *);
    void (*intt)(void *);
    void (*pointwise)(void *, const void *);
    int prims;
} tprim_t;

static const tprim_t tprims[] = {
    {"ref", mldsa_poly_ntt, mldsa_poly_invntt_tomont, mldsa_poly_pointwise_montgomery, PQC_PRIMS_REF},
#if !defined(PQC_HOST)
    {"up", up_ntt, up_intt, up_pointwise, PQC_PRIMS_OPT},
#endif
    {"opt", mldsaopt_poly_ntt, mldsaopt_poly_invntt_tomont, mldsaopt_poly_pointwise_montgomery,
     PQC_PRIMS_OPT},
};

static const tprim_t *TP;
static const char *tprim_op;
static int32_t tsrc[256] ALIGNED, tsrc2[256] ALIGNED, tout[256];

static void canon32(int32_t *dst, const int32_t *x)
{
    for (int i = 0; i < 256; i++) {
        int32_t v = x[i] % MLDSA_Q;
        dst[i] = v < 0 ? v + MLDSA_Q : v;
    }
}

static int job_tprim(void *arg)
{
    int32_t w[256] ALIGNED, b[256] ALIGNED;
    (void)arg;

    memcpy(b, tsrc2, sizeof(b));
    for (int i = 0; i < 100; i++) {
        memcpy(w, tsrc, sizeof(w));
        if (tprim_op[0] == 'n') {
            TP->ntt(w);
        } else if (tprim_op[0] == 'i') {
            TP->intt(w);
        } else {
            TP->pointwise(w, b);
        }
    }
    canon32(tout, w);
    return 0;
}

static void run_tprims(void)
{
    static const char *const ops[] = {"ntt", "invntt", "pointwise"};
    char label[32];

    bench_rng_seed(5);
    randombytes((uint8_t *)tsrc, sizeof(tsrc));
    randombytes((uint8_t *)tsrc2, sizeof(tsrc2));
    for (int i = 0; i < 256; i++) {
        tsrc[i] %= MLDSA_Q;      /* (-q, q), as the transforms require */
        tsrc2[i] %= MLDSA_Q;
    }
    for (size_t v = 0; v < sizeof(tprims) / sizeof(tprims[0]); v++) {
        TP = &tprims[v];
        if (!bench_selected(bench_name("mldsa-prim", TP->name, ""))) {
            continue;
        }
        pqc_prims_select(TP->prims);
        for (size_t i = 0; i < 3; i++) {
            if (i == 2 && TP->pointwise == NULL) {
                continue;
            }
            tprim_op = ops[i];
            strcpy(label, ops[i]);
            strcat(label, "_x100");
            bench_run(bench_name("mldsa-prim", TP->name, label), job_tprim, NULL, 3, 8 * 1024);
            /* "up" exists on the device only; it must agree with "opt" */
            bench_digest(bench_name("mldsa-prim", strcmp(TP->name, "up") ? TP->name : "opt", ops[i]),
                         tout, sizeof(tout));
        }
    }
}
#endif

void suite_main(void)
{
#if defined(PQC_PROFILE) && !defined(PQC_HOST)
    run_profile();
    return;
#endif
#if !defined(PQC_MLDSA_LOWRAM) && !defined(PQC_MLDSA_ONLY_87)
    run_tprims();
#endif
    for (int v = 0; v < NVAR; v++) {
        const char *vn = variants[v];
        pqc_prims_select(prims_of[v]);
        for (size_t i = 0; i < NLEV; i++) {
            D = &dsas[v][i];
            if (!bench_selected(bench_name(D->name, vn, ""))) {
                continue;
            }
            bench_info(D->name, "pk=%u sk=%u sig=%u", (unsigned)D->pkb, (unsigned)D->skb,
                       (unsigned)D->sigb);
            bench_check(bench_name(D->name, vn, "kat"),
                        bench_run(bench_name(D->name, vn, "kat"), job_kat, NULL, 1, D->stack) == 0);

            bench_rng_seed(1);
            PERMS_RESET();
            bench_run(bench_name(D->name, vn, "keygen"), job_keygen, NULL, 5, D->stack);
            if (prims_of[v]) PERMS_REPORT(D->name, "keygen", 5);
            /* 20 signatures with fresh randomness each; the random stream is
             * the same for every variant, so the averages are comparable. */
            bench_rng_seed(2);
            PERMS_RESET();
            bench_run(bench_name(D->name, vn, "sign"), job_sign, NULL, 20, D->stack);
            if (prims_of[v]) PERMS_REPORT(D->name, "sign", 20);
            PERMS_RESET();
            bench_run(bench_name(D->name, vn, "verify"), job_verify, NULL, 5, D->stack);
            if (prims_of[v]) PERMS_REPORT(D->name, "verify", 5);

            bench_digest(bench_name(D->name, vn, "pk"), pk, D->pkb);
            bench_digest(bench_name(D->name, vn, "sk"), sk, D->skb);
            bench_digest(bench_name(D->name, vn, "sig"), sig, D->sigb);
        }
    }
}
