#include <string.h>
#include "bench.h"
#include "kemsig.h"
#include "pqc_prims.h"
#ifndef PQC_HOST
#include "esp_heap_caps.h"
#include "esp_heap_caps_init.h"
#endif

/* All key, ciphertext and signature buffers come out of one static arena. */
#ifndef KEMSIG_ARENA
#define KEMSIG_ARENA 16384
#endif
#define MSG_BYTES 32

static uint8_t arena[KEMSIG_ARENA];
static const char *const variants[] = {"ref", "opt"};

/* The heap of the RAM image is split into regions, and the largest one is
 * 112 KB, which bounds the stack a job can get. A suite whose reference code
 * needs more defines KEMSIG_EXTRA_HEAP: that many bytes of static memory are
 * handed to the heap as one more region, large enough for the big stack. */
#if !defined(PQC_HOST) && defined(KEMSIG_EXTRA_HEAP)
static uint8_t extra_heap[KEMSIG_EXTRA_HEAP] __attribute__((aligned(16)));
#endif

/* The largest free block bounds the stack a job can get on the device. */
void kemsig_heap_info(void)
{
#if !defined(PQC_HOST) && defined(KEMSIG_EXTRA_HEAP)
    static int added;
    if (!added) {
        added = 1;
        heap_caps_add_region((intptr_t)extra_heap, (intptr_t)extra_heap + sizeof(extra_heap));
    }
#endif
#ifndef PQC_HOST
    bench_info("heap", "free=%u largest_block=%u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
#endif
}

uint8_t *kemsig_arena(size_t *size)
{
    *size = sizeof(arena);
    return arena;
}

/* randombytes() with the prototypes some upstream projects declare. */
void kemsig_randombytes_ull(unsigned char *x, unsigned long long xlen)
{
    randombytes(x, (size_t)xlen);
}

void kemsig_randombytes_sz(unsigned char *x, size_t xlen)
{
    randombytes(x, xlen);
}

/* ---------------- KEM ---------------- */
static const kem_alg *K;
static const kem_ops *KO;
static uint8_t *k_pk, *k_sk, *k_ct, *k_ssa, *k_ssb;

static int kem_keygen(void *arg) { (void)arg; return KO->keypair(k_pk, k_sk); }
static int kem_encaps(void *arg) { (void)arg; return KO->enc(k_ct, k_ssa, k_pk); }
static int kem_decaps(void *arg)
{
    (void)arg;
    if (KO->dec(k_ssb, k_ct, k_sk) != 0) {
        return 1;
    }
    return memcmp(k_ssa, k_ssb, K->ssb) != 0 ? 2 : 0;
}

/* A corrupted ciphertext must not decapsulate to the real shared secret. */
static int kem_tamper(void *arg)
{
    int rc;
    (void)arg;
    k_ct[K->ctb / 2] ^= 0x04;
    rc = KO->dec(k_ssb, k_ct, k_sk);
    k_ct[K->ctb / 2] ^= 0x04;
    if (rc == 0 && memcmp(k_ssa, k_ssb, K->ssb) == 0) {
        return 1;
    }
    return 0;
}

/* The same test for a scheme whose upstream code is known to fail it: the
 * job itself succeeds and the outcome is kept for an information line. */
static int k_tamper_outcome;
static int kem_tamper_info(void *arg)
{
    k_tamper_outcome = kem_tamper(arg);
    return 0;
}

void kem_run(const kem_alg *const *algs, size_t n)
{
    kemsig_heap_info();
    for (int v = 0; v < 2; v++) {
        const char *vn = variants[v];
        pqc_prims_select(v);
        for (size_t i = 0; i < n; i++) {
            size_t need;
            K = algs[i];
            KO = (v && K->opt) ? K->opt : &K->ref;
            if (!bench_selected(bench_name(K->name, vn, ""))) {
                continue;
            }
            need = (size_t)K->pkb + K->skb + K->ctb + 2 * (size_t)K->ssb;
            if (need > sizeof(arena)) {
                bench_check(bench_name(K->name, vn, "buffers-fit"), 0);
                continue;
            }
            k_pk = arena;
            k_sk = k_pk + K->pkb;
            k_ct = k_sk + K->skb;
            k_ssa = k_ct + K->ctb;
            k_ssb = k_ssa + K->ssb;
            memset(arena, 0, need);
            if (v == 0) {
                bench_info(K->name, "kem pk=%u sk=%u ct=%u ss=%u", (unsigned)K->pkb, (unsigned)K->skb,
                           (unsigned)K->ctb, (unsigned)K->ssb);
            }
            bench_rng_seed(1);
            bench_run(bench_name(K->name, vn, "keygen"), kem_keygen, NULL, K->iters, K->stack);
            bench_rng_seed(2);
            bench_run(bench_name(K->name, vn, "encaps"), kem_encaps, NULL, K->iters, K->stack);
            bench_run(bench_name(K->name, vn, "decaps"), kem_decaps, NULL, K->iters, K->stack);
            if (K->flags & KEM_TAMPER_INFO) {
                bench_run(bench_name(K->name, vn, "tamper"), kem_tamper_info, NULL, 1, K->stack);
                bench_info(bench_name(K->name, vn, "tamper"), "%s",
                           k_tamper_outcome == 0 ? "modified ciphertext gave a different key"
                                   : "modified ciphertext was ACCEPTED with the same key (upstream behaviour)");
            } else {
                bench_check(bench_name(K->name, vn, "tamper"),
                            bench_run(bench_name(K->name, vn, "tamper"), kem_tamper, NULL, 1, K->stack) == 0);
            }
            bench_digest(bench_name(K->name, vn, "pk"), k_pk, K->pkb);
            bench_digest(bench_name(K->name, vn, "sk"), k_sk, K->skb);
            bench_digest(bench_name(K->name, vn, "ct"), k_ct, K->ctb);
            bench_digest(bench_name(K->name, vn, "ss"), k_ssa, K->ssb);
        }
    }
    pqc_prims_select(PQC_PRIMS_REF);
}

/* ---------------- signatures ---------------- */
static const sig_alg *S;
static const sig_ops *SO;
static uint8_t *s_pk, *s_sk, *s_sig;
static size_t s_siglen;
static uint8_t s_msg[MSG_BYTES];

static int sig_keygen(void *arg) { (void)arg; return SO->keypair(s_pk, s_sk); }
static int sig_sign(void *arg)
{
    (void)arg;
    s_siglen = S->sigb;
    return SO->sign(s_sig, &s_siglen, s_msg, MSG_BYTES, s_sk);
}
static int sig_verify(void *arg)
{
    (void)arg;
    return SO->verify(s_sig, s_siglen, s_msg, MSG_BYTES, s_pk);
}

/* A changed signature and a changed message must both be rejected. */
static int sig_tamper(void *arg)
{
    int rc;
    (void)arg;
    s_sig[s_siglen / 2] ^= 0x04;
    rc = SO->verify(s_sig, s_siglen, s_msg, MSG_BYTES, s_pk);
    s_sig[s_siglen / 2] ^= 0x04;
    if (rc == 0) {
        return 1;
    }
    s_msg[0] ^= 1;
    rc = SO->verify(s_sig, s_siglen, s_msg, MSG_BYTES, s_pk);
    s_msg[0] ^= 1;
    return rc == 0 ? 2 : 0;
}

void sig_run(const sig_alg *const *algs, size_t n)
{
    kemsig_heap_info();
    for (int v = 0; v < 2; v++) {
        const char *vn = variants[v];
        pqc_prims_select(v);
        for (size_t i = 0; i < n; i++) {
            size_t need;
            S = algs[i];
            SO = (v && S->opt) ? S->opt : &S->ref;
            if (!bench_selected(bench_name(S->name, vn, ""))) {
                continue;
            }
            need = (size_t)S->pkb + S->skb + S->sigb;
            if (need > sizeof(arena)) {
                bench_check(bench_name(S->name, vn, "buffers-fit"), 0);
                continue;
            }
            s_pk = arena;
            s_sk = s_pk + S->pkb;
            s_sig = s_sk + S->skb;
            memset(arena, 0, need);
            for (int j = 0; j < MSG_BYTES; j++) {
                s_msg[j] = (uint8_t)(0xA0 + j);
            }
            s_siglen = 0;
            bench_rng_seed(1);
            bench_run(bench_name(S->name, vn, "keygen"), sig_keygen, NULL, S->it_keygen, S->stack);
            bench_rng_seed(2);
            bench_run(bench_name(S->name, vn, "sign"), sig_sign, NULL, S->it_sign, S->stack);
            bench_run(bench_name(S->name, vn, "verify"), sig_verify, NULL, S->it_verify, S->stack);
            bench_check(bench_name(S->name, vn, "tamper"),
                        bench_run(bench_name(S->name, vn, "tamper"), sig_tamper, NULL, 1, S->stack) == 0);
            if (v == 0) {
                bench_info(S->name, "sig pk=%u sk=%u sig=%u (max %u)", (unsigned)S->pkb, (unsigned)S->skb,
                           (unsigned)s_siglen, (unsigned)S->sigb);
            }
            bench_digest(bench_name(S->name, vn, "pk"), s_pk, S->pkb);
            bench_digest(bench_name(S->name, vn, "sk"), s_sk, S->skb);
            bench_digest(bench_name(S->name, vn, "sig"), s_sig, s_siglen);
        }
    }
    pqc_prims_select(PQC_PRIMS_REF);
}
