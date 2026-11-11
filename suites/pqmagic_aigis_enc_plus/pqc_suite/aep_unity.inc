/* One Aigis-Enc+ parameter set (NGCC round-1 reference in PQMagic) as a
 * single compilation unit. The including file defines PARAMS (1..3),
 * PQMAGIC_NAMESPACE_PREFIX, PQM_ALG (name of the exported descriptor) and
 * PQM_NAME (job name). Built as PQMagic builds it (USE_SHA3), except that
 * random bytes come from the harness and Keccak-f[1600] from pqc_prims.h. */
#define USE_SHA3
#ifndef PQM_UPSTREAM_HASH /* host cross-check with the upstream hash code */
#define PQC_PRIMS_KECCAK
#endif
#include "kem/aigis-enc-plus/std/namespace.h"
#undef randombytes
#define randombytes kemsig_randombytes_ull

#include "kem/aigis-enc-plus/std/cbd.c"
#include "kem/aigis-enc-plus/std/fips202.c"
#include "kem/aigis-enc-plus/std/poly.c"
#include "kem/aigis-enc-plus/std/reduce.c"
#include "kem/aigis-enc-plus/std/precomp.c"
#include "kem/aigis-enc-plus/std/ntt.c"
#include "kem/aigis-enc-plus/std/polyvec.c"
#include "kem/aigis-enc-plus/std/owcpa.c"
#include "kem/aigis-enc-plus/std/kem.c"
#include "kem/aigis-enc-plus/std/verify.c"
#include "kem/aigis-enc-plus/std/hashkdf.c"
#include "kem/aigis-enc-plus/std/gen_a.c"
#include "kem/aigis-enc-plus/std/pack.c"

#include "kemsig.h"

static int pqm_keypair(unsigned char *pk, unsigned char *sk) { return mkem_keygen(pk, sk); }
static int pqm_enc(unsigned char *ct, unsigned char *ss, const unsigned char *pk)
{
    return mkem_enc((uint8_t *)pk, ss, ct);
}
static int pqm_dec(unsigned char *ss, const unsigned char *ct, const unsigned char *sk)
{
    return mkem_dec((uint8_t *)sk, (uint8_t *)ct, ss);
}

const kem_alg PQM_ALG = {
    PQM_NAME, CRYPTO_PUBLICKEYBYTES, CRYPTO_SECRETKEYBYTES, CRYPTO_CIPHERTEXTBYTES, CRYPTO_BYTES,
    {pqm_keypair, pqm_enc, pqm_dec}, 0, 72 * 1024, 10, KEM_TAMPER_INFO};
