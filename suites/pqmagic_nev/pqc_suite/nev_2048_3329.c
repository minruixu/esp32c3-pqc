#define PARAMS 9
#ifdef PQM_HASH_SM3
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_2048_3329_sm3_
#define PQM_ALG pqm_nev_2048_3329_sm3_alg
#define PQM_NAME "nev-2048-3329-sm3"
#else
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_2048_3329_
#define PQM_ALG pqm_nev_2048_3329_alg
#define PQM_NAME "nev-2048-3329"
#endif
#include "nev_unity.inc"
