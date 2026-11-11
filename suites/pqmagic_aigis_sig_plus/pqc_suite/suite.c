/* Aigis-Sig+ (NGCC round-1 candidate, reference code from PQMagic). */
#include "bench.h"
#include "kemsig.h"

extern const sig_alg pqm_aigis_sig_plus_1_alg, pqm_aigis_sig_plus_2_alg, pqm_aigis_sig_plus_3_alg;

/* On the device only parameter set I runs: signing with set II overflowed a
 * 112000-byte stack, the largest a job can get in this image, and set III
 * needs more still. The host build runs all three. */
static const sig_alg *const algs[] = {
    &pqm_aigis_sig_plus_1_alg,
#ifdef PQC_HOST
    &pqm_aigis_sig_plus_2_alg,
    &pqm_aigis_sig_plus_3_alg,
#endif
};

void suite_main(void)
{
    sig_run(algs, sizeof(algs) / sizeof(algs[0]));
}
