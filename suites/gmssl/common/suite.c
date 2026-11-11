/* GmSSL's post-quantum algorithms built on SM3: LMS/HSS, XMSS/XMSS^MT and
 * SPHINCS+, plus GmSSL's Kyber-768. GmSSL's own test programs serve as
 * self-tests; the timings use the public GmSSL API. */
#include <string.h>
#ifdef GMSSL_WITH_KYBER
#include "gmssl_rename.h"
#include <gmssl/kyber.h>
#endif
#ifdef GMSSL_WITH_LMS
#include <gmssl/lms.h>
#endif
#ifdef GMSSL_WITH_SPHINCS
#include <gmssl/sphincs.h>
#endif
#ifdef GMSSL_WITH_XMSS
#include <gmssl/xmss.h>
#endif
#include "bench.h"
#include "gmssl_port.h"
#include "pqc_prims.h"

/* GmSSL keeps whole keys, signatures and contexts in local variables: an
 * HSS key is 8 KB, an HSS signature 10 KB, an XMSS^MT signature 28 KB. */
#define STACK_SMALL (32 * 1024)
#define STACK_LMS (72 * 1024)
#define STACK_XMSS (56 * 1024)
#define STACK_KYBER (48 * 1024)
/* GmSSL's SPHINCS+ keeps a FORS tree (131 KB) or an XMSS tree (16 KB) in a
 * local variable. Those jobs run on a static stack (bigstack.c); the stack
 * the harness allocates only carries the call. */
#define STACK_SPHINCS (6 * 1024)
#define STACK_SPHINCS_TEST (16 * 1024)

int gmssl_lmstest_main(void);
int gmssl_xmsstest_main(void);
int gmssl_sphincstest_main(void);
int gmssl_kybertest_main(void);
int gmssl_lmstest_quick(void);
int gmssl_xmsstest_quick(void);
int gmssl_sphincstest_quick(void);
int gmssl_kybertest_quick(void);

static const uint8_t msg[] = "ESP32-C3 firmware image digest: 0123456789abcdef0123456789abcdef";
#define MLEN (sizeof(msg) - 1)

static uint8_t sigbuf[10 * 1024], pkbuf[1600];
static size_t siglen, pklen;

static int job_test(void *arg)
{
    int (*fn)(void) = (int (*)(void))arg;
    return fn();
}

#ifdef GMSSL_WITH_LMS
/* ---------- LMS and HSS with SM3 ---------- */
static LMS_KEY lms_key;
static LMS_SIGN_CTX lms_ctx;
static HSS_KEY hss_key;
static HSS_SIGN_CTX hss_ctx;
static int lms_type;

static const uint8_t fixed_seed[32] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
                                       0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
                                       0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f};
static const uint8_t fixed_i[16] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                                    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};

static int job_lms_keygen(void *arg)
{
    uint8_t *p = pkbuf;
    (void)arg;

    lms_key_cleanup(&lms_key);
    if (lms_key_generate_ex(&lms_key, lms_type, fixed_seed, fixed_i, 1) != 1) return 1;
    pklen = 0;
    if (lms_public_key_to_bytes(&lms_key, &p, &pklen) != 1) return 2;
    return 0;
}

static int job_lms_sign(void *arg)
{
    (void)arg;
    if (lms_sign_init(&lms_ctx, &lms_key) != 1) return 1;
    if (lms_sign_update(&lms_ctx, msg, MLEN) != 1) return 2;
    if (lms_sign_finish(&lms_ctx, sigbuf, &siglen) != 1) return 3;
    return 0;
}

static int job_lms_verify(void *arg)
{
    (void)arg;
    if (lms_verify_init(&lms_ctx, &lms_key, sigbuf, siglen) != 1) return 1;
    if (lms_verify_update(&lms_ctx, msg, MLEN) != 1) return 2;
    if (lms_verify_finish(&lms_ctx) != 1) return 3;
    return 0;
}

static int job_hss_keygen(void *arg)
{
    static const int types[2] = {LMS_SM3_M32_H5, LMS_SM3_M32_H5};
    uint8_t *p = pkbuf;
    (void)arg;

    hss_key_cleanup(&hss_key);
    if (hss_key_generate(&hss_key, types, 2) != 1) return 1;
    pklen = 0;
    if (hss_public_key_to_bytes(&hss_key, &p, &pklen) != 1) return 2;
    return 0;
}

static int job_hss_sign(void *arg)
{
    (void)arg;
    if (hss_sign_init(&hss_ctx, &hss_key) != 1) return 1;
    if (hss_sign_update(&hss_ctx, msg, MLEN) != 1) return 2;
    if (hss_sign_finish(&hss_ctx, sigbuf, &siglen) != 1) return 3;
    return 0;
}

