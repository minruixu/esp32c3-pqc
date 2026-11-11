#define PARAMS 1
#ifdef PQM_HASH_SM3
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_512_769_c_sm3_
#define PQM_ALG pqm_nev_512_769_c_sm3_alg
#define PQM_NAME "nev-512-769-c-sm3"
#else
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_512_769_c_
#define PQM_ALG pqm_nev_512_769_c_alg
#define PQM_NAME "nev-512-769-c"
#endif
#include "nev_unity.inc"
