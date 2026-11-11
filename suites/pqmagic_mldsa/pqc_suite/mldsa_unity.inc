/* One PQMagic ML-DSA parameter set and hash mode as a single compilation
 * unit. The including file defines ML_DSA_MODE (44/65/87), PQM_NS (symbol
 * prefix), PQM_NAME (job name), PQM_STACK and, for the SHAKE variant,
 * USE_SHAKE. PQMagic's own symbol prefix does not tell the two hash modes
 * apart, so it is replaced. */
#define PQM_CAT2(a, b) a##b
#define PQM_CAT(a, b) PQM_CAT2(a, b)
#include "sig/ml_dsa/std/config.h"
#undef ML_DSA_NAMESPACE
#undef ML_DSA_NAMESPACETOP
#define ML_DSA_NAMESPACE(s) PQM_CAT(PQM_NS, _##s)
#define ML_DSA_NAMESPACETOP PQM_CAT(PQM_NS, _sign_message)

#include "sig/ml_dsa/std/ntt.c"
#include "sig/ml_dsa/std/packing.c"
#include "sig/ml_dsa/std/poly.c"
#include "sig/ml_dsa/std/polyvec.c"
#include "sig/ml_dsa/std/reduce.c"
#include "sig/ml_dsa/std/rounding.c"
#include "sig/ml_dsa/std/sign.c"
#ifdef USE_SHAKE
#include "sig/ml_dsa/std/symmetric-shake.c"
#endif

#include "sig/ml_dsa/std/api.h"
#include "kemsig.h"
#include "mldsa_pqm.h"

static const unsigned char pqm_ctx[1] = {0};

static int pqm_sign(unsigned char *sig, size_t *siglen, const unsigned char *m, size_t mlen,
                    const unsigned char *sk)
{
    return crypto_sign_signature(sig, siglen, m, mlen, pqm_ctx, 0, sk);
}

static int pqm_verify(const unsigned char *sig, size_t siglen, const unsigned char *m, size_t mlen,
                      const unsigned char *pk)
{
    return crypto_sign_verify(sig, siglen, m, mlen, pqm_ctx, 0, pk);
}

const sig_alg PQM_CAT(PQM_NS, _alg) = {
    PQM_NAME, CRYPTO_PUBLICKEYBYTES, CRYPTO_SECRETKEYBYTES, CRYPTO_BYTES,
    {crypto_sign_keypair, pqm_sign, pqm_verify}, 0, PQM_STACK, 5, 10, 5};

#ifdef USE_SHAKE
/* FIPS 204 known-answer test with the vectors of mldsa-native: key pair from
 * a seed, deterministic signature, verification of the reference signature. */
#define MLD_CONFIG_PARAMETER_SET ML_DSA_MODE
#include "fips204_vectors.h"

static int pqm_kat(void *arg)
{
    static uint8_t kpk[CRYPTO_PUBLICKEYBYTES], ksk[CRYPTO_SECRETKEYBYTES], ksig[CRYPTO_BYTES];
    uint8_t mext[2 + TEST_VECTOR_CTX_LEN + TEST_VECTOR_MSG_LEN];
    size_t siglen = 0;
    (void)arg;

    if (crypto_sign_keypair_internal(kpk, ksk, test_vector_rnd) != 0) return 1;
    if (memcmp(kpk, test_vector_pk, CRYPTO_PUBLICKEYBYTES) != 0) return 2;
    if (memcmp(ksk, test_vector_sk, CRYPTO_SECRETKEYBYTES) != 0) return 3;
    mext[0] = 0;
    mext[1] = (uint8_t)TEST_VECTOR_CTX_LEN;
    memcpy(mext + 2, TEST_VECTOR_CTX, TEST_VECTOR_CTX_LEN);
    memcpy(mext + 2 + TEST_VECTOR_CTX_LEN, TEST_VECTOR_MSG, TEST_VECTOR_MSG_LEN);
    if (crypto_sign_signature_internal(ksig, &siglen, mext, sizeof(mext), test_vector_rnd,
                                       test_vector_sk) != 0) return 4;
    if (siglen != CRYPTO_BYTES || memcmp(ksig, test_vector_sig, CRYPTO_BYTES) != 0) return 5;
    if (crypto_sign_verify(test_vector_sig, CRYPTO_BYTES, (const uint8_t *)TEST_VECTOR_MSG,
                           TEST_VECTOR_MSG_LEN, (const uint8_t *)TEST_VECTOR_CTX, TEST_VECTOR_CTX_LEN,
                           test_vector_pk) != 0) return 6;
    return 0;
}

const mldsa_pqm_kat PQM_CAT(PQM_NS, _kat) = {&PQM_CAT(PQM_NS, _alg), pqm_kat};
#endif
