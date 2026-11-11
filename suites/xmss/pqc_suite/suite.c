/* XMSS and XMSS^MT (RFC 8391, NIST SP 800-208): the reference
 * implementation with BDS tree traversal, its published test vectors, and
 * timings. */
#include <string.h>
#include "bench.h"
#include "fips202.h"
#include "params.h"
#include "pqc_prims.h"
#include "utils.h"
#include "wots.h"
#include "xmss_commons.h"
#include "xmss_core.h"
#include "xmss_port.h"

#define STACK (20 * 1024)
#define SK_MAX (24 * 1024)
#define SM_MAX (10 * 1024)
#define MLEN 64

/* ---------- random bytes for key generation ---------- */
const uint8_t *xmss_port_seed;

int xmss_port_randombytes(uint8_t *out, size_t outlen)
{
    if (xmss_port_seed) {
        memcpy(out, xmss_port_seed, outlen);
        xmss_port_seed += outlen;
        return 0;
    }
    return randombytes(out, outlen);
}

/* ---------- test vectors ----------
 * Output of test/vectors.c of the reference implementation: the first 10
 * bytes of SHAKE128 over the value. The table is the one the Go package
 * github.com/bwesterb/go-xmssmt checks itself against. */
typedef struct {
    uint32_t oid;
    const char *pk, *sig, *leaf;
} vec_t;

static const vec_t vec_wots[] = {
    {1, "a5df5a7785a48961552e", "4443fb313e5b0c2e8bec", "fc27066a9b31c0069597"},
    {4, "b60e1297f5c9b328c5e8", "3ae3de6598456112261d", "1ae375ab3af144099b3d"},
    {7, "654c7f6754b55312197f", "a51bd20ef66e93d79464", "70dac71617da61947011"},
    {10, "e7462d29751df96bf5a4", "ffb59bc9bf87e4e4b7f0", "0cf456d0f4b02b341e12"},
    {13, "adbec5b9ba94bff3447d", "b32683d5888df51aa074", "58eb225e44f38082b356"},
    {16, "e008635b776020636868", "05d9a9d517021307b1a7", "2ed530d278acdf27e01d"},
    {19, "8f041a7c67b46fc80b0d", "98a906af2d18429309f6", "34457720369d5f7691e9"},
};
static const vec_t vec_xmss[] = {
    {1, "7de72d192121f414d4bb", "8b6cb278d50a3694ca38", 0},
    {4, "74ee7c42b4e42a424ed9", "b9e63b0376a550eabe1b", 0},
    {7, "764614ee2ce5e4bf0114", "3e9035cffa0fd4be98bd", 0},
    {10, "e47fe831b6ee463e2881", "ce2dc09cd7ad8c87ae06", 0},
    {13, "5933d4b1e696804718c7", "6ec9da2e05da544d9c5d", 0},
    {16, "cef3d38791d56efee1b3", "9939a0f87502df5d1e31", 0},
    {19, "7fa280e502275858b27b", "7782c54424c9ca082926", 0},
};
static const vec_t vec_xmssmt[] = {
    {2, "9df4c75282451bf2bc53", "fd4ff4c18801147b2804", 0},
    {10, "fdeb0cc4fed643bf70ce", "fbeb33a7aed7af7ea526", 0},
    {18, "dbe6fc388fbd610b3401", "2c2a66cae9a16414088d", 0},
    {26, "3739e7d3668932d9ca44", "ec8d62bb9d4ba74c6729", 0},
    {34, "eef50cfa8f267939ad08", "759e579a56097da369b5", 0},
    {42, "2d6ae135fda1077788ca", "09a73575932668ca5e8d", 0},
    {50, "21d799da214da955d915", "45f8be8e21f1af08c828", 0},
};

static const vec_t *find_vec(const vec_t *t, size_t n, uint32_t oid)
{
    for (size_t i = 0; i < n; i++) {
        if (t[i].oid == oid) {
            return &t[i];
        }
    }
    return NULL;
}

