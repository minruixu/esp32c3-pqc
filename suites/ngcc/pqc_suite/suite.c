/* Generic self-test and benchmark of one NGCC round-1 candidate.
 *
 * Every submission package implements the uniform interface that ICCS
 * prescribes for its known-answer-test programs (kem_*, sig_* or kex_*
 * functions plus the ICCS deterministic random number generator). This file
 * drives a candidate through that interface only, so the same code serves
 * all of them. Nothing in here comes from a submission package. */
#include <stdlib.h>
#include <string.h>
#include "bench.h"
#include "pqc_prims.h"
#ifndef PQC_HOST
#include "esp_heap_caps.h"
#endif

typedef unsigned long long ull;
typedef unsigned char uchar;

#ifndef NGCC_ALG
#define NGCC_ALG "ngcc"
#endif
#ifndef NGCC_ITERS
#define NGCC_ITERS 3
#endif
#ifndef NGCC_STACK
#define NGCC_STACK 0
#endif
#ifndef NGCC_STACK_PCT
#define NGCC_STACK_PCT 60
#endif
#ifndef NGCC_RB_LEN
#define NGCC_RB_LEN 64
#endif

/* ---------- randomness ---------- */

/* Context of the ICCS generator, as declared in the packages' drng.h. */
typedef struct {
    uchar V[55];
    uchar C[55];
    uchar reseed_counter[55];
} DRNG_ctx;

/* The ICCS generator itself, compiled from the package under these names. */
int iccs_init_random_number(DRNG_ctx *drng, const uchar *seed, ull seed_len_bytes);
int iccs_get_random_number(DRNG_ctx *drng, uchar *out, ull out_len_bits);

/* The generator instance the candidates draw their randomness from. */
#ifdef NGCC_CAND_HAS_DRNG_ALGORITHM
extern DRNG_ctx drng_algorithm;
#else
DRNG_ctx drng_algorithm;
#endif

/* 1: drng_algorithm is the real ICCS generator (known-answer test).
 * 0: drng_algorithm is the harness generator, as in every other suite. */
static int kat_mode;

/* What the candidate's code calls (its references are renamed to these). */
int ngcc_init_random_number(DRNG_ctx *drng, const uchar *seed, ull seed_len_bytes)
{
    return iccs_init_random_number(drng, seed, seed_len_bytes);
}

int ngcc_get_random_number(DRNG_ctx *drng, uchar *out, ull out_len_bits)
{
    size_t n;

    if (kat_mode || drng != &drng_algorithm) {
        return iccs_get_random_number(drng, out, out_len_bits);
    }
    n = (size_t)((out_len_bits + 7) / 8);
    randombytes(out, n);
    if (out_len_bits & 7) { /* the ICCS generator keeps the high bits of the last byte */
        out[n - 1] &= (uchar)(0xffu << (8 - (out_len_bits & 7)));
    }
    return 0;
}

/* Entry points for the candidate's own random-byte functions. Which one a
 * call is renamed to depends on the width of its length argument, because a
 * 64-bit length is passed differently from a 32-bit one on RV32. */
int ngcc_rb_ull(uchar *x, ull xlen)
{
    return ngcc_get_random_number(&drng_algorithm, x, xlen * 8);
}

int ngcc_rb_sz(uchar *x, size_t xlen)
{
    return ngcc_get_random_number(&drng_algorithm, x, (ull)xlen * 8);
}

#if NGCC_RB_LEN == 64
int ngcc_randombytes(uchar *x, ull xlen)
#else
int ngcc_randombytes(uchar *x, size_t xlen)
#endif
{
    return ngcc_get_random_number(&drng_algorithm, x, (ull)xlen * 8);
}

/* Seed of test vector 0, derived exactly as the ICCS KAT programs do. */
static void kat_seed_algorithm(void)
{
    DRNG_ctx ds;
    uchar nonce[64], seed[64];

    for (int i = 0; i < 16; i++) {
        memcpy(nonce + 4 * i, "seed", 4);
    }
    iccs_init_random_number(&ds, nonce, 64);
    iccs_get_random_number(&ds, seed, 64 * 8);
    iccs_init_random_number(&drng_algorithm, seed, 64);
}

/* ---------- helpers ---------- */

static const char *vn = "ref";
static uint32_t job_stack;

/* Buffers get some slack beyond the claimed length: a few candidates write a
 * signed message (signature followed by the message) or a length prefix. */
