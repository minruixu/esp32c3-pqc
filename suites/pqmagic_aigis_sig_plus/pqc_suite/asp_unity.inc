/* One Aigis-Sig+ parameter set (NGCC round-1 reference in PQMagic) as a
 * single compilation unit. The including file defines PARAMS (1..3),
 * PQMAGIC_NAMESPACE_PREFIX, PQM_ALG (exported descriptor), PQM_NAME and
 * PQM_STACK. Built as PQMagic builds it (USE_SHA3); random bytes come from
 * the harness and Keccak-f[1600] from pqc_prims.h. */
#define USE_SHA3
#ifndef PQM_UPSTREAM_HASH /* host cross-check with the upstream hash code */
#define PQC_PRIMS_KECCAK
#define PQC_PRIMS_SM3
#endif
#include "sig/aigis-sig-plus/std/namespace.h"
#undef randombytes
#define randombytes kemsig_randombytes_sz

#include "sig/aigis-sig-plus/std/sign.c"
#include "sig/aigis-sig-plus/std/polyvec.c"
#include "sig/aigis-sig-plus/std/packing.c"
#include "sig/aigis-sig-plus/std/poly.c"
#include "sig/aigis-sig-plus/std/reduce.c"
#include "sig/aigis-sig-plus/std/ntt.c"
#include "sig/aigis-sig-plus/std/rounding.c"
#include "sig/aigis-sig-plus/std/fips202.c"
#include "sig/aigis-sig-plus/std/hashkdf.c"
#include "sig/aigis-sig-plus/std/auxfunc.c"
/* pspm.c and fips202.c each have their own static load64/store64 */
#define load64 pspm_load64
#define store64 pspm_store64
#include "sig/aigis-sig-plus/std/pspm.c"
#undef load64
#undef store64

#include "sig/aigis-sig-plus/std/api.h"
#include "kemsig.h"

static int pqm_sign(unsigned char *sig, size_t *siglen, const unsigned char *m, size_t mlen,
                    const unsigned char *sk)
{
    unsigned long long n = 0;
    int rc = msig_sign((unsigned char *)sk, (unsigned char *)m, mlen, sig, &n);
    *siglen = (size_t)n;
    return rc;
}

static int pqm_verify(const unsigned char *sig, size_t siglen, const unsigned char *m, size_t mlen,
                      const unsigned char *pk)
{
    return msig_verf((unsigned char *)pk, (unsigned char *)sig, siglen, (unsigned char *)m, mlen);
}

const sig_alg PQM_ALG = {
    PQM_NAME, SIG_PUBLICKEYBYTES, SIG_SECRETKEYBYTES, SIG_BYTES,
    {msig_keygen, pqm_sign, pqm_verify}, 0, PQM_STACK, 5, 10, 5};
