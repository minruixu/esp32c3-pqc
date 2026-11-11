/* One NEV parameter set (NGCC round-1 reference in PQMagic) as a single
 * compilation unit. The including file defines PARAMS (1..12),
 * PQMAGIC_NAMESPACE_PREFIX, PQM_ALG (exported descriptor) and PQM_NAME (job
 * name), and PQM_HASH_SM3 for the SM3 configuration (USE_ICCS upstream)
 * instead of the SHA-3 one (USE_SHA3). Random bytes come from the harness;
 * Keccak-f[1600] and the SM3 compression function from pqc_prims.h. */
#ifdef PQM_HASH_SM3
#define USE_ICCS
#else
#define USE_SHA3
#endif
#ifndef PQM_UPSTREAM_HASH /* host cross-check with the upstream hash code */
#define PQC_PRIMS_KECCAK
#define PQC_PRIMS_SM3
#endif
#include "kem/nev/std/namespace.h"
#undef randombytes
#define randombytes kemsig_randombytes_ull

#include "kem/nev/std/poly.c"
#include "kem/nev/std/ntt.c"
#include "kem/nev/std/owpke.c"
#include "kem/nev/std/verify.c"
#include "kem/nev/std/sample.c"
#include "kem/nev/std/pack.c"
#include "kem/nev/std/cca.c"
#include "kem/nev/std/cpa.c"
#include "kem/nev/std/symmetrics/hashkdf.c"
#include "kem/nev/std/symmetrics/fips202.c"
#include "kem/nev/std/symmetrics/auxfunc.c"

#include "kemsig.h"

static int pqm_keypair(unsigned char *pk, unsigned char *sk) { return kem_cca_keygen(pk, sk); }
static int pqm_enc(unsigned char *ct, unsigned char *ss, const unsigned char *pk)
{
    return kem_cca_enc(ss, ct, pk);
}
static int pqm_dec(unsigned char *ss, const unsigned char *ct, const unsigned char *sk)
{
    return kem_cca_dec(ss, (uint8_t *)ct, (uint8_t *)sk);
}

const kem_alg PQM_ALG = {
    PQM_NAME, KEM_CCA_PK_BYTES, KEM_CCA_SK_BYTES, KEM_CCA_CT_BYTES, SEED_BYTES,
    {pqm_keypair, pqm_enc, pqm_dec}, 0, 56 * 1024, 10};
