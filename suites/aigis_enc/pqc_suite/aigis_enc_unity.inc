/* One Aigis-enc parameter set and hash mode as a single compilation unit.
 * The including file defines AIGIS_ENC_MODE (1..4), PQM_NS (symbol prefix),
 * PQM_NAME (job name) and, for the SHAKE variant, USE_SHAKE. PQMagic's own
 * symbol prefix does not tell the two hash modes apart, so it is replaced. */
#include "kem/aigis-enc/std/config.h"
#undef AIGIS_ENC_NAMESPACE
#define PQM_CAT2(a, b) a##b
#define PQM_CAT(a, b) PQM_CAT2(a, b)
#define AIGIS_ENC_NAMESPACE(s) PQM_CAT(PQM_NS, _##s)

#include "kem/aigis-enc/std/cbd.c"
#include "kem/aigis-enc/std/genmatrix.c"
#include "kem/aigis-enc/std/hashkdf.c"
#include "kem/aigis-enc/std/kem.c"
#include "kem/aigis-enc/std/ntt.c"
#include "kem/aigis-enc/std/owcpa.c"
#include "kem/aigis-enc/std/poly.c"
#include "kem/aigis-enc/std/polyvec.c"
#include "kem/aigis-enc/std/reduce.c"
#ifndef USE_SHAKE
#include "kem/aigis-enc/std/sm3kdf.c"
#endif
#include "kem/aigis-enc/std/verify.c"

#include "kemsig.h"
const kem_alg PQM_CAT(PQM_NS, _alg) = {
    PQM_NAME, CRYPTO_PUBLICKEYBYTES, CRYPTO_SECRETKEYBYTES, CRYPTO_CIPHERTEXTBYTES, CRYPTO_BYTES,
    {crypto_kem_keypair, crypto_kem_enc, crypto_kem_dec}, 0, 40 * 1024, 10};
