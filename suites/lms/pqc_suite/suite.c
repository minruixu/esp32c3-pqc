/* LMS and HSS: RFC known-answer tests and timings. */
#include <string.h>
#include "bench.h"
#include "lms.h"
#include "lms_kat.h"
#include "lms_kat_sm3.h"
#include "pqc_prims.h"

#define STACK (8 * 1024)
#define SIG_MAX 8800 /* LMS_SHA256_M32_H5 with W=1 is 8688 bytes */

typedef struct {
    const char *name;
    uint32_t lms, ots;
    int sign_iters; /* signatures timed after key generation */
    int heavy;      /* bit 0: ref, bit 1: opt. Key generation alone takes minutes for
                       that variant, so it only runs when asked for by name. */
} set_t;

/* Type codes: RFC 8554 section 5.1 and RFC 9858 section 3. */
static const set_t sets[] = {
    {"lms-sha256-m32-h5-w8", 0x05, 0x04, 32, 0},
    {"lms-sha256-m32-h5-w4", 0x05, 0x03, 32, 0},
    {"lms-sha256-m32-h5-w2", 0x05, 0x02, 32, 0},
    {"lms-sha256-m32-h5-w1", 0x05, 0x01, 32, 0},
    {"lms-sha256-m32-h10-w4", 0x06, 0x03, 70, 0},
    {"lms-sha256-m32-h10-w8", 0x06, 0x04, 40, 1},
    {"lms-sha256-m24-h5-w8", 0x0a, 0x08, 32, 0},
    {"lms-sha256-m24-h10-w4", 0x0b, 0x07, 70, 0},
    {"lms-shake-m32-h5-w4", 0x0f, 0x0b, 32, 0},
    {"lms-shake-m24-h5-w4", 0x14, 0x0f, 32, 0},
    {"lms-sm3-m32-h5-w8", 0x05, 0x0e, 32, 0}, /* SM3, GmSSL numbering */
    {"lms-sm3-m32-h10-w8", 0x06, 0x0e, 40, 3},
};

static lms_key_t key_a, key_b;
static uint8_t pk[64], sig[SIG_MAX], sig2[SIG_MAX];
static size_t siglen, sig2len;
static const uint8_t msg[] = "ESP32-C3 firmware image digest: 0123456789abcdef0123456789abcdef";
static const set_t *S;
static uint32_t stored_q; /* stands in for the counter kept in non-volatile storage */

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int persist_q(void *ctx, uint32_t next_q)
{
    (void)ctx;
    stored_q = next_q;
    return 0;
}

/* ---------- known-answer tests ---------- */
static const lms_kat_t *T;
static int kat_step;

/* (a) the RFC signature verifies, and changed inputs do not */
static int job_kat_verify(void *arg)
{
    (void)arg;
    if (hss_verify(T->pk, T->pklen, T->msg, T->msglen, T->sig, T->siglen) != 0) return 1;
    memcpy(sig, T->sig, T->siglen);
    sig[T->siglen / 2] ^= 0x01;
    if (hss_verify(T->pk, T->pklen, T->msg, T->msglen, sig, T->siglen) == 0) return 2;
    memcpy(sig, T->msg, T->msglen);
    sig[0] ^= 0x80;
    if (hss_verify(T->pk, T->pklen, sig, T->msglen, T->sig, T->siglen) == 0) return 3;
    if (hss_verify(T->pk, T->pklen, T->msg, T->msglen, T->sig, T->siglen - 1) == 0) return 4;
    return 0;
}

/* (b) the RFC private key gives the RFC public key, and signing with the
 * RFC's q and randomizer C gives the RFC signature byte for byte */
static int job_kat_sign(void *arg)
{
    lms_param_t P0, P1;
    const uint8_t *s0 = T->sig + 4, *m;
    uint32_t levels = be32(T->pk);
    size_t mlen, n;
    (void)arg;

    kat_step = 0;
    if (lms_param_init(&P0, be32(T->pk + 4), be32(T->pk + 8)) != 0) return 1;
    if (P0.h > LMS_SUB_H * LMS_MAX_LAYERS) return 0; /* tree too tall to rebuild here */
    /* top tree: public key, then the signature it made */
    if (lms_key_load(&key_a, P0.lms_type, P0.ots_type, T->seed[0], T->ident[0], be32(s0)) != 0) return 2;
    lms_public_key(&key_a, pk);
    if (memcmp(pk, T->pk + 4, lms_pk_bytes(&P0)) != 0) return 3;
    kat_step = 1;
    if (levels == 1) {
        m = T->msg;
        mlen = T->msglen;
    } else {
        m = s0 + lms_sig_bytes(&P0); /* pub[1], the message of sig[0] */
        if (lms_param_init(&P1, be32(m), be32(m + 4)) != 0) return 4;
        mlen = lms_pk_bytes(&P1);
    }
    if (lms_sign_c(&key_a, s0 + 8, m, mlen, sig, &n) != 0) return 5;
    if (n != lms_sig_bytes(&P0) || memcmp(sig, s0, n) != 0) return 6;
    kat_step = 2;
    if (levels == 2) {
        const uint8_t *s1 = m + mlen;

        if (lms_key_load(&key_b, P1.lms_type, P1.ots_type, T->seed[1], T->ident[1], be32(s1)) != 0) return 7;
        lms_public_key(&key_b, pk);
        if (memcmp(pk, m, mlen) != 0) return 8;
        if (lms_sign_c(&key_b, s1 + 8, T->msg, T->msglen, sig, &n) != 0) return 9;
        if (n != lms_sig_bytes(&P1) || memcmp(sig, s1, n) != 0) return 10;
        kat_step = 3;
    }
    return 0;
}

