/* Assembly of mldsa-native's RV32IM backend for the "opt" build. */
#include "mldsa_opt_config.h"
#define MLD_CONFIG_MULTILEVEL_WITH_SHARED
#if defined(PQC_MLDSA_ONLY_87)
#define MLD_CONFIG_PARAMETER_SET 87
#else
#define MLD_CONFIG_PARAMETER_SET 44
#endif
#include "mldsa_native_asm.S"