static int job_hss_verify(void *arg)
{
    (void)arg;
    if (hss_verify_init(&hss_ctx, &hss_key, sigbuf, siglen) != 1) return 1;
    if (hss_verify_update(&hss_ctx, msg, MLEN) != 1) return 2;
    if (hss_verify_finish(&hss_ctx) != 1) return 3;
    return 0;
}

#endif /* GMSSL_WITH_LMS */

#ifdef GMSSL_WITH_XMSS
/* ---------- XMSS and XMSS^MT with SM3 ---------- */
static XMSS_KEY xmss_key;
static XMSS_SIGN_CTX xmss_ctx;
static XMSSMT_KEY xmssmt_key;
static XMSSMT_SIGN_CTX xmssmt_ctx;

static int job_xmss_keygen(void *arg)
{
    uint8_t *p = pkbuf;
    (void)arg;

    xmss_key_cleanup(&xmss_key);
    if (xmss_key_generate(&xmss_key, XMSS_SM3_10_256) != 1) return 1;
    pklen = 0;
    if (xmss_public_key_to_bytes(&xmss_key, &p, &pklen) != 1) return 2;
    return 0;
}

static int job_xmss_sign(void *arg)
{
    (void)arg;
    if (xmss_sign_init(&xmss_ctx, &xmss_key) != 1) return 1;
    if (xmss_sign_update(&xmss_ctx, msg, MLEN) != 1) return 2;
    if (xmss_sign_finish(&xmss_ctx, sigbuf, &siglen) != 1) return 3;
    return 0;
}

static int job_xmss_verify(void *arg)
{
    (void)arg;
    if (xmss_verify_init(&xmss_ctx, &xmss_key, sigbuf, siglen) != 1) return 1;
    if (xmss_verify_update(&xmss_ctx, msg, MLEN) != 1) return 2;
    if (xmss_verify_finish(&xmss_ctx) != 1) return 3;
    return 0;
}

static int job_xmssmt_keygen(void *arg)
{
    uint8_t *p = pkbuf;
    (void)arg;

    xmssmt_key_cleanup(&xmssmt_key);
    if (xmssmt_key_generate(&xmssmt_key, XMSSMT_SM3_20_4_256) != 1) return 1;
    pklen = 0;
    if (xmssmt_public_key_to_bytes(&xmssmt_key, &p, &pklen) != 1) return 2;
    return 0;
}

static int job_xmssmt_sign(void *arg)
{
    (void)arg;
    if (xmssmt_sign_init(&xmssmt_ctx, &xmssmt_key) != 1) return 1;
    if (xmssmt_sign_update(&xmssmt_ctx, msg, MLEN) != 1) return 2;
    if (xmssmt_sign_finish(&xmssmt_ctx, sigbuf, &siglen) != 1) return 3;
    return 0;
}

static int job_xmssmt_verify(void *arg)
{
    (void)arg;
    if (xmssmt_verify_init(&xmssmt_ctx, &xmssmt_key, sigbuf, siglen) != 1) return 1;
    if (xmssmt_verify_update(&xmssmt_ctx, msg, MLEN) != 1) return 2;
    if (xmssmt_verify_finish(&xmssmt_ctx) != 1) return 3;
    return 0;
}

#endif /* GMSSL_WITH_XMSS */

#ifdef GMSSL_WITH_SPHINCS
/* ---------- SPHINCS+ with SM3 (128s parameters) ---------- */
static SPHINCS_KEY sphincs_key;
static SPHINCS_SIGN_CTX sphincs_ctx;

static unsigned sphincs_stack_used;

#ifdef PQC_HOST
#define ON_BIG_STACK(fn) fn(NULL)
#else
#define ON_BIG_STACK(fn) gmssl_run_bigstack(fn, NULL, &sphincs_stack_used)
#endif

static int do_sphincs_keygen(void *arg)
{
    uint8_t *p = pkbuf;
    (void)arg;

    if (sphincs_key_generate(&sphincs_key) != 1) return 1;
    pklen = 0;
    if (sphincs_public_key_to_bytes(&sphincs_key, &p, &pklen) != 1) return 2;
    return 0;
}

static int do_sphincs_sign(void *arg)
{
    (void)arg;
    if (sphincs_sign_init(&sphincs_ctx, &sphincs_key) != 1) return 1;
    if (sphincs_sign_prepare(&sphincs_ctx, msg, MLEN) != 1) return 2;
    if (sphincs_sign_update(&sphincs_ctx, msg, MLEN) != 1) return 3;
    if (sphincs_sign_finish(&sphincs_ctx, sigbuf, &siglen) != 1) return 4;
    return 0;
}

