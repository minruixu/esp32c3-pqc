/* One Aigis-sig parameter set and hash mode as a single compilation unit.
 * The including file defines AIGIS_SIG_MODE (1..3), PQM_NS (symbol prefix),
 * PQM_NAME (job name), PQM_STACK and, for the SHAKE variant, USE_SHAKE.
 * PQMagic's own symbol prefix does not tell the two hash modes apart, so it
 * is replaced. */
#include "sig/aigis-sig/std/config.h"
#undef AIGIS_SIG_NAMESPACE
#undef AIGIS_SIG_NAMESPACETOP
#define PQM_CAT2(a, b) a##b
#define PQM_CAT(a, b) PQM_CAT2(a, b)
#define AIGIS_SIG_NAMESPACE(s) PQM_CAT(PQM_NS, _##s)
#define AIGIS_SIG_NAMESPACETOP PQM_CAT(PQM_NS, _sign_message)

#include "sig/aigis-sig/std/ntt.c"
#include "sig/aigis-sig/std/packing.c"
#include "sig/aigis-sig/std/poly.c"
#include "sig/aigis-sig/std/polyvec.c"
#include "sig/aigis-sig/std/reduce.c"
#include "sig/aigis-sig/std/rounding.c"
#include "sig/aigis-sig/std/sign.c"

#include "sig/aigis-sig/std/api.h"
#include "kemsig.h"

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

#ifdef AIGIS_SIG_LOWRAM
/* suites/aigis_sig_lowram: the "opt" variant is the low-stack code */
#include "aigis_sig_lowram.inc"
#define PQM_OPT_OPS &pqm_lowram_ops
#else
#define PQM_OPT_OPS 0
#endif

const sig_alg PQM_CAT(PQM_NS, _alg) = {
    PQM_NAME, SIG_PUBLICKEYBYTES, SIG_SECRETKEYBYTES, SIG_BYTES,
    {crypto_sign_keypair, pqm_sign, pqm_verify}, PQM_OPT_OPS, PQM_STACK, 5, 10, 5};