/* 1 if SHAKE128(buf)[0..10) equals the hex string */
static int hash_is(const unsigned char *buf, size_t len, const char *hex)
{
    unsigned char h[10];
    char s[21];

    shake128(h, 10, buf, len);
    for (int i = 0; i < 10; i++) {
        s[2 * i] = "0123456789abcdef"[h[i] >> 4];
        s[2 * i + 1] = "0123456789abcdef"[h[i] & 15];
    }
    s[20] = '\0';
    return strcmp(s, hex) == 0;
}

static unsigned char sk[SK_MAX], pk[2 * 64], sm[SM_MAX + MLEN], mout[SM_MAX + MLEN];
static unsigned char big[131 * 64]; /* a WOTS+ public key or signature, n = 64 */
static unsigned long long smlen, mlen;
static unsigned char seed_ctr[3 * 64];

/* WOTS+ vectors: public key, signature and leaf for fixed inputs */
static int job_wots_vec(void *arg)
{
    const vec_t *v = arg;
    xmss_params params;
    unsigned char sk_seed[64], pub_seed[64], m[64], leaf[64];
    uint32_t addr[8], addr2[8];

    if (xmss_parse_oid(&params, v->oid) != 0) return 1;
    for (unsigned i = 0; i < 8; i++) {
        addr[i] = 500000000u * i;
        addr2[i] = 400000000u * i;
    }
    for (unsigned i = 0; i < params.n; i++) {
        m[i] = (unsigned char)(3 * i);
        pub_seed[i] = (unsigned char)(2 * i);
        sk_seed[i] = (unsigned char)i;
    }
    wots_pkgen(&params, big, sk_seed, pub_seed, addr);
    if (!hash_is(big, params.wots_sig_bytes, v->pk)) return 2;
    wots_sign(&params, big, m, sk_seed, pub_seed, addr);
    if (!hash_is(big, params.wots_sig_bytes, v->sig)) return 3;
    gen_leaf_wots(&params, leaf, sk_seed, pub_seed, addr, addr2);
    if (!hash_is(leaf, params.n, v->leaf)) return 4;
    return 0;
}

#ifdef PQC_HOST
/* Full vectors with the plain reference core: key pair from the seed
 * 0, 1, 2, ..., then the signature of the one-byte message 37 at index
 * 2^(h-1). Too slow for the device (each signature rebuilds its trees). */
static int job_full_vec(void *arg)
{
    const vec_t *v = arg;
    int mt = (v->leaf != NULL); /* leaf field reused as the XMSS^MT flag */
    xmss_params params;
    unsigned char msg[1] = {37};

    if ((mt ? xmssmt_parse_oid(&params, v->oid) : xmss_parse_oid(&params, v->oid)) != 0) return 1;
    for (unsigned i = 0; i < 3 * params.n; i++) {
        seed_ctr[i] = (unsigned char)i;
    }
    xmssmt_slow_core_seed_keypair(&params, pk, sk, seed_ctr);
    if (!hash_is(pk, params.pk_bytes, v->pk)) return 2;
    ull_to_bytes(sk, params.index_bytes, 1ull << (params.full_height - 1));
    if (mt) {
        xmssmt_slow_core_sign(&params, sk, sm, &smlen, msg, 1);
    } else {
        xmss_slow_core_sign(&params, sk, sm, &smlen, msg, 1);
    }
    if (!hash_is(sm, params.sig_bytes, v->sig)) return 3;
    if ((mt ? xmssmt_core_sign_open(&params, mout, &mlen, sm, smlen, pk)
            : xmss_core_sign_open(&params, mout, &mlen, sm, smlen, pk)) != 0) return 4;
    return 0;
}
#endif

/* ---------- timed operations ---------- */
typedef struct {
    const char *name;
    int mt;
    uint32_t oid;
    int sign_iters;
    int heavy; /* bit 0: ref, bit 1: opt. Key generation alone takes minutes
                  for that variant, so it only runs when asked for by name. */
} set_t;

