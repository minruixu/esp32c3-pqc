/* Variant "opt" (or "lowram"): the same mldsa-native sources built with the
 * configuration in mldsa_opt_config.h. Symbols: mldsaopt44_* and so on. */
#include "mldsa_opt_config.h"

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
