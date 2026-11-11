#define SLH_DSA_MODE slh-dsa-sm3-128f
#define SLH_DSA_HASH_MODE_NAMESPACE sm3_128f
#define THASH simple
#define PQM_NS pqm_slhdsa_sm3_128f_simple
#define PQM_NAME "slhdsa-sm3-128f-pqmagic"
#define PQM_ITERS_KEYGEN 3
#define PQM_ITERS_SIGN 2
#include "slhdsa_unity.inc"
