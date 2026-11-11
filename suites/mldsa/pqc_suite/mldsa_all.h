/* Public API of all three ML-DSA parameter sets of the "ref" build:
 * mldsa44_*, mldsa65_*, mldsa87_*. */
#ifndef MLDSA_ALL_H
#define MLDSA_ALL_H

#define MLD_CONFIG_NAMESPACE_PREFIX mldsa

#define MLD_CONFIG_PARAMETER_SET 44
#include <mldsa_native.h>
#undef MLD_CONFIG_PARAMETER_SET
#undef MLD_H

#define MLD_CONFIG_PARAMETER_SET 65
#include <mldsa_native.h>
#undef MLD_CONFIG_PARAMETER_SET
#undef MLD_H

#define MLD_CONFIG_PARAMETER_SET 87
#include <mldsa_native.h>
#undef MLD_CONFIG_PARAMETER_SET
#undef MLD_H

#endif