/* GmSSL's HSS signature with SM3 verifies, and changed inputs do not */
static int job_kat_sm3(void *arg)
{
    (void)arg;
    if (hss_verify(gmssl_sm3_hss_pk, sizeof(gmssl_sm3_hss_pk), gmssl_sm3_hss_msg, sizeof(gmssl_sm3_hss_msg),
                   gmssl_sm3_hss_sig, sizeof(gmssl_sm3_hss_sig)) != 0) return 1;
    memcpy(sig, gmssl_sm3_hss_sig, sizeof(gmssl_sm3_hss_sig));
    sig[sizeof(gmssl_sm3_hss_sig) / 2] ^= 0x01;
    if (hss_verify(gmssl_sm3_hss_pk, sizeof(gmssl_sm3_hss_pk), gmssl_sm3_hss_msg, sizeof(gmssl_sm3_hss_msg),
                   sig, sizeof(gmssl_sm3_hss_sig)) == 0) return 2;
    return 0;
}

/* ---------- timed operations ---------- */
static const uint8_t bench_seed[32] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
                                       0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
                                       0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f};
static const uint8_t bench_i[16] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                                    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};

static int job_keygen(void *arg)
{
    (void)arg;
    if (lms_key_load(&key_a, S->lms, S->ots, bench_seed, bench_i, 0) != 0) return 1;
    key_a.persist = persist_q;
    lms_public_key(&key_a, pk);
    return 0;
}

static int job_sign(void *arg)
{
    (void)arg;
    return lms_sign(&key_a, msg, sizeof(msg) - 1, sig, &siglen);
}

static int job_verify(void *arg)
{
    (void)arg;
    return lms_verify(pk, lms_pk_bytes(&key_a.P), msg, sizeof(msg) - 1, sig, siglen);
}

/* Every signature of a key verifies; a key reloaded from (seed, I, q) signs
 * the same bytes as the key that got there by signing; a used-up key refuses. */
static int job_selftest(void *arg)
{
    uint32_t total, i;

    if (arg == NULL) { /* start from a new key; otherwise continue with the one in use */
        if (lms_key_load(&key_a, S->lms, S->ots, bench_seed, bench_i, 0) != 0) return 1;
        key_a.persist = persist_q;
        lms_public_key(&key_a, pk);
    }
    total = key_a.q + lms_remaining(&key_a);
    if (total > 100) total = 100;
    for (i = key_a.q; i < total; i++) {
        if (lms_sign(&key_a, msg, sizeof(msg) - 1, sig, &siglen) != 0) return 2;
        if (stored_q != i + 1) return 3;
        if (lms_verify(pk, lms_pk_bytes(&key_a.P), msg, sizeof(msg) - 1, sig, siglen) != 0) return 4;
        sig[siglen - 1] ^= 1;
        if (lms_verify(pk, lms_pk_bytes(&key_a.P), msg, sizeof(msg) - 1, sig, siglen) == 0) return 5;
        sig[siglen - 1] ^= 1;
    }
    if (lms_key_load(&key_b, S->lms, S->ots, bench_seed, bench_i, total - 1) != 0) return 6;
    if (lms_sign(&key_b, msg, sizeof(msg) - 1, sig2, &sig2len) != 0) return 7;
    if (sig2len != siglen || memcmp(sig, sig2, siglen) != 0) return 8;
    if (lms_remaining(&key_a) == 0 && lms_sign(&key_a, msg, sizeof(msg) - 1, sig2, &sig2len) != -1) return 9;
    return 0;
}

/* ---------- HSS ---------- */
static hss_key_t hss;
static uint8_t hss_spk[1400];          /* signature of H5/W8 plus a public key */
static uint8_t hss_pk[64], hss_sig[2700];
static size_t hss_siglen;

static int job_hss_keygen(void *arg)
{
    (void)arg;
    hss.levels = 2;
    hss.lms_type[0] = hss.lms_type[1] = 0x05; /* LMS_SHA256_M32_H5 */
    hss.ots_type[0] = hss.ots_type[1] = 0x04; /* LMOTS_SHA256_N32_W8 */
    hss.lv[0] = &key_a;
    hss.lv[1] = &key_b;
    hss.spk[0] = hss_spk;
    if (hss_keygen(&hss, bench_seed, bench_i) != 0) return 1;
    hss_public_key(&hss, hss_pk);
    return 0;
}

static int job_hss_sign(void *arg)
{
    (void)arg;
    return hss_sign(&hss, msg, sizeof(msg) - 1, hss_sig, &hss_siglen);
}

