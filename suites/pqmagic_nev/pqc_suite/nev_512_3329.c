#define PARAMS 7
#ifdef PQM_HASH_SM3
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_512_3329_sm3_
#define PQM_ALG pqm_nev_512_3329_sm3_alg
#define PQM_NAME "nev-512-3329-sm3"
#else
#define PQMAGIC_NAMESPACE_PREFIX pqm_nev_512_3329_
#define PQM_ALG pqm_nev_512_3329_alg
#define PQM_NAME "nev-512-3329"
#endif
#include "nev_unity.inc"
