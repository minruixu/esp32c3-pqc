/* SLH-DSA (FIPS 205): known-answer tests from the NIST ACVP sample vectors
 * and timings, for all 12 parameter sets.
 *
 * The timed operations are the known-answer tests themselves: key generation
 * from the ACVP seeds, deterministic signing of the ACVP message, and
 * verification of that signature. */
#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "pqc_prims.h"
#include "sha2_api.h"
#include "slh_dsa.h"
#include "slh_param.h"
#include "kat_vectors.h"

#ifndef PQC_HOST
#include "sha256_hw.h"
#include "slh_c3.h"
#define C3(x) (&slh_c3_##x)
#define FAST(x) (&slh_fast_##x)
#else
#define C3(x) (&slh_dsa_##x)
#define FAST(x) (&slh_dsa_##x)
#endif

typedef struct {
    const char *name;
    const slh_param_t *ref, *opt;
    int fast; /* an "f" set: cheap enough for several iterations */
} set_t;

/* Same order as slh_kats[] in kat_vectors.h. */
static const set_t sets[] = {
    {"slhdsa-sha2-128s", &slh_dsa_sha2_128s, C3(sha2_128s), 0},
    {"slhdsa-sha2-128f", &slh_dsa_sha2_128f, C3(sha2_128f), 1},
    {"slhdsa-sha2-192s", &slh_dsa_sha2_192s, C3(sha2_192s), 0},
    {"slhdsa-sha2-192f", &slh_dsa_sha2_192f, C3(sha2_192f), 1},
    {"slhdsa-sha2-256s", &slh_dsa_sha2_256s, C3(sha2_256s), 0},
    {"slhdsa-sha2-256f", &slh_dsa_sha2_256f, C3(sha2_256f), 1},
    {"slhdsa-shake-128s", &slh_dsa_shake_128s, FAST(shake_128s), 0},
    {"slhdsa-shake-128f", &slh_dsa_shake_128f, FAST(shake_128f), 1},
    {"slhdsa-shake-192s", &slh_dsa_shake_192s, FAST(shake_192s), 0},
    {"slhdsa-shake-192f", &slh_dsa_shake_192f, FAST(shake_192f), 1},
    {"slhdsa-shake-256s", &slh_dsa_shake_256s, FAST(shake_256s), 0},
    {"slhdsa-shake-256f", &slh_dsa_shake_256f, FAST(shake_256f), 1},
};
#define NSETS (sizeof(sets) / sizeof(sets[0]))

#define STACK (12 * 1024)
#define MAX_SIG 49856

static const slh_param_t *P;
static const slh_kat_t *K;
static uint8_t pk[64], sk[128];
static uint8_t sig[MAX_SIG];
static size_t sig_sz;
static int tamper_ok;

static int same_hash(const uint8_t *data, size_t len, const uint8_t *expect)
{
    uint8_t h[32];
    sha2_256(h, data, len);
    return memcmp(h, expect, 32) == 0;
}

static int j_keygen(void *arg)
{
    size_t n = slh_pk_sz(P) / 2;
    (void)arg;
    if (slh_keygen_internal(sk, pk, K->kg_skseed, K->kg_skprf, K->kg_pkseed, P) != 0) return 1;
    if (memcmp(sk, K->kg_sk, 4 * n) != 0) return 2;
    if (memcmp(pk, K->kg_pk, 2 * n) != 0) return 3;
    return 0;
}

static int j_sign(void *arg)
{
    (void)arg;
    sig_sz = slh_sign(sig, K->sg_msg, K->sg_msg_len, K->sg_ctx, K->sg_ctx_len, K->sg_sk, NULL, P);
    return sig_sz == K->sig_len ? 0 : 1;
}

static int j_verify(void *arg)
{
    size_t n = slh_pk_sz(P) / 2;
    (void)arg;
    return slh_verify(K->sg_msg, K->sg_msg_len, sig, sig_sz, K->sg_ctx, K->sg_ctx_len,
                      K->sg_sk + 2 * n, P) == 1 ? 0 : 1;
}

/* A changed signature, message or context must be rejected. */
static int j_tamper(void *arg)
{
    size_t n = slh_pk_sz(P) / 2;
    const uint8_t *vk = K->sg_sk + 2 * n;
    static const size_t where[] = {0, 1, 2, 3}; /* R, FORS, middle, end of the hypertree */
    uint8_t m2[1] = {0x5a};
    (void)arg;

    tamper_ok = 1;
    for (size_t i = 0; i < sizeof(where) / sizeof(where[0]); i++) {
        size_t pos = where[i] == 0 ? 0 : where[i] == 1 ? n + 5 : where[i] == 2 ? sig_sz / 2 : sig_sz - 1;
        sig[pos] ^= 0x04;
        if (slh_verify(K->sg_msg, K->sg_msg_len, sig, sig_sz, K->sg_ctx, K->sg_ctx_len, vk, P) != 0) {
            tamper_ok = 0;
        }
        sig[pos] ^= 0x04;
    }
    if (slh_verify(m2, 1, sig, sig_sz, K->sg_ctx, K->sg_ctx_len, vk, P) != 0) tamper_ok = 0;
    if (slh_verify(K->sg_msg, K->sg_msg_len, sig, sig_sz, m2, 1, vk, P) != 0) tamper_ok = 0;
    if (slh_verify(K->sg_msg, K->sg_msg_len, sig, sig_sz - 1, K->sg_ctx, K->sg_ctx_len, vk, P) != 0) tamper_ok = 0;
    /* and the untouched signature still verifies */
    if (slh_verify(K->sg_msg, K->sg_msg_len, sig, sig_sz, K->sg_ctx, K->sg_ctx_len, vk, P) != 1) tamper_ok = 0;
    return 0;
}

