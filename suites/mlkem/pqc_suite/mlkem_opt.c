/* Variant "opt": the same mlkem-native sources, built a second time with the
 * ESP32-C3 code of this project plugged into its configuration hooks:
 *   - FIPS 202 from c3_fips202_mlkem.h: a word-oriented sponge on top of the
 *     Keccak-f[1600] of components/prims (assembly on the device), used
 *     serially (no four-way batching);
 *   - NTT, inverse NTT, base multiplication, reduction, Montgomery
 *     conversion and rejection sampling in assembly
 *     (mlkem_arith_rv32imc.S), device only.
 * Symbols: mlkemopt512_*, mlkemopt768_*, mlkemopt1024_*.
 * On the build machine the arithmetic is the portable C code and the Keccak
 * permutation is the reference one, so the host run checks the same paths. */
#define MLK_CONFIG_NAMESPACE_PREFIX mlkemopt

#define MLK_CONFIG_SERIAL_FIPS202_ONLY
#define MLK_CONFIG_FIPS202_CUSTOM_HEADER "c3_fips202_mlkem.h"
#if !defined(PQC_HOST)
#define MLK_CONFIG_USE_NATIVE_BACKEND_ARITH
#define MLK_CONFIG_ARITH_BACKEND_FILE "c3_arith_backend.h"
#endif

#define MLK_CONFIG_MULTILEVEL_WITH_SHARED
#define MLK_CONFIG_MONOBUILD_KEEP_SHARED_HEADERS
#define MLK_CONFIG_PARAMETER_SET 512
#include "mlkem_native.c"
#undef MLK_CONFIG_PARAMETER_SET
#undef MLK_CONFIG_MULTILEVEL_WITH_SHARED

#define MLK_CONFIG_MULTILEVEL_NO_SHARED
#define MLK_CONFIG_PARAMETER_SET 768
#include "mlkem_native.c"
#undef MLK_CONFIG_PARAMETER_SET
#undef MLK_CONFIG_MONOBUILD_KEEP_SHARED_HEADERS

#define MLK_CONFIG_PARAMETER_SET 1024
#include "mlkem_native.c"
#undef MLK_CONFIG_PARAMETER_SET