static const set_t sets[] = {
    {"xmssmt-sha2_20-4_256", 1, 2, 40, 0},
    {"xmssmt-sha2_20-4_192", 1, 34, 40, 0},
    {"xmssmt-shake256_20-4_256", 1, 42, 8, 0},
    {"xmssmt-shake256_20-4_192", 1, 50, 8, 1},
    {"xmss-sha2_10_256", 0, 1, 8, 1},
    {"xmss-sha2_10_192", 0, 13, 8, 1},
    {"xmss-shake256_10_256", 0, 16, 4, 3},
};

static const set_t *S;
static xmss_params P;
static const unsigned char msg[MLEN + 1] = "ESP32-C3 firmware image digest: 0123456789abcdef0123456789abcdef";
static int pk_vector_ok;

/* Key pair from the seed 0, 1, 2, ... so the public key can be compared
 * with the published vector. */
static int job_keygen(void *arg)
{
    const vec_t *v;
    (void)arg;

    for (unsigned i = 0; i < 3 * P.n; i++) {
        seed_ctr[i] = (unsigned char)i;
    }
    /* The state has unused bytes; start from zero so that its fingerprint
     * does not depend on what was in the buffer before. */
    memset(sk, 0, sizeof(sk));
    xmss_port_seed = seed_ctr;
    if ((S->mt ? xmssmt_core_keypair(&P, pk, sk) : xmss_core_keypair(&P, pk, sk)) != 0) return 1;
    xmss_port_seed = NULL;
    v = S->mt ? find_vec(vec_xmssmt, sizeof(vec_xmssmt) / sizeof(vec_t), S->oid)
              : find_vec(vec_xmss, sizeof(vec_xmss) / sizeof(vec_t), S->oid);
    pk_vector_ok = v && hash_is(pk, P.pk_bytes, v->pk);
    return 0;
}

static int job_sign(void *arg)
{
    (void)arg;
    return S->mt ? xmssmt_core_sign(&P, sk, sm, &smlen, msg, MLEN)
                 : xmss_core_sign(&P, sk, sm, &smlen, msg, MLEN);
}

static int job_verify(void *arg)
{
    (void)arg;
    if ((S->mt ? xmssmt_core_sign_open(&P, mout, &mlen, sm, smlen, pk)
               : xmss_core_sign_open(&P, mout, &mlen, sm, smlen, pk)) != 0) return 1;
    return (mlen != MLEN) || memcmp(mout, msg, MLEN) != 0;
}

