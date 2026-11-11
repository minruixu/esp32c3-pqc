#define PARAMS 8
#ifdef PQM_HASH_SM3
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_1024_3329_sm3_
#define PQM_ALG pqm_nev_1024_3329_sm3_alg
#define PQM_NAME "nev-1024-3329-sm3"
#else
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_1024_3329_
#define PQM_ALG pqm_nev_1024_3329_alg
#define PQM_NAME "nev-1024-3329"
#endif
#include "nev_unity.inc"
