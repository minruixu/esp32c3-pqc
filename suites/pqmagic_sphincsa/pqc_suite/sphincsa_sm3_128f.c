#define SPHINCS_A_MODE sphincs-a-sm3-128f
#define SPHINCS_A_HASH_MODE_NAMESPACE sm3_128f
#define THASH simple
#define PQM_HASH_SM3
#define PQM_NS pqm_sphincsa_sm3_128f
#define PQM_NAME "sphincs-a-sm3-128f"
#define PQM_ITERS_KEYGEN 3
#define PQM_ITERS_SIGN 2
#include "sphincsa_unity.inc"