static uchar *alloc_buf(ull n)
{
    uchar *p = calloc((size_t)n + 256, 1);
    if (p == NULL) {
        bench_info("error", "cannot allocate a %u byte buffer", (unsigned)n);
    }
    return p;
}

static uint32_t choose_stack(void)
{
#if NGCC_STACK > 0
    return NGCC_STACK;
#elif defined(PQC_HOST)
    return 0;
#else
    /* Unknown code: give the job a share (60 % unless told otherwise) of the
     * largest free block and leave the rest for the candidate's own malloc
     * calls. */
    size_t big = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    size_t s = (big / 100 * NGCC_STACK_PCT) & ~(size_t)0xfff;
    return (uint32_t)(s < 16384 ? 16384 : s);
#endif
}

static void run(const char *op, bench_fn fn, int iters)
{
    bench_run(bench_name(NGCC_ALG, vn, op), fn, NULL, iters, job_stack);
}

/* Run a self-test job once and record whether it returned 0. (bench_name()
 * returns a static buffer, so the two names are built one after the other.) */
static void check_job(const char *check, const char *op, bench_fn fn)
{
    int rc = bench_run(bench_name(NGCC_ALG, vn, op), fn, NULL, 1, job_stack);
    bench_check(bench_name(NGCC_ALG, vn, check), rc == 0);
}

static void digest(const char *what, const uchar *p, ull n)
{
    bench_digest(bench_name(NGCC_ALG, vn, what), p, (size_t)n);
}

/* ================= key encapsulation ================= */
#if defined(NGCC_KIND_KEM)

ull kem_get_pk_len_bytes(void);
ull kem_get_sk_len_bytes(void);
ull kem_get_ss_len_bytes(void);
ull kem_get_ct_len_bytes(void);
int kem_keygen(uchar *pk, ull *pk_len, uchar *sk, ull *sk_len);
int kem_enc(uchar *pk, ull pk_len, uchar *ss, ull *ss_len, uchar *ct, ull *ct_len);
int kem_dec(uchar *sk, ull sk_len, uchar *ct, ull ct_len, uchar *ss, ull *ss_len);

static uchar *pk, *sk, *ct, *ss, *ss2;
static ull pk_cap, sk_cap, ct_cap, ss_cap;
static ull pkl, skl, ctl, ssl, ssl2;

static int job_keygen(void *arg)
{
    (void)arg;
    bench_rng_seed(1);
    pkl = pk_cap;
    skl = sk_cap;
    return kem_keygen(pk, &pkl, sk, &skl);
}

static int job_encaps(void *arg)
{
    (void)arg;
    bench_rng_seed(2);
    ssl = ss_cap;
    ctl = ct_cap;
    return kem_enc(pk, pkl, ss, &ssl, ct, &ctl);
}

static int job_decaps(void *arg)
{
    int rc;
    (void)arg;
    ssl2 = ss_cap;
    rc = kem_dec(sk, skl, ct, ctl, ss2, &ssl2);
    if (rc != 0) {
        return rc;
    }
    return (ssl2 != ssl || memcmp(ss, ss2, (size_t)ssl) != 0) ? 99 : 0;
}

/* Test vector 0 of the package's KAT file: same seed, same call order. */
static int job_kat(void *arg)
{
    int rc;
    (void)arg;
    kat_mode = 1;
    kat_seed_algorithm();
    rc = job_keygen(NULL);
    if (rc == 0) {
        rc = job_encaps(NULL);
    }
    if (rc == 0) {
        rc = job_decaps(NULL);
    }
    kat_mode = 0;
    return rc;
}

/* A corrupted ciphertext must not decapsulate to the same shared secret. */
static int job_tamper(void *arg)
{
    int rc, same;
    (void)arg;
    ct[ctl / 2] ^= 1;
    ssl2 = ss_cap;
    rc = kem_dec(sk, skl, ct, ctl, ss2, &ssl2);
    same = (rc == 0 && ssl2 == ssl && memcmp(ss, ss2, (size_t)ssl) == 0);
    ct[ctl / 2] ^= 1;
    return same;
}

