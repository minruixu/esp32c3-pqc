/* One parameter set of the upstream HQC reference implementation as a single
 * compilation unit. Define HQC_LEVEL (1, 3 or 5) and HQC_NS(x) first.
 *
 * The upstream sources include "parameters.h", "hqc-kem.h" and
 * "reed_solomon.h" by name and expect the build system to put the right
 * per-level directory on the include path. Here the per-level headers are
 * included explicitly first; their include guards then make the later
 * by-name includes (which resolve to the level-1 files) do nothing. */
#include "hqc_rename.h"

#if HQC_LEVEL == 1
#include "common/hqc-1/hqc-kem.h"
#include "ref/hqc-1/parameters.h"
#include "ref/hqc-1/reed_solomon.h"
#elif HQC_LEVEL == 3
#include "common/hqc-3/hqc-kem.h"
#include "ref/hqc-3/parameters.h"
#include "ref/hqc-3/reed_solomon.h"
#elif HQC_LEVEL == 5
#include "common/hqc-5/hqc-kem.h"
#include "ref/hqc-5/parameters.h"
#include "ref/hqc-5/reed_solomon.h"
#else
#error "HQC_LEVEL must be 1, 3 or 5"
#endif

/* Upstream defines BITMASK with `1UL`, which is 32 bits wide on this target.
 * For HQC-5 the shift count is 37 (57637 mod 64), so the mask of the last
 * word of a polynomial comes out wrong on any 32-bit machine. Same macro
 * with a 64-bit one; on 64-bit machines nothing changes. */
#undef BITMASK
#define BITMASK(a, size) ((UINT64_C(1) << ((a) % (size))) - 1)

#include "common/crypto_memset.c"
#include "common/symmetric.c"
#include "ref/gf.c"
#include "ref/gf2x.c"
#include "ref/reed_muller.c"
#include "ref/reed_solomon.c"
#include "common/code.c"
#include "ref/vector.c"
#include "ref/parsing.c"
#include "ref/hqc.c"
#include "common/hqc-kem.c"
