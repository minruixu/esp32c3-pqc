/* One PQMagic SLH-DSA (SPHINCS+) parameter set as a single compilation unit.
 * The including file defines SLH_DSA_MODE, SLH_DSA_HASH_MODE_NAMESPACE and
 * THASH exactly as PQMagic's CMakeLists.txt does, PQM_THASH_SIMPLE or
 * PQM_THASH_ROBUST, and PQM_NS, PQM_NAME, PQM_ITERS_KEYGEN, PQM_ITERS_SIGN. */
#define PQM_CAT2(a, b) a##b
#define PQM_CAT(a, b) PQM_CAT2(a, b)

#include "sig/slh_dsa/std/address.c"
#include "sig/slh_dsa/std/fors.c"
#include "sig/slh_dsa/std/merkle.c"
#include "sig/slh_dsa/std/sign.c"
#include "sig/slh_dsa/std/utils.c"
#include "sig/slh_dsa/std/utilsx1.c"
#include "sig/slh_dsa/std/wots.c"
#include "sig/slh_dsa/std/wotsx1.c"
#include "sig/slh_dsa/std/hash_sm3.c"
#ifdef PQM_THASH_ROBUST
#include "sig/slh_dsa/std/thash_sm3_robust.c"
#else
#include "sig/slh_dsa/std/thash_sm3_simple.c"
#endif

#include "sig/slh_dsa/std/api.h"
#include "kemsig.h"

const sig_alg PQM_CAT(PQM_NS, _alg) = {
    PQM_NAME, CRYPTO_PUBLICKEYBYTES, CRYPTO_SECRETKEYBYTES, CRYPTO_BYTES,
    {crypto_sign_keypair, crypto_sign_signature, crypto_sign_verify}, 0, 16 * 1024,
    PQM_ITERS_KEYGEN, PQM_ITERS_SIGN, 3};
