/* One SCloud+ level and hash family (reference tier) as a single compilation
 * unit. The including file defines PQM_SC_LEVEL, one of PQM_SC_SM3 /
 * PQM_SC_SHAKE / PQM_SC_AES, PQM_NS (symbol prefix), PQM_NAME (job name) and
 * PQM_STACK. Sources come from downloads/scloudplus (see tools/fetch). */
#define SCLOUDPLUS_TIER_REFERENCE
#if defined(PQM_SC_SM3)
#define SCLOUDPLUS_FAMILY_SM3
#define SCLOUDPLUS_REF_FAMILY_SM3
#elif defined(PQM_SC_SHAKE)
#define SCLOUDPLUS_FAMILY_SHAKE
#define SCLOUDPLUS_REF_FAMILY_SHAKE
#else
#define SCLOUDPLUS_FAMILY_AES
#define SCLOUDPLUS_REF_FAMILY_AES
#endif
#ifndef PQM_UPSTREAM_HASH /* host cross-check with SCloud+'s own hash code */
#define PQC_PRIMS_KECCAK
#define PQC_PRIMS_SM3
#endif

#include "sc_rename.h"
#define randombytes sc_randombytes

#include "_shared/scloudplus_core/common/encode.c"
#include "_shared/scloudplus_core/common/kem.c"

/* The two large matrix products and the AES counter-block function keep
 * their upstream code under the names *_upstream; sc_opt.inc defines the
 * functions the rest of the code calls and chooses between the two. */
#include "matrix.h"
#include "aes.h"
#undef mul_as_e
#undef mul_sa_e
#define mul_as_e SC_NAME(mul_as_e_upstream)
#define mul_sa_e SC_NAME(mul_sa_e_upstream)
#include "_shared/scloudplus_core/ref/matrix_reference.c"
#undef mul_as_e
#undef mul_sa_e
#define mul_as_e SC_NAME(mul_as_e)
#define mul_sa_e SC_NAME(mul_sa_e)
#if defined(PQM_SC_AES)
#undef AES128_CTR_zero_sch
#define AES128_CTR_zero_sch SC_NAME(AES128_CTR_zero_sch_upstream)
#include "_shared/scloudplus_core/ref/aes_reference.c"
#undef AES128_CTR_zero_sch
#define AES128_CTR_zero_sch SC_NAME(AES128_CTR_zero_sch)
#endif
#include "_shared/scloudplus_core/ref/pack_reference.c"
#include "_shared/scloudplus_core/common/pke.c"
#include "_shared/scloudplus_core/common/sample.c"
#include "_shared/scloudplus_core/common/util.c"
#if defined(PQM_SC_SM3)
#undef sm3_bounded_xof_seed_row
#define sm3_bounded_xof_seed_row SC_NAME(sm3_bounded_xof_seed_row_upstream)
#include "_shared/scloudplus_core/common/hash_sm3_portable.c"
#undef sm3_bounded_xof_seed_row
#define sm3_bounded_xof_seed_row SC_NAME(sm3_bounded_xof_seed_row)
#ifdef PQM_UPSTREAM_HASH
#include "_shared/scloudplus_core/ref/sm3_reference.c"
#else
#include "sm3_reference.c" /* downloads/scloudplus_port */
#endif
#else
#ifdef PQM_UPSTREAM_HASH
#include "_shared/scloudplus_core/common/hash_aes_shake.c"
#else
#include "hash_aes_shake.c" /* downloads/scloudplus_port */
#endif
#endif

#include "sc_opt.inc"

#include "kemsig.h"

static int pqm_keypair(unsigned char *pk, unsigned char *sk) { return scloud_kemkeygen(pk, sk); }
static int pqm_enc(unsigned char *ct, unsigned char *ss, const unsigned char *pk)
{
    return scloud_kemencaps(pk, ct, ss);
}
static int pqm_dec(unsigned char *ss, const unsigned char *ct, const unsigned char *sk)
{
    return scloud_kemdecaps(sk, ct, ss);
}

const kem_alg SC_CAT(PQM_NS, _alg) = {
    PQM_NAME, scloudplus_pk, scloudplus_kem_sk, scloudplus_ctx, scloudplus_ss,
    {pqm_keypair, pqm_enc, pqm_dec}, 0, PQM_STACK, PQM_ITERS};
