#define SPHINCS_A_MODE sphincs-a-shake-128f
#define SPHINCS_A_HASH_MODE_NAMESPACE shake_128f
#define THASH simple
#define PQM_HASH_SHAKE
#define PQM_NS pqm_sphincsa_shake_128f
#define PQM_NAME "sphincs-a-shake-128f"
#define PQM_ITERS_KEYGEN 3
#define PQM_ITERS_SIGN 2
#include "sphincsa_unity.inc"
