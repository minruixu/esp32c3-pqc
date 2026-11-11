/* SPHINCS-alpha (PQMagic): the parameter sets chosen by the build. */
#include "bench.h"
#include "kemsig.h"

#ifdef SPX_HAVE_sm3_128f
extern const sig_alg pqm_sphincsa_sm3_128f_alg;
#endif
#ifdef SPX_HAVE_sm3_128s
extern const sig_alg pqm_sphincsa_sm3_128s_alg;
#endif
#ifdef SPX_HAVE_sha2_128f
extern const sig_alg pqm_sphincsa_sha2_128f_alg;
#endif
#ifdef SPX_HAVE_sha2_128s
extern const sig_alg pqm_sphincsa_sha2_128s_alg;
#endif
#ifdef SPX_HAVE_sha2_192f
extern const sig_alg pqm_sphincsa_sha2_192f_alg;
#endif
#ifdef SPX_HAVE_sha2_192s
extern const sig_alg pqm_sphincsa_sha2_192s_alg;
#endif
#ifdef SPX_HAVE_sha2_256f
extern const sig_alg pqm_sphincsa_sha2_256f_alg;
#endif
#ifdef SPX_HAVE_sha2_256s
extern const sig_alg pqm_sphincsa_sha2_256s_alg;
#endif
#ifdef SPX_HAVE_shake_128f
extern const sig_alg pqm_sphincsa_shake_128f_alg;
#endif
#ifdef SPX_HAVE_shake_128s
extern const sig_alg pqm_sphincsa_shake_128s_alg;
#endif
#ifdef SPX_HAVE_shake_192f
extern const sig_alg pqm_sphincsa_shake_192f_alg;
#endif
#ifdef SPX_HAVE_shake_192s
extern const sig_alg pqm_sphincsa_shake_192s_alg;
#endif
#ifdef SPX_HAVE_shake_256f
extern const sig_alg pqm_sphincsa_shake_256f_alg;
#endif
#ifdef SPX_HAVE_shake_256s
extern const sig_alg pqm_sphincsa_shake_256s_alg;
#endif

static const sig_alg *const algs[] = {
#ifdef SPX_HAVE_sm3_128f
    &pqm_sphincsa_sm3_128f_alg,
#endif
#ifdef SPX_HAVE_sm3_128s
    &pqm_sphincsa_sm3_128s_alg,
#endif
#ifdef SPX_HAVE_sha2_128f
    &pqm_sphincsa_sha2_128f_alg,
#endif
#ifdef SPX_HAVE_sha2_128s
    &pqm_sphincsa_sha2_128s_alg,
#endif
#ifdef SPX_HAVE_sha2_192f
    &pqm_sphincsa_sha2_192f_alg,
#endif
#ifdef SPX_HAVE_sha2_192s
    &pqm_sphincsa_sha2_192s_alg,
#endif
#ifdef SPX_HAVE_sha2_256f
    &pqm_sphincsa_sha2_256f_alg,
#endif
#ifdef SPX_HAVE_sha2_256s
    &pqm_sphincsa_sha2_256s_alg,
#endif
#ifdef SPX_HAVE_shake_128f
    &pqm_sphincsa_shake_128f_alg,
#endif
#ifdef SPX_HAVE_shake_128s
    &pqm_sphincsa_shake_128s_alg,
#endif
#ifdef SPX_HAVE_shake_192f
    &pqm_sphincsa_shake_192f_alg,
#endif
#ifdef SPX_HAVE_shake_192s
    &pqm_sphincsa_shake_192s_alg,
#endif
#ifdef SPX_HAVE_shake_256f
    &pqm_sphincsa_shake_256f_alg,
#endif
#ifdef SPX_HAVE_shake_256s
    &pqm_sphincsa_shake_256s_alg,
#endif
};

void suite_main(void)
{
    sig_run(algs, sizeof(algs) / sizeof(algs[0]));
}
