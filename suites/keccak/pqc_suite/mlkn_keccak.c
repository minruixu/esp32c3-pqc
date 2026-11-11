/* mlkem-native's portable C Keccak-f[1600], built on its own for comparison. */
#define MLK_CONFIG_NAMESPACE_PREFIX kbench
#include "src/fips202/keccakf1600.c"

void kbench_mlkn_permute(uint64_t *state) { mlk_keccakf1600_permute_c(state); }