static int do_sphincs_verify(void *arg)
{
    (void)arg;
    if (sphincs_verify_init(&sphincs_ctx, &sphincs_key, sigbuf, siglen) != 1) return 1;
    if (sphincs_verify_update(&sphincs_ctx, msg, MLEN) != 1) return 2;
    if (sphincs_verify_finish(&sphincs_ctx) != 1) return 3;
    return 0;
}

static int job_sphincs_keygen(void *arg)
{
    (void)arg;
    return ON_BIG_STACK(do_sphincs_keygen);
}

static int job_sphincs_sign(void *arg)
{
    (void)arg;
    return ON_BIG_STACK(do_sphincs_sign);
}

static int job_sphincs_verify(void *arg)
{
    (void)arg;
    return ON_BIG_STACK(do_sphincs_verify);
}
#endif /* GMSSL_WITH_SPHINCS */

#ifdef GMSSL_WITH_KYBER
/* ---------- Kyber-768 as implemented in GmSSL ---------- */
static KYBER_KEY kyber_key;
static KYBER_CIPHERTEXT kyber_ct;
static uint8_t kyber_k1[32], kyber_k2[32];

static int job_kyber_keygen(void *arg)
{
    uint8_t rnd[32];
    (void)arg;
    randombytes(rnd, sizeof(rnd));
    return kyber_key_generate_ex(&kyber_key, rnd) != 1;
}

static int job_kyber_encap(void *arg)
{
    (void)arg;
    return kyber_encap(&kyber_key, &kyber_ct, kyber_k1) != 1;
}

static int job_kyber_decap(void *arg)
{
    (void)arg;
    if (kyber_decap(&kyber_key, &kyber_ct, kyber_k2) != 1) return 1;
    return memcmp(kyber_k1, kyber_k2, 32) != 0;
}

#endif /* GMSSL_WITH_KYBER */

/* ---------- driver ---------- */
static bench_fn reject_target;

/* a signature with one bit changed must not verify */
static int job_reject(void *arg)
{
    int rc;
    (void)arg;

    sigbuf[siglen / 2] ^= 1;
    rc = reject_target(NULL);
    sigbuf[siglen / 2] ^= 1;
    return rc == 0;
}

static void run_sig(const char *alg, const char *vn, bench_fn kg, bench_fn sg, bench_fn vf, int sign_iters,
                    uint32_t stack)
{
    bench_rng_seed(7);
    bench_run(bench_name(alg, vn, "keygen"), kg, NULL, 1, stack);
    bench_digest(bench_name(alg, vn, "pk"), pkbuf, pklen);
    bench_run(bench_name(alg, vn, "sign"), sg, NULL, sign_iters, stack);
    bench_check(bench_name(alg, vn, "verify"), bench_run(bench_name(alg, vn, "verify"), vf, NULL, 3, stack) == 0);
    bench_digest(bench_name(alg, vn, "sig"), sigbuf, siglen);
    reject_target = vf;
    bench_check(bench_name(alg, vn, "reject"),
                bench_run(bench_name(alg, vn, "reject"), job_reject, NULL, 1, stack) == 0);
    bench_info(alg, "pk=%u sig=%u", (unsigned)pklen, (unsigned)siglen);
}

/* 1 if the long job `alg` of variant vn should be skipped in this run:
 * it only runs when the filter names it, not on a run of everything. */
static int skip_heavy(const char *alg, const char *vn)
{
    if (bench_selected(bench_name("x", vn, "x"))) {
        bench_info(bench_name(alg, vn, "skipped"), "run with -f %s/%s", alg, vn);
        return 1;
    }
    return 0;
}

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};
    static const struct {
        const char *name;
        int (*quick)(void);
        int (*full)(void);
        uint32_t stack;
    } tests[] = {
#ifdef GMSSL_WITH_LMS
        {"gmssl-lmstest", gmssl_lmstest_quick, gmssl_lmstest_main, STACK_LMS},
#endif
#ifdef GMSSL_WITH_XMSS
        {"gmssl-xmsstest", gmssl_xmsstest_quick, gmssl_xmsstest_main, STACK_XMSS},
#endif
#ifdef GMSSL_WITH_SPHINCS
        {"gmssl-sphincstest", gmssl_sphincstest_quick, gmssl_sphincstest_main, STACK_SPHINCS_TEST},
#endif
#ifdef GMSSL_WITH_KYBER
        {"gmssl-kybertest", gmssl_kybertest_quick, gmssl_kybertest_main, STACK_KYBER},
#endif
    };

    for (int v = 0; v < 2; v++) {
        const char *vn = variants[v];
        const char *a;
        (void)a;

        pqc_prims_select(v);
        gmssl_fast = v;
        if (v) {
            gmssl_port_calibrate();
        }

        /* GmSSL's own tests */
        for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
            if (!bench_selected(bench_name(tests[i].name, vn, ""))) {
                continue;
            }
            bench_rng_seed(3);
            bench_check(bench_name(tests[i].name, vn, "quick"),
                        bench_run(bench_name(tests[i].name, vn, "quick"), job_test, (void *)tests[i].quick, 1,
                                  tests[i].stack) == 0);
#ifdef PQC_HOST
            bench_rng_seed(4);
            bench_check(bench_name(tests[i].name, vn, "hostfull"),
                        bench_run(bench_name(tests[i].name, vn, "hostfull"), job_test, (void *)tests[i].full, 1,
                                  tests[i].stack) == 0);
#endif
        }

