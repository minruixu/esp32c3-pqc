/* Aigis-sig (PQMagic): parameter sets 1-3, SM3 and SHAKE hash modes. */
#include "bench.h"
#include "kemsig.h"

extern const sig_alg aigis_sig_1_sm3_alg, aigis_sig_2_sm3_alg, aigis_sig_3_sm3_alg;
extern const sig_alg aigis_sig_1_shake_alg, aigis_sig_2_shake_alg, aigis_sig_3_shake_alg;

static const sig_alg *const algs[] = {
    &aigis_sig_1_sm3_alg,   &aigis_sig_2_sm3_alg,   &aigis_sig_3_sm3_alg,
    &aigis_sig_1_shake_alg, &aigis_sig_2_shake_alg, &aigis_sig_3_shake_alg,
};

void suite_main(void)
{
    sig_run(algs, sizeof(algs) / sizeof(algs[0]));
}