static void suite_kind(void)
{
    pk_cap = kem_get_pk_len_bytes();
    sk_cap = kem_get_sk_len_bytes();
    ct_cap = kem_get_ct_len_bytes();
    ss_cap = kem_get_ss_len_bytes();
    bench_info(NGCC_ALG, "kind=KEM pk=%u sk=%u ct=%u ss=%u", (unsigned)pk_cap, (unsigned)sk_cap,
               (unsigned)ct_cap, (unsigned)ss_cap);
    if (!pk) pk = alloc_buf(pk_cap);
    if (!sk) sk = alloc_buf(sk_cap);
    if (!ct) ct = alloc_buf(ct_cap);
    if (!ss) ss = alloc_buf(ss_cap);
    if (!ss2) ss2 = alloc_buf(ss_cap);
    if (!pk || !sk || !ct || !ss || !ss2) {
        bench_check(bench_name(NGCC_ALG, vn, "buffers"), 0);
        return;
    }
    job_stack = choose_stack();
    bench_info("job_stack", "%u", (unsigned)job_stack);

    check_job("kat-roundtrip", "kat", job_kat);
    digest("kat0-pk", pk, pkl);
    digest("kat0-sk", sk, skl);
    digest("kat0-ct", ct, ctl);
    digest("kat0-ss", ss, ssl);

    run("keygen", job_keygen, NGCC_ITERS);
    run("encaps", job_encaps, NGCC_ITERS);
    run("decaps", job_decaps, NGCC_ITERS);
    digest("pk", pk, pkl);
    digest("sk", sk, skl);
    digest("ct", ct, ctl);
    digest("ss", ss, ssl);
    check_job("tampered-ct-rejected", "tamper", job_tamper);
}

/* ================= digital signature ================= */
#elif defined(NGCC_KIND_SIG)

ull sig_get_pk_len_bytes(void);
ull sig_get_sk_len_bytes(void);
ull sig_get_sn_len_bytes(void);
int sig_keygen(uchar *pk, ull *pk_len, uchar *sk, ull *sk_len);
int sig_sign(uchar *sk, ull sk_len, uchar *m, ull m_len, uchar *sn, ull *sn_len);
int sig_verify(uchar *pk, ull pk_len, uchar *sn, ull sn_len, uchar *m, ull m_len);

#define MSG_LEN 56 /* length of the message of test vector 0 */

static uchar *pk, *sk, *sn;
static uchar msg[128];
static ull pk_cap, sk_cap, sn_cap;
static ull pkl, skl, snl;

static int job_keygen(void *arg)
{
    (void)arg;
    bench_rng_seed(1);
    pkl = pk_cap;
    skl = sk_cap;
    return sig_keygen(pk, &pkl, sk, &skl);
}

static int job_sign(void *arg)
{
    (void)arg;
    bench_rng_seed(2);
    snl = sn_cap;
    return sig_sign(sk, skl, msg, MSG_LEN, sn, &snl);
}

static int job_verify(void *arg)
{
    (void)arg;
    return sig_verify(pk, pkl, sn, snl, msg, MSG_LEN);
}

static int job_kat(void *arg)
{
    DRNG_ctx dm;
    uchar nonce[64];
    int rc;
    (void)arg;

    for (int i = 0; i < 21; i++) {
        memcpy(nonce + 3 * i, "msg", 3);
    }
    nonce[63] = 'm';
    iccs_init_random_number(&dm, nonce, 64);
    iccs_get_random_number(&dm, msg, MSG_LEN * 8);

    kat_mode = 1;
    kat_seed_algorithm();
    rc = job_keygen(NULL);
    if (rc == 0) {
        rc = job_sign(NULL);
    }
    if (rc == 0) {
        rc = job_verify(NULL);
    }
    kat_mode = 0;
    return rc;
}

/* A changed message and a changed signature must both be rejected. */
static int job_tamper(void *arg)
{
    int bad_msg, bad_sig;
    (void)arg;
    msg[0] ^= 1;
    bad_msg = sig_verify(pk, pkl, sn, snl, msg, MSG_LEN);
    msg[0] ^= 1;
    sn[snl / 2] ^= 1;
    bad_sig = sig_verify(pk, pkl, sn, snl, msg, MSG_LEN);
    sn[snl / 2] ^= 1;
    return (bad_msg == 0 ? 1 : 0) + (bad_sig == 0 ? 2 : 0);
}

