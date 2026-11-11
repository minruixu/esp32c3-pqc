/* GF(2^8) arithmetic for the Reed-Solomon code of HQC on the ESP32-C3.
 * Included by hqc_unit_opt.inc in place of the reference ref/gf.c; it
 * defines gf_mul, gf_square and gf_inverse with the same results.
 *
 * The reference gf_mul builds the carry-less product with a constant-time
 * 2-bit window and reduces it in a loop: about 300 cycles on this core. The
 * processor's 32x32 -> 64-bit multiplier can do the carry-less product in
 * one step if the bits of both factors are first spread out to every fourth
 * position: column i+j of the integer product then holds the *number* of
 * pairs with a_i = b_j = 1, at most 8, which fits in its four bits, and the
 * lowest bit of each column is the carry-less product bit. The reduction
 * modulo x^8 + x^4 + x^3 + x + 1 is done on the spread form with shifts.
 * No table, no branch, no dependence of the instruction sequence on the
 * operands. */
#include <stdint.h>
#include "gf.h"

/* bit i -> bit 4i, for an 8-bit value */
static inline __attribute__((always_inline)) uint32_t c3_gf_spread(uint32_t x)
{
    x = (x | (x << 12)) & 0x000F000Fu;
    x = (x | (x << 6)) & 0x03030303u;
    x = (x | (x << 3)) & 0x11111111u;
    return x;
}

/* bit 4i -> bit i */
static inline __attribute__((always_inline)) uint32_t c3_gf_unspread(uint32_t x)
{
    x = (x | (x >> 3)) & 0x03030303u;
    x = (x | (x >> 6)) & 0x000F000Fu;
    x = (x | (x >> 12)) & 0xFFu;
    return x;
}

inline __attribute__((always_inline)) uint16_t gf_mul(uint16_t a, uint16_t b)
{
    uint64_t p = (uint64_t)c3_gf_spread(a & 0xFF) * c3_gf_spread(b & 0xFF);
    uint32_t lo = (uint32_t)p & 0x11111111u;         /* degrees 0..7 */
    uint32_t hi = (uint32_t)(p >> 32) & 0x11111111u; /* degrees 8..14 */

    /* x^8 = x^4 + x^3 + x + 1. Multiplying by x^j is a shift by 4j bits;
       what leaves the 32-bit word is of degree 8 to 10 and is folded once
       more. */
    uint32_t over = (hi >> 16) ^ (hi >> 20);
    lo ^= hi ^ (hi << 4) ^ (hi << 12) ^ (hi << 16);
    lo ^= over ^ (over << 4) ^ (over << 12) ^ (over << 16);
    return (uint16_t)c3_gf_unspread(lo);
}

uint16_t gf_square(uint16_t a)
{
    return gf_mul(a, a);
}

uint16_t gf_inverse(uint16_t a)
{
    uint16_t inv, tmp1, tmp2;

    inv = gf_square(a);       /* a^2 */
    tmp1 = gf_mul(inv, a);    /* a^3 */
    inv = gf_square(inv);     /* a^4 */
    tmp2 = gf_mul(inv, tmp1); /* a^7 */
    tmp1 = gf_mul(inv, tmp2); /* a^11 */
    inv = gf_mul(tmp1, inv);  /* a^15 */
    inv = gf_square(inv);     /* a^30 */
    inv = gf_square(inv);     /* a^60 */
    inv = gf_square(inv);     /* a^120 */
    inv = gf_mul(inv, tmp2);  /* a^127 */
    inv = gf_square(inv);     /* a^254 */
    return inv;
}
