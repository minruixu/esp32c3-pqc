#define SPHINCS_A_MODE sphincs-a-shake-192f
#define SPHINCS_A_HASH_MODE_NAMESPACE shake_192f
#define THASH simple
#define PQM_HASH_SHAKE
#define PQM_NS pqm_sphincsa_shake_192f
#define PQM_NAME "sphincs-a-shake-192f"
#define PQM_ITERS_KEYGEN 3
#define PQM_ITERS_SIGN 2
/* The 128 KB table leaves no room for the default 20 KB job stack. */
#define PQM_STACK (10 * 1024)
#include "sphincsa_unity.inc"