static int job_hss_verify(void *arg)
{
    (void)arg;
    return hss_verify(hss_pk, hss_pk_bytes(&hss), msg, sizeof(msg) - 1, hss_sig, hss_siglen);
}

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};

    for (int v = 0; v < 2; v++) {
        const char *vn = variants[v];

        pqc_prims_select(v);
        lms_fast = v;
#if !defined(PQC_HOST) && !defined(LMS_HW_CHAIN_OFF)
        lms_chain_accel = v ? lms_chain_sha256_hw : NULL;
#endif

        /* known-answer tests */
        for (size_t i = 0; i < sizeof(lms_kats) / sizeof(lms_kats[0]); i++) {
            char alg[40];

            T = &lms_kats[i];
            strcpy(alg, "kat-");
            strcat(alg, T->name);
            if (!bench_selected(bench_name(alg, vn, ""))) {
                continue;
            }
            bench_check(bench_name(alg, vn, "verify"),
                        bench_run(bench_name(alg, vn, "verify"), job_kat_verify, NULL, 1, STACK) == 0);
            if (T->seed[0] && v == 0 && be32(T->pk + 4) >= 0x0f) {
                /* SHAKE trees with W=8: minutes with the reference Keccak */
                bench_info(bench_name(alg, vn, "keygen+sign"), "only run in the opt variant");
            } else if (T->seed[0]) {
                int rc = bench_run(bench_name(alg, vn, "keygen+sign"), job_kat_sign, NULL, 1, STACK);
                bench_check(bench_name(alg, vn, "keygen+sign"), rc == 0);
                bench_info(bench_name(alg, vn, "steps"), "%d", kat_step);
            }
        }

        if (bench_selected(bench_name("kat-gmssl_sm3_hss", vn, ""))) {
            bench_check(bench_name("kat-gmssl_sm3_hss", vn, "verify"),
                        bench_run(bench_name("kat-gmssl_sm3_hss", vn, "verify"), job_kat_sm3, NULL, 1, STACK) == 0);
        }

        /* timings */
        for (size_t i = 0; i < sizeof(sets) / sizeof(sets[0]); i++) {
            S = &sets[i];
            if (!bench_selected(bench_name(S->name, vn, ""))) {
                continue;
            }
            if ((S->heavy & (1 << v)) && bench_selected(bench_name("x", vn, "x"))) {
                /* only on request: tools/pqc run lms -f <name>/<variant> */
                bench_info(bench_name(S->name, vn, "skipped"), "run with -f %s/%s", S->name, vn);
                continue;
            }
            bench_run(bench_name(S->name, vn, "keygen"), job_keygen, NULL, 1, STACK);
            bench_info(S->name, "pk=%u sig=%u signatures=%u n=%u w=%u p=%u",
                       (unsigned)lms_pk_bytes(&key_a.P), (unsigned)lms_sig_bytes(&key_a.P),
                       1u << key_a.P.h, key_a.P.n, key_a.P.w, key_a.P.p);
            bench_digest(bench_name(S->name, vn, "pk"), pk, lms_pk_bytes(&key_a.P));
            bench_run(bench_name(S->name, vn, "sign"), job_sign, NULL, S->sign_iters, STACK);
            bench_run(bench_name(S->name, vn, "verify"), job_verify, NULL, 10, STACK);
            bench_digest(bench_name(S->name, vn, "sig"), sig, siglen);
            if (S->lms == 0x05 && S->ots == 0x03) {
                bench_check(bench_name(S->name, vn, "selftest"),
                            bench_run(bench_name(S->name, vn, "selftest"), job_selftest, NULL, 1, STACK) == 0);
            }
            if (S->lms == 0x06 && S->ots == 0x03 && v == 1) {
                /* two layers of subtrees: goes on to signature 100, across three subtrees */
                bench_check(bench_name(S->name, vn, "selftest"),
                            bench_run(bench_name(S->name, vn, "selftest"), job_selftest, (void *)1, 1, STACK) == 0);
                bench_digest(bench_name(S->name, vn, "sig100"), sig, siglen);
            }
        }

        /* HSS, two levels of H5/W8: 1024 signatures */
        if (bench_selected(bench_name("hss-sha256-h5w8-h5w8", vn, ""))) {
            const char *a = "hss-sha256-h5w8-h5w8";

            bench_run(bench_name(a, vn, "keygen"), job_hss_keygen, NULL, 1, STACK);
            bench_info(a, "pk=%u sig=%u signatures=1024", (unsigned)hss_pk_bytes(&hss),
                       (unsigned)hss_sig_bytes(&hss));
            bench_digest(bench_name(a, vn, "pk"), hss_pk, hss_pk_bytes(&hss));
            /* 40 signatures: the lower tree is used up and replaced after 32 */
            bench_run(bench_name(a, vn, "sign"), job_hss_sign, NULL, 40, STACK);
            bench_check(bench_name(a, vn, "verify"),
                        bench_run(bench_name(a, vn, "verify"), job_hss_verify, NULL, 5, STACK) == 0);
            bench_digest(bench_name(a, vn, "sig"), hss_sig, hss_siglen);
        }
    }
}
