/* One PQMagic ML-KEM parameter set and hash mode as a single compilation
 * unit. The including file defines ML_KEM_MODE (512/768/1024), PQM_NS
 * (symbol prefix), PQM_NAME (job name) and, for the SHAKE variant,
 * USE_SHAKE. Symbols that PQMagic does not prefix per hash mode are renamed. */
#define PQM_CAT2(a, b) a##b
#define PQM_CAT(a, b) PQM_CAT2(a, b)
#include "kem/ml_kem/std/params.h"
#undef ML_KEM_NAMESPACE
#define ML_KEM_NAMESPACE(s) PQM_CAT(PQM_NS, s)
#define crypto_kem_keypair_internal PQM_CAT(PQM_NS, _keypair_internal)
#define crypto_kem_enc_internal PQM_CAT(PQM_NS, _enc_internal)
#define sm3_256 PQM_CAT(PQM_NS, _sm3_256)
#define sm3_512 PQM_CAT(PQM_NS, _sm3_512)

#include "kem/ml_kem/std/cbd.c"
#include "kem/ml_kem/std/indcpa.c"
#include "kem/ml_kem/std/kem.c"
#include "kem/ml_kem/std/ntt.c"
#include "kem/ml_kem/std/poly.c"
#include "kem/ml_kem/std/polyvec.c"
#include "kem/ml_kem/std/reduce.c"
#ifndef USE_SHAKE
#include "kem/ml_kem/std/sm3_hash.c"
#endif
#include "kem/ml_kem/std/symmetric.c"
#include "kem/ml_kem/std/verify.c"

#include "kem/ml_kem/std/api.h"
#include "kemsig.h"
#include "mlkem_pqm.h"

const kem_alg PQM_CAT(PQM_NS, _alg) = {
    PQM_NAME, CRYPTO_PUBLICKEYBYTES, CRYPTO_SECRETKEYBYTES, CRYPTO_CIPHERTEXTBYTES, CRYPTO_BYTES,
    {crypto_kem_keypair, crypto_kem_enc, crypto_kem_dec}, 0, 40 * 1024, 10};

/* Deterministic entry points, for the FIPS 203 known-answer test. */
const mlkem_pqm_derand PQM_CAT(PQM_NS, _derand) = {crypto_kem_keypair_internal, crypto_kem_enc_internal};
