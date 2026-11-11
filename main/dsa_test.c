#include <string.h>
#include "mldsa_native.h"
#include "dsa_vectors.h"
#include "tests.h"

#define PK_BYTES MLDSA_PUBLICKEYBYTES(MLD_CONFIG_PARAMETER_SET)
#define SK_BYTES MLDSA_SECRETKEYBYTES(MLD_CONFIG_PARAMETER_SET)
#define SIG_BYTES MLDSA_BYTES(MLD_CONFIG_PARAMETER_SET)

static uint8_t pk[PK_BYTES], sk[SK_BYTES], sig[SIG_BYTES];
static const uint8_t msg[] = "post-quantum hello from an ESP32-C3";
static const uint8_t ctx[] = "pqc_c3";

/* Known-answer test against the vectors shipped with mldsa-native. */
int dsa_kat(void)
{
    uint8_t pre[MLD_DOMAIN_SEPARATION_MAX_BYTES];
    size_t pre_len;

    if (mldsa_keypair_internal(pk, sk, test_vector_rnd) != 0) return 1;
    if (memcmp(pk, test_vector_pk, PK_BYTES) != 0) return 2;
    if (memcmp(sk, test_vector_sk, SK_BYTES) != 0) return 3;

    pre_len = mldsa_prepare_domain_separation_prefix(
        pre, NULL, 0, (const uint8_t *)TEST_VECTOR_CTX, TEST_VECTOR_CTX_LEN,
        MLD_PREHASH_NONE);
    if (pre_len == 0) return 4;
    if (mldsa_signature_internal(sig, (const uint8_t *)TEST_VECTOR_MSG,
                                 TEST_VECTOR_MSG_LEN, pre, pre_len,
                                 test_vector_rnd, test_vector_sk, 0) != 0) return 5;
    if (memcmp(sig, test_vector_sig, SIG_BYTES) != 0) return 6;

    if (mldsa_verify(test_vector_sig, (const uint8_t *)TEST_VECTOR_MSG,
                     TEST_VECTOR_MSG_LEN, (const uint8_t *)TEST_VECTOR_CTX,
                     TEST_VECTOR_CTX_LEN, test_vector_pk) != 0) return 7;

    /* A corrupted signature must be rejected. */
    sig[10] ^= 1;
    if (mldsa_verify(sig, (const uint8_t *)TEST_VECTOR_MSG,
                     TEST_VECTOR_MSG_LEN, (const uint8_t *)TEST_VECTOR_CTX,
                     TEST_VECTOR_CTX_LEN, test_vector_pk) == 0) return 8;
    return 0;
}

int dsa_keygen(void) { return mldsa_keypair(pk, sk); }
int dsa_sign(void)
{
    return mldsa_signature(sig, msg, sizeof(msg) - 1, ctx, sizeof(ctx) - 1, sk);
}
int dsa_verify(void)
{
    return mldsa_verify(sig, msg, sizeof(msg) - 1, ctx, sizeof(ctx) - 1, pk);
}
