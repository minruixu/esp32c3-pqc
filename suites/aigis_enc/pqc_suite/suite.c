/* Aigis-enc (PQMagic): parameter sets 1-4, SM3 and SHAKE hash modes. */
#include "bench.h"
#include "kemsig.h"

extern const kem_alg aigis_enc_1_sm3_alg, aigis_enc_2_sm3_alg, aigis_enc_3_sm3_alg, aigis_enc_4_sm3_alg;
extern const kem_alg aigis_enc_1_shake_alg, aigis_enc_2_shake_alg, aigis_enc_3_shake_alg,
    aigis_enc_4_shake_alg;

static const kem_alg *const algs[] = {
    &aigis_enc_1_sm3_alg,   &aigis_enc_2_sm3_alg,   &aigis_enc_3_sm3_alg,   &aigis_enc_4_sm3_alg,
    &aigis_enc_1_shake_alg, &aigis_enc_2_shake_alg, &aigis_enc_3_shake_alg, &aigis_enc_4_shake_alg,
};

void suite_main(void)
{
    kem_run(algs, sizeof(algs) / sizeof(algs[0]));
}
