/* PQMagic's ML-DSA: the SM3 instantiation ("mldsa65-sm3-pqmagic") and, as a
 * reference point, the same code with SHAKE, which must reproduce the
 * FIPS 204 known-answer vectors. */
#include "bench.h"
#include "kemsig.h"
#include "mldsa_pqm.h"
#include "pqc_prims.h"

#define DECL(m)                                                                 \
    extern const sig_alg pqm_mldsa##m##_sm3_alg, pqm_mldsa##m##_shake_alg;      \
    extern const mldsa_pqm_kat pqm_mldsa##m##_shake_kat;

#ifdef PQM_MLDSA_44
DECL(44)
#endif
#ifdef PQM_MLDSA_65
DECL(65)
#endif
#ifdef PQM_MLDSA_87
DECL(87)
#endif

static const sig_alg *const algs[] = {
#ifdef PQM_MLDSA_44
    &pqm_mldsa44_sm3_alg, &pqm_mldsa44_shake_alg,
#endif
#ifdef PQM_MLDSA_65
    &pqm_mldsa65_sm3_alg, &pqm_mldsa65_shake_alg,
#endif
#ifdef PQM_MLDSA_87
    &pqm_mldsa87_sm3_alg, &pqm_mldsa87_shake_alg,
#endif
};

static const mldsa_pqm_kat *const kats[] = {
#ifdef PQM_MLDSA_44
    &pqm_mldsa44_shake_kat,
#endif
#ifdef PQM_MLDSA_65
    &pqm_mldsa65_shake_kat,
#endif
#ifdef PQM_MLDSA_87
    &pqm_mldsa87_shake_kat,
#endif
};

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};

    kemsig_heap_info();
    for (int v = 0; v < 2; v++) {
        pqc_prims_select(v);
        for (size_t i = 0; i < sizeof(kats) / sizeof(kats[0]); i++) {
            const char *nm = bench_name(kats[i]->alg->name, variants[v], "kat-fips204");
            if (bench_selected(nm)) {
                bench_check(nm, bench_run(nm, kats[i]->kat, 0, 1, kats[i]->alg->stack) == 0);
            }
        }
    }
    sig_run(algs, sizeof(algs) / sizeof(algs[0]));
}
