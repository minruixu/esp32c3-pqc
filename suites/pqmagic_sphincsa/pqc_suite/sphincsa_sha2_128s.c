#define SPHINCS_A_MODE sphincs-a-sha2-128s
#define SPHINCS_A_HASH_MODE_NAMESPACE sha2_128s
#define THASH simple
#define PQM_HASH_SHA2
#define PQM_NS pqm_sphincsa_sha2_128s
#define PQM_NAME "sphincs-a-sha2-128s"
#define PQM_ITERS_KEYGEN 1
#define PQM_ITERS_SIGN 1
#include "sphincsa_unity.inc"
