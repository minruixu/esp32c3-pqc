#define SPHINCS_A_MODE sphincs-a-sm3-128s
#define SPHINCS_A_HASH_MODE_NAMESPACE sm3_128s
#define THASH simple
#define PQM_HASH_SM3
#define PQM_NS pqm_sphincsa_sm3_128s
#define PQM_NAME "sphincs-a-sm3-128s"
#define PQM_ITERS_KEYGEN 1
#define PQM_ITERS_SIGN 1
#include "sphincsa_unity.inc"
