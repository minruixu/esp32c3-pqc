/* PQMagic's fips202.c with its Keccak-f[1600] replaced by the project's
 * shared one (see the PQC_PRIMS_KECCAK block in the vendored file). */
#define PQC_PRIMS_KECCAK
#include "hash/keccak/fips202.c"
