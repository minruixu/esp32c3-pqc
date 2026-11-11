#define SPHINCS_A_MODE sphincs-a-sha2-256s
#define SPHINCS_A_HASH_MODE_NAMESPACE sha2_256s
#define THASH simple
#define PQM_HASH_SHA2
#define PQM_NS pqm_sphincsa_sha2_256s
#define PQM_NAME "sphincs-a-sha2-256s"
#define PQM_ITERS_KEYGEN 1
#define PQM_ITERS_SIGN 1
#include "sphincsa_unity.inc"
