#define SPHINCS_A_MODE sphincs-a-sha2-128f
#define SPHINCS_A_HASH_MODE_NAMESPACE sha2_128f
#define THASH simple
#define PQM_HASH_SHA2
#define PQM_NS pqm_sphincsa_sha2_128f
#define PQM_NAME "sphincs-a-sha2-128f"
#define PQM_ITERS_KEYGEN 3
#define PQM_ITERS_SIGN 2
#include "sphincsa_unity.inc"
