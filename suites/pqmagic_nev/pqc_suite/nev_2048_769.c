#define PARAMS 12
#ifdef PQM_HASH_SM3
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_2048_769_sm3_
#define PQM_ALG pqm_nev_2048_769_sm3_alg
#define PQM_NAME "nev-2048-769-sm3"
#else
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_2048_769_
#define PQM_ALG pqm_nev_2048_769_alg
#define PQM_NAME "nev-2048-769"
#endif
#include "nev_unity.inc"
