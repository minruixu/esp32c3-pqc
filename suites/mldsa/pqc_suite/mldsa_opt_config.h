/* Configuration of the "opt" build of mldsa-native, shared by the C and the
 * assembly compilation units:
 *   - FIPS 202 from c3_fips202_mldsa.h: a word-oriented sponge on top of the
 *     Keccak-f[1600] of components/prims (assembly on the device), used
 *     serially (no four-way batching);
 *   - the arithmetic backend of c3_mldsa_backend.h, device only: NTT,
 *     inverse NTT and rejection sampling from this project, pointwise
 *     multiplication from mldsa-native's own RV32IM assembly.
 * PQC_MLDSA_LOWRAM additionally selects MLD_CONFIG_REDUCE_RAM. */
#ifndef PQC_MLDSA_OPT_CONFIG_H
#define PQC_MLDSA_OPT_CONFIG_H

#if defined(PQC_MLDSA_ONLY_87) && !defined(PQC_MLDSA_LOWRAM)
#include "mldsa_heap_alloc.h"
#endif

#if defined(PQC_MLDSA_LOWRAM)
#define MLD_CONFIG_NAMESPACE_PREFIX mldsalr
#define MLD_CONFIG_REDUCE_RAM
#else
#define MLD_CONFIG_NAMESPACE_PREFIX mldsaopt
#endif

#define MLD_CONFIG_SERIAL_FIPS202_ONLY
#define MLD_CONFIG_FIPS202_CUSTOM_HEADER "c3_fips202_mldsa.h"
#if !defined(PQC_HOST)
#define MLD_CONFIG_USE_NATIVE_BACKEND_ARITH
#define MLD_CONFIG_ARITH_BACKEND_FILE "c3_mldsa_backend.h"
#endif

#endif /* PQC_MLDSA_OPT_CONFIG_H */
