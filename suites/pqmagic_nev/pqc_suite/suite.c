/* NEV (NGCC round-1 candidate, reference code from PQMagic). */
#include "bench.h"
#include "kemsig.h"

#ifdef PQM_HASH_SM3
#define ALG(m) pqm_nev_##m##_sm3_alg
#else
#define ALG(m) pqm_nev_##m##_alg
#endif
#define MODES(X) X(512_769_c) X(1024_769_c) X(2048_769_c) X(512_1409) X(1024_1409) X(2048_1409) \
                 X(512_3329) X(1024_3329) X(2048_3329) X(512_769) X(1024_769) X(2048_769)

#define DECL(m) extern const kem_alg ALG(m);
MODES(DECL)
#define PTR(m) &ALG(m),
static const kem_alg *const algs[] = {MODES(PTR)};

void suite_main(void)
{
    kem_run(algs, sizeof(algs) / sizeof(algs[0]));
}
