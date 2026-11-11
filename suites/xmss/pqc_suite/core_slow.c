/* xmss_core.c of the reference code (no traversal state: every signature
 * recomputes its tree), under other names. Only the host build calls it, to
 * reproduce the signature test vectors of the reference implementation. */
#define xmss_xmssmt_core_sk_bytes xmss_slow_core_sk_bytes
#define xmss_core_keypair xmss_slow_core_keypair
#define xmss_core_sign xmss_slow_core_sign
#define xmssmt_core_seed_keypair xmssmt_slow_core_seed_keypair
#define xmssmt_core_keypair xmssmt_slow_core_keypair
#define xmssmt_core_sign xmssmt_slow_core_sign
#include "xmss_core.c"
