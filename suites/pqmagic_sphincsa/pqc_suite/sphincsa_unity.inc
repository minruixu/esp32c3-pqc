/* One SPHINCS-alpha parameter set (PQMagic) as a single compilation unit.
 * The including file defines SPHINCS_A_MODE, SPHINCS_A_HASH_MODE_NAMESPACE
 * and THASH exactly as PQMagic's CMakeLists.txt does, one of PQM_HASH_SM3,
 * PQM_HASH_SHA2, PQM_HASH_SHAKE, and PQM_NS, PQM_NAME, PQM_ITERS_KEYGEN,
 * PQM_ITERS_SIGN. The tweakable hash is the "simple" one. The constant-sum
 * encoding table is kept in compact form (sphincsa_dp.h). */
#define PQM_CAT2(a, b) a##b
#define PQM_CAT(a, b) PQM_CAT2(a, b)
#ifndef PQM_UPSTREAM_HASH /* host cross-check with the unchanged upstream code */
#define PQC_PRIMS_SHA256
#define PQC_COMPACT_DP
#endif

#include "sig/sphincs-a/std/address.c"
#include "sig/sphincs-a/std/fors.c"
#include "sig/sphincs-a/std/merkle.c"
#include "sig/sphincs-a/std/sign.c"
#include "sig/sphincs-a/std/uintx.c"
#include "sig/sphincs-a/std/utils.c"
#include "sig/sphincs-a/std/utilsx1.c"
#include "sig/sphincs-a/std/wots.c"
#include "sig/sphincs-a/std/wotsx1.c"
#if defined(PQM_HASH_SM3)
#include "sig/sphincs-a/std/hash_sm3.c"
#include "sig/sphincs-a/std/thash_sm3_simple.c"
#elif defined(PQM_HASH_SHA2)
#include "sig/sphincs-a/std/sha2.c"
#include "sig/sphincs-a/std/hash_sha2.c"
#include "sig/sphincs-a/std/thash_sha2_simple.c"
#else
#include "sig/sphincs-a/std/hash_shake.c"
#include "sig/sphincs-a/std/thash_shake_simple.c"
#endif

#include "sig/sphincs-a/std/api.h"
#include "kemsig.h"

/* Task stack of each job. The scheme itself uses 3 to 4 KB. */
#ifndef PQM_STACK
#define PQM_STACK (20 * 1024)
#endif

const sig_alg PQM_CAT(PQM_NS, _alg) = {
    PQM_NAME, CRYPTO_PUBLICKEYBYTES, CRYPTO_SECRETKEYBYTES, CRYPTO_BYTES,
    {crypto_sign_keypair, crypto_sign_signature, crypto_sign_verify}, 0, PQM_STACK,
    PQM_ITERS_KEYGEN, PQM_ITERS_SIGN, 3};
