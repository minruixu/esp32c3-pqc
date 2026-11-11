/* PQMagic's SLH-DSA with SM3: 128f and 128s, simple and robust. */
#include "bench.h"
#include "kemsig.h"

extern const sig_alg pqm_slhdsa_sm3_128f_simple_alg, pqm_slhdsa_sm3_128s_simple_alg;
extern const sig_alg pqm_slhdsa_sm3_128f_robust_alg, pqm_slhdsa_sm3_128s_robust_alg;

static const sig_alg *const algs[] = {
    &pqm_slhdsa_sm3_128f_simple_alg, &pqm_slhdsa_sm3_128s_simple_alg,
    &pqm_slhdsa_sm3_128f_robust_alg, &pqm_slhdsa_sm3_128s_robust_alg,
};

void suite_main(void)
{
    sig_run(algs, sizeof(algs) / sizeof(algs[0]));
}
