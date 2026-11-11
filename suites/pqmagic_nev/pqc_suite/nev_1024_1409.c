#define PARAMS 5
#ifdef PQM_HASH_SM3
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_1024_1409_sm3_
#define PQM_ALG pqm_nev_1024_1409_sm3_alg
#define PQM_NAME "nev-1024-1409-sm3"
#else
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_1024_1409_
#define PQM_ALG pqm_nev_1024_1409_alg
#define PQM_NAME "nev-1024-1409"
#endif
#include "nev_unity.inc"
