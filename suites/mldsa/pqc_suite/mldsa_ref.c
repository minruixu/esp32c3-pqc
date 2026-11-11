/* Variant "ref": mldsa-native exactly as upstream ships it (portable C, its
 * own Keccak), all three parameter sets in one compilation unit as in its
 * examples/monolithic_build_multilevel. Symbols: mldsa44_*, mldsa65_*,
 * mldsa87_*. */
#if defined(PQC_MLDSA_ONLY_87)
#include "mldsa_heap_alloc.h"
#endif
#include "mldsa_all.h"

#if defined(PQC_MLDSA_ONLY_87)

#define MLD_CONFIG_MULTILEVEL_WITH_SHARED
#define MLD_CONFIG_PARAMETER_SET 87
#include "mldsa_native.c"
#undef MLD_CONFIG_PARAMETER_SET

#else /* all three parameter sets */

#define MLD_CONFIG_MULTILEVEL_WITH_SHARED
#define MLD_CONFIG_MONOBUILD_KEEP_SHARED_HEADERS
#define MLD_CONFIG_PARAMETER_SET 44
#include "mldsa_native.c"
#undef MLD_CONFIG_PARAMETER_SET
#undef MLD_CONFIG_MULTILEVEL_WITH_SHARED

#define MLD_CONFIG_MULTILEVEL_NO_SHARED
#define MLD_CONFIG_PARAMETER_SET 65
#include "mldsa_native.c"
#undef MLD_CONFIG_PARAMETER_SET
#undef MLD_CONFIG_MONOBUILD_KEEP_SHARED_HEADERS

#define MLD_CONFIG_PARAMETER_SET 87
#include "mldsa_native.c"
#undef MLD_CONFIG_PARAMETER_SET

#endif
