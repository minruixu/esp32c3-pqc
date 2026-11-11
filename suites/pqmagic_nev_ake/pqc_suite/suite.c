/* NEV-AKE (NGCC round-1 candidate, reference code from PQMagic): two
 * parties generate long-term keys, exchange two messages and must derive
 * the same session key. */
#include <string.h>
#include "ake_bench.h"
#include "bench.h"
#include "kemsig.h"
#include "pqc_prims.h"

#define MODES(X) X(512_769_c) X(1024_769_c) X(2048_769_c) X(512_1409) X(1024_1409) X(2048_1409) \
                 X(512_769) X(1024_769) X(2048_769)
#define DECL(m) extern const ake_alg pqm_nev_ake_##m##_alg;
MODES(DECL)
#define PTR(m) &pqm_nev_ake_##m##_alg,
static const ake_alg *const algs[] = {MODES(PTR)};

static const ake_alg *A;
static uint8_t *pki, *ski, *pkj, *skj, *m1, *st, *m2, *ki, *kj, *idi, *idj;

static int job_keygen(void *arg) { (void)arg; return A->party_keygen(pki, ski); }
static int job_keygen_peer(void *arg) { (void)arg; return A->party_keygen(pkj, skj); }
static int job_init(void *arg) { (void)arg; A->init(m1, st, ski, pkj); return 0; }
static int job_response(void *arg)
{
    (void)arg;
    A->der_response(m2, kj, idi, idj, skj, pki, m1);
    return 0;
}
static int job_finish(void *arg)
{
    (void)arg;
    A->der_init(ki, idi, idj, ski, pkj, st, m2);
    return memcmp(ki, kj, A->keyb) != 0;
}

/* A modified second message must not lead to the responder's key. */
static int job_tamper(void *arg)
{
    int same;
    (void)arg;
    m2[A->m2b / 2] ^= 0x04;
    A->der_init(ki, idi, idj, ski, pkj, st, m2);
    m2[A->m2b / 2] ^= 0x04;
    same = memcmp(ki, kj, A->keyb) == 0;
    A->der_init(ki, idi, idj, ski, pkj, st, m2);
    return same;
}

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};
    size_t size;
    uint8_t *arena = kemsig_arena(&size);

    kemsig_heap_info();
    for (int v = 0; v < 2; v++) {
        const char *vn = variants[v];
        pqc_prims_select(v);
        for (size_t i = 0; i < sizeof(algs) / sizeof(algs[0]); i++) {
            size_t need;
            A = algs[i];
            if (!bench_selected(bench_name(A->name, vn, ""))) {
                continue;
            }
            need = 2 * ((size_t)A->pkb + A->skb) + A->m1b + A->stb + A->m2b + 4 * (size_t)A->keyb;
            if (need > size) {
                bench_check(bench_name(A->name, vn, "buffers-fit"), 0);
                continue;
            }
            memset(arena, 0, need);
            pki = arena;
            ski = pki + A->pkb;
            pkj = ski + A->skb;
            skj = pkj + A->pkb;
            m1 = skj + A->skb;
            st = m1 + A->m1b;
            m2 = st + A->stb;
            ki = m2 + A->m2b;
            kj = ki + A->keyb;
            idi = kj + A->keyb;
            idj = idi + A->keyb;
            idj[0] = 1;
            if (v == 0) {
                bench_info(A->name, "ake pk=%u sk=%u msg1=%u msg2=%u state=%u key=%u", (unsigned)A->pkb,
                           (unsigned)A->skb, (unsigned)A->m1b, (unsigned)A->m2b, (unsigned)A->stb,
                           (unsigned)A->keyb);
            }
            bench_rng_seed(1);
            bench_run(bench_name(A->name, vn, "keygen"), job_keygen, NULL, A->iters, A->stack);
            bench_run(bench_name(A->name, vn, "keygen-peer"), job_keygen_peer, NULL, 1, A->stack);
            bench_rng_seed(2);
            bench_run(bench_name(A->name, vn, "init"), job_init, NULL, A->iters, A->stack);
            bench_rng_seed(3);
            bench_run(bench_name(A->name, vn, "response"), job_response, NULL, A->iters, A->stack);
            bench_run(bench_name(A->name, vn, "finish"), job_finish, NULL, A->iters, A->stack);
            bench_check(bench_name(A->name, vn, "tamper"),
                        bench_run(bench_name(A->name, vn, "tamper"), job_tamper, NULL, 1, A->stack) == 0);
            bench_digest(bench_name(A->name, vn, "pk"), pki, A->pkb);
            bench_digest(bench_name(A->name, vn, "sk"), ski, A->skb);
            bench_digest(bench_name(A->name, vn, "msg1"), m1, A->m1b);
            bench_digest(bench_name(A->name, vn, "msg2"), m2, A->m2b);
            bench_digest(bench_name(A->name, vn, "key"), ki, A->keyb);
        }
    }
    pqc_prims_select(PQC_PRIMS_REF);
}
