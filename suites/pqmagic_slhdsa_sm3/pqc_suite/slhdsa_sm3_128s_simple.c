#define SLH_DSA_MODE slh-dsa-sm3-128s
#define SLH_DSA_HASH_MODE_NAMESPACE sm3_128s
#define THASH simple
#define PQM_NS pqm_slhdsa_sm3_128s_simple
#define PQM_NAME "slhdsa-sm3-128s-pqmagic"
#define PQM_ITERS_KEYGEN 1
#define PQM_ITERS_SIGN 1
#include "slhdsa_unity.inc"
