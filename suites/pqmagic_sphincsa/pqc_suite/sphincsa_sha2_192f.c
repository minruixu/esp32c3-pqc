#define SPHINCS_A_MODE sphincs-a-sha2-192f
#define SPHINCS_A_HASH_MODE_NAMESPACE sha2_192f
#define THASH simple
#define PQM_HASH_SHA2
#define PQM_NS pqm_sphincsa_sha2_192f
#define PQM_NAME "sphincs-a-sha2-192f"
#define PQM_ITERS_KEYGEN 3
#define PQM_ITERS_SIGN 2
/* The 128 KB table leaves no room for the default 20 KB job stack. */
#define PQM_STACK (10 * 1024)
#include "sphincsa_unity.inc"