#ifdef GMSSL_WITH_KYBER
        a = "gmssl-kyber768";
        if (bench_selected(bench_name(a, vn, ""))) {
            bench_rng_seed(5);
            bench_run(bench_name(a, vn, "keygen"), job_kyber_keygen, NULL, 10, STACK_KYBER);
            bench_run(bench_name(a, vn, "encaps"), job_kyber_encap, NULL, 10, STACK_KYBER);
            bench_check(bench_name(a, vn, "decaps"),
                        bench_run(bench_name(a, vn, "decaps"), job_kyber_decap, NULL, 10, STACK_KYBER) == 0);
            {
                uint8_t *p = sigbuf;
                size_t len = 0;

                kyber_public_key_to_bytes(&kyber_key, &p, &len);
                kyber_private_key_to_bytes(&kyber_key, &p, &len);
                kyber_ciphertext_to_bytes(&kyber_ct, &p, &len);
                bench_digest(bench_name(a, vn, "pk+sk+ct"), sigbuf, len);
                bench_info(a, "pk+sk+ct=%u bytes", (unsigned)len);
            }
            bench_digest(bench_name(a, vn, "ss"), kyber_k1, 32);
        }
#endif

#ifdef GMSSL_WITH_LMS
        a = "gmssl-lms-sm3-h5";
        if (bench_selected(bench_name(a, vn, ""))) {
            lms_type = LMS_SM3_M32_H5;
            run_sig(a, vn, job_lms_keygen, job_lms_sign, job_lms_verify, 16, STACK_LMS);
        }
        a = "gmssl-hss-sm3-h5-h5";
        if (bench_selected(bench_name(a, vn, ""))) {
            run_sig(a, vn, job_hss_keygen, job_hss_sign, job_hss_verify, 16, STACK_LMS);
        }
        a = "gmssl-lms-sm3-h10";
        if (bench_selected(bench_name(a, vn, "")) && !skip_heavy(a, vn)) {
            lms_type = LMS_SM3_M32_H10;
            run_sig(a, vn, job_lms_keygen, job_lms_sign, job_lms_verify, 8, STACK_LMS);
        }
#endif
#ifdef GMSSL_WITH_XMSS
        a = "gmssl-xmssmt-sm3-20_4";
        if (bench_selected(bench_name(a, vn, ""))) {
            run_sig(a, vn, job_xmssmt_keygen, job_xmssmt_sign, job_xmssmt_verify, 8, STACK_XMSS);
        }
        a = "gmssl-xmss-sm3-10";
        if (bench_selected(bench_name(a, vn, "")) && !skip_heavy(a, vn)) {
            run_sig(a, vn, job_xmss_keygen, job_xmss_sign, job_xmss_verify, 4, STACK_XMSS);
        }
#endif
#ifdef GMSSL_WITH_SPHINCS
        a = "gmssl-sphincs-sm3-128s";
        if (bench_selected(bench_name(a, vn, "")) && !skip_heavy(a, vn)) {
            sphincs_stack_used = 0;
#ifdef PQC_HOST
            run_sig(a, vn, job_sphincs_keygen, job_sphincs_sign, job_sphincs_verify, 1, STACK_SPHINCS);
#else
            /* Key generation only. Signing was tried on the 138 KB static
             * stack and the board stopped answering (see NOTES.md); it is not
             * started here so that a run cannot hang. */
            bench_rng_seed(7);
            bench_run(bench_name(a, vn, "keygen"), job_sphincs_keygen, NULL, 1, STACK_SPHINCS);
            bench_digest(bench_name(a, vn, "pk"), pkbuf, pklen);
            bench_info(bench_name(a, vn, "sign"), "not run on the device: needs more stack than the chip has");
#endif
            bench_info(bench_name(a, vn, "stack"), "%u bytes on the static stack", sphincs_stack_used);
        }
#endif
    }
}