static void suite_kind(void)
{
    pk_cap = sig_get_pk_len_bytes();
    sk_cap = sig_get_sk_len_bytes();
    sn_cap = sig_get_sn_len_bytes();
    bench_info(NGCC_ALG, "kind=SIG pk=%u sk=%u sig=%u", (unsigned)pk_cap, (unsigned)sk_cap,
               (unsigned)sn_cap);
    if (!pk) pk = alloc_buf(pk_cap);
    if (!sk) sk = alloc_buf(sk_cap);
    if (!sn) sn = alloc_buf(sn_cap);
    if (!pk || !sk || !sn) {
        bench_check(bench_name(NGCC_ALG, vn, "buffers"), 0);
        return;
    }
    job_stack = choose_stack();
    bench_info("job_stack", "%u", (unsigned)job_stack);

    check_job("kat-roundtrip", "kat", job_kat);
    digest("kat0-pk", pk, pkl);
    digest("kat0-sk", sk, skl);
    digest("kat0-sig", sn, snl);
    bench_info("kat0_sig_len", "%u", (unsigned)snl);

    bench_rng_seed(7);
    randombytes(msg, MSG_LEN);
    run("keygen", job_keygen, NGCC_ITERS);
    run("sign", job_sign, NGCC_ITERS);
    run("verify", job_verify, NGCC_ITERS);
    digest("pk", pk, pkl);
    digest("sk", sk, skl);
    digest("sig", sn, snl);
    bench_info("sig_len", "%u", (unsigned)snl);
    check_job("tampered-rejected", "tamper", job_tamper);
}

/* ================= key exchange ================= */
#elif defined(NGCC_KIND_KEX)

ull kex_get_passes_num(void);
ull kex_get_pk_len_bytes(void);
ull kex_get_sk_len_bytes(void);
ull kex_get_sta_len_bytes(void);
ull kex_get_stb_len_bytes(void);
ull kex_get_ss_len_bytes(void);
ull kex_get_total_msg_len_bytes(void);
int kex_init_a(uchar *pka, ull *pka_len, uchar *ska, ull *ska_len, uchar *sta, ull *sta_len);
int kex_init_b(uchar *pkb, ull *pkb_len, uchar *skb, ull *skb_len, uchar *stb, ull *stb_len);
int kex_generate_pass1_msg_a(uchar *ska, ull ska_len, uchar *pkb, ull pkb_len, uchar *sta,
                             ull *sta_len, uchar *m1, ull *m1_len);
int kex_generate_pass2_msg_b(uchar *skb, ull skb_len, uchar *pka, ull pka_len, uchar *m1,
                             ull m1_len, uchar *stb, ull *stb_len, uchar *m2, ull *m2_len);
/* Only protocols with three passes define this one. */
int kex_generate_pass3_msg_a(uchar *ska, ull ska_len, uchar *pkb, ull pkb_len, uchar *m2,
                             ull m2_len, uchar *sta, ull *sta_len, uchar *m3, ull *m3_len)
    __attribute__((weak));
int kex_derive_ss_a(uchar *ska, ull ska_len, uchar *pkb, ull pkb_len, uchar *mb, ull mb_len,
                    uchar *sta, ull sta_len, uchar *ssa, ull *ssa_len);
int kex_derive_ss_b(uchar *skb, ull skb_len, uchar *pka, ull pka_len, uchar *ma, ull ma_len,
                    uchar *stb, ull stb_len, uchar *ssb, ull *ssb_len);

static uchar *pka, *ska, *pkb, *skb, *sta, *stb, *ssa, *ssb, *m1, *m2, *m3;
static ull pk_cap, sk_cap, sta_cap, stb_cap, ss_cap, msg_cap, passes;
static ull pkal, skal, pkbl, skbl, stal, stbl, ssal, ssbl, m1l, m2l, m3l;

static int job_init_a(void *arg)
{
    (void)arg;
    bench_rng_seed(1);
    pkal = pk_cap;
    skal = sk_cap;
    stal = sta_cap;
    return kex_init_a(pka, &pkal, ska, &skal, sta, &stal);
}

static int job_init_b(void *arg)
{
    (void)arg;
    bench_rng_seed(3);
    pkbl = pk_cap;
    skbl = sk_cap;
    stbl = stb_cap;
    return kex_init_b(pkb, &pkbl, skb, &skbl, stb, &stbl);
}

/* The message flow of the ICCS KAT program: a negative return value is an
 * error, 1 means "this was the last pass". */
