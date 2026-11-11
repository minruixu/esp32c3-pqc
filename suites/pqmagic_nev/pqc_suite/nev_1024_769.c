#define PARAMS 11
#ifdef PQM_HASH_SM3
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_1024_769_sm3_
#define PQM_ALG pqm_nev_1024_769_sm3_alg
#define PQM_NAME "nev-1024-769-sm3"
#else
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_1024_769_
#define PQM_ALG pqm_nev_1024_769_alg
#define PQM_NAME "nev-1024-769"
#endif
#include "nev_unity.inc"
