/* One parameter set of HQC as built for the ESP32-C3: the upstream reference
 * sources, except for
 *   ref/hqc.c, ref/gf2x.c    -> hqc_c3_pke.c   sparse polynomial products
 *   ref/gf.c                 -> hqc_c3_gf.c    GF(2^8) with the multiplier
 *   ref/reed_muller.c        -> hqc_c3_rm.c    Reed-Muller decoder
 *   common/symmetric.c       -> hqc_c3_sym.c   word-wise SHAKE/SHA-3 sponge
 * The Reed-Solomon coder, the sampling, the parsing and the KEM layer are
 * the upstream files.
 * Define HQC_LEVEL (1, 3 or 5) and HQC_NS(x) first. See hqc_unit_ref.inc
 * for how the per-level headers are selected. */
#include "hqc_rename.h"
#define hqc_c3_probe_mul HQC_NS(c3_probe_mul)

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
#include "hqc_c3_sym.c"
#include "hqc_c3_gf.c"
#include "hqc_c3_rm.c"
#include "ref/reed_solomon.c"
#include "common/code.c"
#include "ref/vector.c"
#include "ref/parsing.c"
#include "hqc_c3_pke.c"
#include "common/hqc-kem.c"