static int exchange(void)
{
    uchar *ma = NULL, *mb = NULL;
    ull mal = 0, mbl = 0;
    int rc;

    m1l = m2l = m3l = 0;
    rc = kex_generate_pass1_msg_a(ska, skal, pkb, pkbl, sta, &stal, m1, &m1l);
    if (rc < 0) return rc;
    ma = m1;
    mal = m1l;
    if (rc != 1) {
        rc = kex_generate_pass2_msg_b(skb, skbl, pka, pkal, m1, m1l, stb, &stbl, m2, &m2l);
        if (rc < 0) return rc;
        mb = m2;
        mbl = m2l;
        if (rc != 1 && passes >= 3 && kex_generate_pass3_msg_a) {
            rc = kex_generate_pass3_msg_a(ska, skal, pkb, pkbl, m2, m2l, sta, &stal, m3, &m3l);
            if (rc < 0) return rc;
            ma = m3;
            mal = m3l;
        }
    }
    ssal = ssbl = ss_cap;
    rc = kex_derive_ss_a(ska, skal, pkb, pkbl, mb, mbl, sta, stal, ssa, &ssal);
    if (rc < 0) return rc;
    rc = kex_derive_ss_b(skb, skbl, pka, pkal, ma, mal, stb, stbl, ssb, &ssbl);
    if (rc < 0) return rc;
    return (ssal != ssbl || memcmp(ssa, ssb, (size_t)ssal) != 0) ? 99 : 0;
}

static int job_full(void *arg)
{
    int rc = job_init_a(arg);
    if (rc == 0) {
        rc = job_init_b(arg);
    }
    bench_rng_seed(2);
    return rc ? rc : exchange();
}

static int job_kat(void *arg)
{
    int rc;
    kat_mode = 1;
    kat_seed_algorithm();
    rc = job_full(arg);
    kat_mode = 0;
    return rc;
}

static void suite_kind(void)
{
    passes = kex_get_passes_num();
    pk_cap = kex_get_pk_len_bytes();
    sk_cap = kex_get_sk_len_bytes();
    sta_cap = kex_get_sta_len_bytes();
    stb_cap = kex_get_stb_len_bytes();
    ss_cap = kex_get_ss_len_bytes();
    msg_cap = kex_get_total_msg_len_bytes();
    bench_info(NGCC_ALG, "kind=KEX passes=%u pk=%u sk=%u sta=%u stb=%u ss=%u msgs=%u", (unsigned)passes,
               (unsigned)pk_cap, (unsigned)sk_cap, (unsigned)sta_cap, (unsigned)stb_cap, (unsigned)ss_cap,
               (unsigned)msg_cap);
    if (!pka) pka = alloc_buf(pk_cap);
    if (!ska) ska = alloc_buf(sk_cap);
    if (!pkb) pkb = alloc_buf(pk_cap);
    if (!skb) skb = alloc_buf(sk_cap);
    if (!sta) sta = alloc_buf(sta_cap);
    if (!stb) stb = alloc_buf(stb_cap);
    if (!ssa) ssa = alloc_buf(ss_cap);
    if (!ssb) ssb = alloc_buf(ss_cap);
    if (!m1) m1 = alloc_buf(msg_cap);
    if (!m2) m2 = alloc_buf(msg_cap);
    if (!m3) m3 = alloc_buf(passes >= 3 ? msg_cap : 0);
    if (!pka || !ska || !pkb || !skb || !sta || !stb || !ssa || !ssb || !m1 || !m2 || !m3) {
        bench_check(bench_name(NGCC_ALG, vn, "buffers"), 0);
        return;
    }
    job_stack = choose_stack();
    bench_info("job_stack", "%u", (unsigned)job_stack);

    check_job("kat-roundtrip", "kat", job_kat);
    digest("kat0-pka", pka, pkal);
    digest("kat0-pkb", pkb, pkbl);
    digest("kat0-m1", m1, m1l);
    digest("kat0-m2", m2, m2l);
    digest("kat0-ss", ssa, ssal);

    run("init_a", job_init_a, NGCC_ITERS);
    run("init_b", job_init_b, NGCC_ITERS);
    run("full", job_full, NGCC_ITERS);
    digest("pka", pka, pkal);
    digest("pkb", pkb, pkbl);
    digest("m1", m1, m1l);
    digest("m2", m2, m2l);
    digest("ss", ssa, ssal);
}

#else
#error "NGCC_KIND_KEM, NGCC_KIND_SIG or NGCC_KIND_KEX must be defined"
#endif

void suite_main(void)
{
    pqc_prims_select(PQC_PRIMS_REF);
    suite_kind();
#ifdef NGCC_HAS_OPT /* the ICCS hash helpers reach SM3 through components/prims */
    vn = "opt";
    pqc_prims_select(PQC_PRIMS_OPT);
    suite_kind();
#endif
}