/* Hedged signing through the internal interface, then verification. */
static int j_hedged(void *arg)
{
    size_t n = slh_pk_sz(P) / 2;
    (void)arg;
    sig_sz = slh_sign_internal(sig, K->hg_msg, K->hg_msg_len, K->hg_sk, K->hg_rnd, P);
    if (sig_sz != K->sig_len) return 1;
    return slh_verify_internal(K->hg_msg, K->hg_msg_len, sig, sig_sz, K->hg_sk + 2 * n, P) == 1 ? 0 : 2;
}

static const slh_ver_kat_t *V;
static int j_acvp_verify(void *arg)
{
    int r;
    (void)arg;
    if (V->internal) {
        r = slh_verify_internal(V->msg, V->msg_len, V->sig, V->sig_len, V->pk, P);
    } else {
        r = slh_verify(V->msg, V->msg_len, V->sig, V->sig_len, V->ctx, V->ctx_len, V->pk, P);
    }
    return r == V->expect ? 0 : 1;
}

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};
    char label[48];

#ifndef PQC_HOST
    slh_c3_init();
    slh_fast_shake_init();
#endif
    for (int v = 0; v < 2; v++) {
        const char *vn = variants[v];
        pqc_prims_select(v);
        for (size_t i = 0; i < NSETS; i++) {
            const set_t *s = &sets[i];
            size_t n;
            int rc;

            if (!bench_selected(bench_name(s->name, vn, ""))) {
                continue;
            }
            P = v ? s->opt : s->ref;
            K = &slh_kats[i];
            n = slh_pk_sz(P) / 2;
            bench_info(s->name, "pk=%u sk=%u sig=%u (%s)", (unsigned)slh_pk_sz(P),
                       (unsigned)slh_sk_sz(P), (unsigned)slh_sig_sz(P), K->alg);

            /* key generation = ACVP keyGen vector */
            rc = bench_run(bench_name(s->name, vn, "keygen"), j_keygen, NULL, s->fast ? 3 : 1, STACK);
            snprintf(label, sizeof(label), "acvp-keygen-tc%d", K->kg_tcid);
            bench_check(bench_name(s->name, vn, label), rc == 0);
            bench_digest(bench_name(s->name, vn, "pk"), pk, 2 * n);
            bench_digest(bench_name(s->name, vn, "sk"), sk, 4 * n);

            /* signing = ACVP sigGen vector (deterministic, external, pure) */
            sig_sz = 0;
            bench_run(bench_name(s->name, vn, "sign"), j_sign, NULL, s->fast ? 2 : 1, STACK);
            snprintf(label, sizeof(label), "acvp-siggen-tc%d", K->sg_tcid);
            bench_check(bench_name(s->name, vn, label),
                        sig_sz == K->sig_len && same_hash(sig, sig_sz, K->sg_sighash));
            bench_digest(bench_name(s->name, vn, "sig"), sig, sig_sz);

            /* verification of that signature, and rejection of changed ones */
            bench_run(bench_name(s->name, vn, "verify"), j_verify, NULL, 5, STACK);
            tamper_ok = 0;
            bench_run(bench_name(s->name, vn, "reject-x7"), j_tamper, NULL, 1, STACK);
            bench_check(bench_name(s->name, vn, "rejects-changed-signature"), tamper_ok);

            /* hedged signing through the internal interface (fast sets) */
            if (K->hg_sk != NULL) {
                sig_sz = 0;
                bench_run(bench_name(s->name, vn, "sign-hedged"), j_hedged, NULL, 1, STACK);
                snprintf(label, sizeof(label), "acvp-siggen-hedged-tc%d", K->hg_tcid);
                bench_check(bench_name(s->name, vn, label),
                            sig_sz == K->sig_len && same_hash(sig, sig_sz, K->hg_sighash));
            }

            /* ACVP sigVer vectors that are stored in full */
            for (size_t k = 0; k < sizeof(slh_ver_kats) / sizeof(slh_ver_kats[0]); k++) {
                V = &slh_ver_kats[k];
                if (strcmp(V->alg, K->alg) != 0) {
                    continue;
                }
                rc = bench_run(bench_name(s->name, vn, V->expect ? "verify-acvp-valid" : "verify-acvp-invalid"),
                               j_acvp_verify, NULL, 1, STACK);
                snprintf(label, sizeof(label), "acvp-sigver-tc%d-%s", V->tcid, V->expect ? "accept" : "reject");
                bench_check(bench_name(s->name, vn, label), rc == 0);
            }
        }
    }
    pqc_prims_select(PQC_PRIMS_REF);
#ifndef PQC_HOST
    slh_c3_wipe();
#endif
}
