#include <string.h>
#include "mlkem_native.h"
#include "kem_vectors.h"
#include "tests.h"

#define PK_BYTES MLKEM_PUBLICKEYBYTES(MLK_CONFIG_PARAMETER_SET)
#define SK_BYTES MLKEM_SECRETKEYBYTES(MLK_CONFIG_PARAMETER_SET)
#define CT_BYTES MLKEM_CIPHERTEXTBYTES(MLK_CONFIG_PARAMETER_SET)

static uint8_t pk[PK_BYTES], sk[SK_BYTES], ct[CT_BYTES];
static uint8_t ss_a[MLKEM_BYTES], ss_b[MLKEM_BYTES];

/* Known-answer test against the vectors shipped with mlkem-native. */
int kem_kat(void)
{
    uint8_t coins[2 * MLKEM_SYMBYTES];
    memcpy(coins, test_vector_d, MLKEM_SYMBYTES);
    memcpy(coins + MLKEM_SYMBYTES, test_vector_z, MLKEM_SYMBYTES);

    if (mlkem_keypair_derand(pk, sk, coins) != 0) return 1;
    if (memcmp(pk, test_vector_pk, PK_BYTES) != 0) return 2;
    if (memcmp(sk, test_vector_sk, SK_BYTES) != 0) return 3;

    if (mlkem_enc_derand(ct, ss_a, test_vector_pk, test_vector_m) != 0) return 4;
    if (memcmp(ct, test_vector_ct, CT_BYTES) != 0) return 5;
    if (memcmp(ss_a, test_vector_ss, MLKEM_BYTES) != 0) return 6;

    if (mlkem_dec(ss_b, test_vector_ct, test_vector_sk) != 0) return 7;
    if (memcmp(ss_b, test_vector_ss, MLKEM_BYTES) != 0) return 8;

    /* A corrupted ciphertext must not yield the real shared secret. */
    ct[0] ^= 1;
    if (mlkem_dec(ss_b, ct, test_vector_sk) != 0) return 9;
    if (memcmp(ss_b, test_vector_ss, MLKEM_BYTES) == 0) return 10;
    return 0;
}

int kem_keygen(void) { return mlkem_keypair(pk, sk); }
int kem_encaps(void) { return mlkem_enc(ct, ss_a, pk); }
int kem_decaps(void)
{
    if (mlkem_dec(ss_b, ct, sk) != 0) return 1;
    return memcmp(ss_a, ss_b, MLKEM_BYTES) != 0;
}
