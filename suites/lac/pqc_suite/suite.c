/* LAC-v3b: Light, 128, 192 and 256. */
#include "bench.h"
#include "kemsig.h"

extern const kem_alg lac_light_alg, lac_128_alg, lac_192_alg, lac_256_alg;

static const kem_alg *const algs[] = {&lac_light_alg, &lac_128_alg, &lac_192_alg, &lac_256_alg};

void suite_main(void)
{
    kem_run(algs, sizeof(algs) / sizeof(algs[0]));
}
