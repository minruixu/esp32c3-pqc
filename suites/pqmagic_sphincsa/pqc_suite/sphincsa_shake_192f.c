#define SPHINCS_A_MODE sphincs-a-shake-192f
#define SPHINCS_A_HASH_MODE_NAMESPACE shake_192f
#define THASH simple
#define PQM_HASH_SHAKE
#define PQM_NS pqm_sphincsa_shake_192f
#define PQM_NAME "sphincs-a-shake-192f"
#define PQM_ITERS_KEYGEN 3
#define PQM_ITERS_SIGN 2
#include "sphincsa_unity.inc"
