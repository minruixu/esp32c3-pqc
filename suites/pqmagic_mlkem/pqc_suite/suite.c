/* PQMagic's ML-KEM: the SM3 instantiation ("mlkem768-sm3-pqmagic") and, as a
 * reference point, the same code with SHAKE, which must reproduce the
 * FIPS 203 known-answer vectors. */
#include <string.h>
#include "bench.h"
#include "kemsig.h"
#include "mlkem_pqm.h"
#include "pqc_prims.h"
#include "fips203_vectors.h"

extern const kem_alg pqm_mlkem512_sm3_alg, pqm_mlkem768_sm3_alg, pqm_mlkem1024_sm3_alg;
extern const kem_alg pqm_mlkem512_shake_alg, pqm_mlkem768_shake_alg, pqm_mlkem1024_shake_alg;
extern const mlkem_pqm_derand pqm_mlkem512_shake_derand, pqm_mlkem768_shake_derand,
    pqm_mlkem1024_shake_derand;

static const kem_alg *const algs[] = {
    &pqm_mlkem512_sm3_alg,   &pqm_mlkem768_sm3_alg,   &pqm_mlkem1024_sm3_alg,
    &pqm_mlkem512_shake_alg, &pqm_mlkem768_shake_alg, &pqm_mlkem1024_shake_alg,
};

typedef struct {
    const kem_alg *alg;
    const mlkem_pqm_derand *derand;
    const uint8_t *pk, *sk, *ct, *ss;
} kat_t;

static const kat_t kats[] = {
    {&pqm_mlkem512_shake_alg, &pqm_mlkem512_shake_derand, test_vector_pk_512, test_vector_sk_512,
     test_vector_ct_512, test_vector_ss_512},
    {&pqm_mlkem768_shake_alg, &pqm_mlkem768_shake_derand, test_vector_pk_768, test_vector_sk_768,
     test_vector_ct_768, test_vector_ss_768},
    {&pqm_mlkem1024_shake_alg, &pqm_mlkem1024_shake_derand, test_vector_pk_1024, test_vector_sk_1024,
     test_vector_ct_1024, test_vector_ss_1024},
};

static uint8_t pk[1568], sk[3168], ct[1568], ss[32];

/* FIPS 203 known-answer test (vectors from mlkem-native). */
static int job_kat(void *arg)
{
    const kat_t *k = arg;
    const kem_alg *a = k->alg;
    uint8_t coins[64];

    memcpy(coins, test_vector_d, 32);
    memcpy(coins + 32, test_vector_z, 32);
    if (k->derand->keypair_internal(pk, sk, coins) != 0) return 1;
    if (memcmp(pk, k->pk, a->pkb) != 0) return 2;
    if (memcmp(sk, k->sk, a->skb) != 0) return 3;
    if (k->derand->enc_internal(ct, ss, k->pk, test_vector_m) != 0) return 4;
    if (memcmp(ct, k->ct, a->ctb) != 0) return 5;
    if (memcmp(ss, k->ss, 32) != 0) return 6;
    if (a->ref.dec(ss, k->ct, k->sk) != 0) return 7;
    if (memcmp(ss, k->ss, 32) != 0) return 8;
    return 0;
}

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};

    for (int v = 0; v < 2; v++) {
        pqc_prims_select(v);
        for (size_t i = 0; i < sizeof(kats) / sizeof(kats[0]); i++) {
            const char *nm = bench_name(kats[i].alg->name, variants[v], "kat-fips203");
            if (bench_selected(nm)) {
                bench_check(nm, bench_run(nm, job_kat, (void *)&kats[i], 1, 40 * 1024) == 0);
            }
        }
    }
    kem_run(algs, sizeof(algs) / sizeof(algs[0]));
}
