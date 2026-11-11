#define SLH_DSA_MODE slh-dsa-sm3-128s
#define SLH_DSA_HASH_MODE_NAMESPACE sm3_128s
#define THASH robust
#define PQM_THASH_ROBUST
#define PQM_NS pqm_slhdsa_sm3_128s_robust
#define PQM_NAME "slhdsa-sm3-128s-robust-pqmagic"
#define PQM_ITERS_KEYGEN 1
#define PQM_ITERS_SIGN 1
#include "slhdsa_unity.inc"
