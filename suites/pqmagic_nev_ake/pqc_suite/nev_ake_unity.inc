/* One NEV-AKE parameter set (NGCC round-1 reference in PQMagic) as a single
 * compilation unit. The including file defines PARAMS (1..9),
 * PQMAGIC_NAMESPACE_PREFIX, PQM_ALG (exported descriptor) and PQM_NAME.
 * Built as PQMagic builds it (USE_SHA3); random bytes come from the harness
 * and Keccak-f[1600] from pqc_prims.h. */
#define USE_SHA3
#ifndef PQM_UPSTREAM_HASH /* host cross-check with the upstream hash code */
#define PQC_PRIMS_KECCAK
#define PQC_PRIMS_SM3
#endif
#include "kex/nev-ake/std/namespace.h"
#undef randombytes
#define randombytes kemsig_randombytes_ull

#include "kex/nev-ake/std/poly.c"
#include "kex/nev-ake/std/ntt.c"
#include "kex/nev-ake/std/owpke.c"
#include "kex/nev-ake/std/owkem.c"
#include "kex/nev-ake/std/verify.c"
#include "kex/nev-ake/std/sample.c"
#include "kex/nev-ake/std/pack.c"
#include "kex/nev-ake/std/cca.c"
#include "kex/nev-ake/std/cpa.c"
#include "kex/nev-ake/std/ake.c"
#include "kex/nev-ake/std/symmetrics/hashkdf.c"
#include "kex/nev-ake/std/symmetrics/fips202.c"
#include "kex/nev-ake/std/symmetrics/auxfunc.c"

#include "kex/nev-ake/std/api.h"
#include "kemsig.h"
#include "ake_bench.h"

const ake_alg PQM_ALG = {
    PQM_NAME, KEM_PUBLICKEYBYTES, KEM_SECRETKEYBYTES, AKE_M1_BYTES, AKE_ST_BYTES, AKE_M2_BYTES,
    SEED_BYTES, ake_party_keygen, ake_init, ake_der_response, ake_der_init, 60 * 1024, 10};
