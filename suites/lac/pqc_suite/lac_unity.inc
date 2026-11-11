/* One LAC security level as a single compilation unit. The including file
 * defines one of LAC_LIGHT, LAC128, LAC192, LAC256, and PQM_NS (symbol
 * prefix), PQM_NAME (job name). Sources come from downloads/lac_port (see
 * tools/fetch/lac.sh). LAC's rand.c, which needs OpenSSL, is replaced by the
 * five functions below with the same behaviour: random bytes from the
 * harness, AES-256-CTR as the seed expander, SHA-256 as the hash. */
#include "lac_rename.h"

#include <string.h>
#include "bch.c"
#include "bin-lwe.c"
#include "ecc.c"
#include "encrypt.c"
#include "kem.c"

#include "kemsig.h"
#include "lac_port.h"
#include "sha2.h"

int random_bytes(unsigned char *r, unsigned int len)
{
    if (r == NULL) {
        return 1;
    }
    kemsig_randombytes_sz(r, len);
    return 0;
}

int pseudo_random_bytes(unsigned char *r, unsigned int len, const unsigned char *seed)
{
    if (r == NULL || seed == NULL) {
        return 1;
    }
    lac_aes256ctr(r, len, seed);
    return 0;
}

int hash(const unsigned char *in, unsigned int len_in, unsigned char *out)
{
    if (in == NULL || out == NULL) {
        return 1;
    }
    sha256(out, in, len_in);
    return 0;
}

int hash_to_k(const unsigned char *in, unsigned int len_in, unsigned char *out)
{
    unsigned char tmp_out[32];

    if (in == NULL || out == NULL) {
        return 1;
    }
    sha256(tmp_out, in, len_in);
    memcpy(out, tmp_out, MESSAGE_LEN);
    return 0;
}

int gen_seed(unsigned char *in, unsigned int len_in, unsigned char *out)
{
    if (in == NULL || out == NULL) {
        return 1;
    }
    sha256(out, in, len_in);
    return 0;
}

const kem_alg LAC_CAT(PQM_NS, _alg) = {
    PQM_NAME, CRYPTO_PUBLICKEYBYTES, CRYPTO_SECRETKEYBYTES, CRYPTO_CIPHERTEXTBYTES, CRYPTO_BYTES,
    {crypto_kem_keypair, crypto_kem_enc, crypto_kem_dec}, 0, 64 * 1024, 10};
