/* Aigis-Enc+ (NGCC round-1 candidate, reference code from PQMagic). */
#include "bench.h"
#include "kemsig.h"

extern const kem_alg pqm_aigis_enc_plus_1_alg, pqm_aigis_enc_plus_2_alg, pqm_aigis_enc_plus_3_alg;

static const kem_alg *const algs[] = {&pqm_aigis_enc_plus_1_alg, &pqm_aigis_enc_plus_2_alg,
                                      &pqm_aigis_enc_plus_3_alg};

void suite_main(void)
{
    kem_run(algs, sizeof(algs) / sizeof(algs[0]));
}