static int job_reject(void *arg)
{
    int rc;
    (void)arg;

    sm[P.sig_bytes / 2] ^= 1;
    rc = S->mt ? xmssmt_core_sign_open(&P, mout, &mlen, sm, smlen, pk)
               : xmss_core_sign_open(&P, mout, &mlen, sm, smlen, pk);
    sm[P.sig_bytes / 2] ^= 1;
    return rc == 0;
}

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};
    char alg[48];

    for (int v = 0; v < 2; v++) {
        const char *vn = variants[v];

        pqc_prims_select(v);
        xmss_fast = v;
        xmss_hash_cache_clear();
#if !defined(PQC_HOST) && !defined(XMSS_HW_HASH_OFF)
        xmss_hw_prf = v ? xmss_sha_hw_prf : NULL;
        xmss_hw_thash_f = v ? xmss_sha_hw_thash_f : NULL;
        xmss_hw_thash_h = v ? xmss_sha_hw_thash_h : NULL;
        xmss_hw_cache_clear();
#endif

        /* WOTS+ vectors of every hash function the reference code supports */
        for (size_t i = 0; i < sizeof(vec_wots) / sizeof(vec_wots[0]); i++) {
            strcpy(alg, "kat-wots-oid");
            alg[12] = (char)('0' + vec_wots[i].oid / 10);
            alg[13] = (char)('0' + vec_wots[i].oid % 10);
            alg[14] = '\0';
            if (bench_selected(bench_name(alg, vn, ""))) {
                bench_check(bench_name(alg, vn, "vector"),
                            bench_run(bench_name(alg, vn, "vector"), job_wots_vec, (void *)&vec_wots[i], 1,
                                      STACK) == 0);
            }
        }
#ifdef PQC_HOST
        {
            static const vec_t full[] = {
                {1, "7de72d192121f414d4bb", "8b6cb278d50a3694ca38", 0},
                {13, "5933d4b1e696804718c7", "6ec9da2e05da544d9c5d", 0},
                {16, "cef3d38791d56efee1b3", "9939a0f87502df5d1e31", 0},
                {2, "9df4c75282451bf2bc53", "fd4ff4c18801147b2804", "mt"},
                {34, "eef50cfa8f267939ad08", "759e579a56097da369b5", "mt"},
                {42, "2d6ae135fda1077788ca", "09a73575932668ca5e8d", "mt"},
            };
            for (size_t i = 0; i < sizeof(full) / sizeof(full[0]); i++) {
                strcpy(alg, full[i].leaf ? "hostkat-xmssmt-oid" : "hostkat-xmss-oid");
                size_t l = strlen(alg);
                alg[l] = (char)('0' + full[i].oid / 10);
                alg[l + 1] = (char)('0' + full[i].oid % 10);
                alg[l + 2] = '\0';
                if (bench_selected(bench_name(alg, vn, ""))) {
                    bench_check(bench_name(alg, vn, "vector"),
                                bench_run(bench_name(alg, vn, "vector"), job_full_vec, (void *)&full[i], 1,
                                          STACK) == 0);
                }
            }
        }
#endif

        for (size_t i = 0; i < sizeof(sets) / sizeof(sets[0]); i++) {
            S = &sets[i];
            if (!bench_selected(bench_name(S->name, vn, ""))) {
                continue;
            }
            if ((S->heavy & (1 << v)) && bench_selected(bench_name("x", vn, "x"))) {
                bench_info(bench_name(S->name, vn, "skipped"), "run with -f %s/%s", S->name, vn);
                continue;
            }
            if ((S->mt ? xmssmt_parse_oid(&P, S->oid) : xmss_parse_oid(&P, S->oid)) != 0) {
                bench_check(bench_name(S->name, vn, "params"), 0);
                continue;
            }
            if (v == 1) {
                /* BDS trade-off parameter k of the reference code: keep the
                 * top 2^k - k - 1 right-hand nodes, and signing has (h - k)/2
                 * tree-hash steps per tree instead of h/2. The reference
                 * default is 0. Same keys and signatures, larger state. */
                P.bds_k = (P.tree_height == 5) ? 3 : 4;
                P.sk_bytes = xmss_xmssmt_core_sk_bytes(&P);
            }
            if (P.sk_bytes > SK_MAX || P.sig_bytes > SM_MAX) {
                bench_check(bench_name(S->name, vn, "params"), 0);
                continue;
            }
            bench_info(bench_name(S->name, vn, "sizes"), "pk=%u sk_with_state=%u sig=%u signatures=2^%u n=%u w=%u bds_k=%u",
                       P.pk_bytes, (unsigned)P.sk_bytes, P.sig_bytes, P.full_height, P.n, P.wots_w, P.bds_k);
            xmss_hash_cache_clear();
#ifndef PQC_HOST
            xmss_hw_cache_clear();
#endif
            bench_run(bench_name(S->name, vn, "keygen"), job_keygen, NULL, 1, STACK);
            bench_check(bench_name(S->name, vn, "pk-vector"), pk_vector_ok);
            bench_digest(bench_name(S->name, vn, "pk"), pk, P.pk_bytes);
            bench_run(bench_name(S->name, vn, "sign"), job_sign, NULL, S->sign_iters, STACK);
            bench_check(bench_name(S->name, vn, "verify"),
                        bench_run(bench_name(S->name, vn, "verify"), job_verify, NULL, 5, STACK) == 0);
            bench_check(bench_name(S->name, vn, "reject"),
                        bench_run(bench_name(S->name, vn, "reject"), job_reject, NULL, 1, STACK) == 0);
            bench_digest(bench_name(S->name, vn, "sig"), sm, P.sig_bytes);
            bench_digest(bench_name(S->name, vn, "sk"), sk, (size_t)P.sk_bytes);
        }
    }
}
