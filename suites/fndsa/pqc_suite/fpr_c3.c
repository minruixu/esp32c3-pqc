/* Emulated IEEE-754 double addition and multiplication for a 32-bit core
 * without FPU and without bit-manipulation instructions (ESP32-C3, RV32IMC).
 *
 * Included by fndsa_sign_c3.c after sign_inner.h. It defines fpr_add() and
 * fpr_mul() with the same results, bit for bit, as c-fn-dsa's sign_fpr.c,
 * plus fpr_add_sub(), which computes x + y and x - y together.
 *
 * c-fn-dsa's versions are written with 64-bit integers. On a 32-bit core
 * every 64-bit shift by a variable count, every 64-bit comparison and the
 * 64-bit leading-zero count turn into long sequences, because they must stay
 * free of secret-dependent branches. The versions here do the same
 * computation on 32-bit halves directly:
 *   - the operands are ordered by magnitude with two 32-bit comparisons;
 *   - the alignment shift first moves by a whole word (a masked select),
 *     then by 0..31 bits;
 *   - leading zeros are counted on the one 32-bit word that holds the top
 *     bit, then the 64-bit value is shifted once;
 *   - the 53x53-bit product is built from four 32x32-bit products.
 * Like the originals they contain no branch and no table lookup that depends
 * on the operands (check: no conditional branch in the disassembly).
 *
 * x + y and x - y share the ordering, unpacking and alignment of the
 * operands; only the add/subtract, normalization and rounding are done
 * twice. The FFT butterflies of signing need exactly such pairs. */
#include <stdint.h>

/* Hides the value of a mask from the optimizer. Without it GCC recognizes
   "(a ^ b) & mask" selections, where mask is 0 or all ones, and compiles
   them to a conditional branch around a move (RV32IMC has no conditional
   move), which would make the running time depend on the operands. */
#if defined(__GNUC__) || defined(__clang__)
#define C3_OPAQUE(v)   __asm__("" : "+r"(v))
#else
#define C3_OPAQUE(v)   ((void)0)
#endif

/* Operands of an addition after ordering and alignment. */
typedef struct {
    uint32_t xl, xh; /* mantissa of the larger operand, times 8 (bit 55 set unless zero) */
    uint32_t yl, yh; /* mantissa of the smaller one, times 8, aligned, lowest bit sticky */
    uint32_t ex;     /* exponent field of the larger operand */
    uint32_t sx;     /* high word of the larger operand: its sign is bit 31 */
    uint32_t dm;     /* all ones if x and y have different signs, else 0 */
    uint32_t sw;     /* 1 if the operands were exchanged, else 0 */
} c3_fpr_ops;

static inline __attribute__((always_inline)) void
c3_fpr_prepare(c3_fpr_ops *o, uint64_t x, uint64_t y)
{
    uint32_t xl = (uint32_t)x, xh = (uint32_t)(x >> 32);
    uint32_t yl = (uint32_t)y, yh = (uint32_t)(y >> 32);

    /* Exchange x and y if |x| < |y|, or if |x| = |y| and x is negative
       (so that z + (-z) is +0). */
    uint32_t xa = xh << 1, ya = yh << 1;
    uint32_t hlt = (uint32_t)(xa < ya);
    uint32_t heq = (uint32_t)(xa == ya);
    uint32_t llt = (uint32_t)(xl < yl);
    uint32_t leq = (uint32_t)(xl == yl);
    uint32_t sw = hlt | (heq & (llt | (leq & (xh >> 31))));
    uint32_t m = 0u - sw;
    uint32_t t;
    C3_OPAQUE(m);

    o->dm = (uint32_t)((int32_t)(xh ^ yh) >> 31);
    o->sw = sw;
    t = (xl ^ yl) & m; xl ^= t; yl ^= t;
    t = (xh ^ yh) & m; xh ^= t; yh ^= t;

    /* Exponent fields and mantissas. The mantissa gets its implicit bit
       unless the exponent field is 0 (the value is then a zero), and is
       shifted left by 3 bits to make room for guard bits. */
    uint32_t ex = (xh >> 20) & 0x7FF;
    uint32_t ey = (yh >> 20) & 0x7FF;
    uint32_t xmh = ((xh << 12) >> 9) | (xl >> 29) | ((uint32_t)(ex != 0) << 23);
    uint32_t xml = xl << 3;
    uint32_t ymh = ((yh << 12) >> 9) | (yl >> 29) | ((uint32_t)(ey != 0) << 23);
    uint32_t yml = yl << 3;

    /* Shift y right by n = ex - ey bits; every bit shifted out is OR-ed
       into the lowest bit. From n = 60 on, y cannot change the result and
       is replaced with 0. */
    uint32_t n = ex - ey;
    uint32_t keep = (uint32_t)((int32_t)(n - 60) >> 31);
    C3_OPAQUE(keep);
    yml &= keep;
    ymh &= keep;

    uint32_t big = (n >> 5) & 1;     /* shift by a whole word first */
    uint32_t mb = 0u - big;
    C3_OPAQUE(mb);
    uint32_t nb = ~mb;
    uint32_t lost = yml & mb;
    yml ^= (yml ^ ymh) & mb;
    ymh &= nb;

    uint32_t s = n & 31;             /* then by 0..31 bits */
    lost |= (yml << 1) << (31 - s);
    yml = (yml >> s) | ((ymh << 1) << (31 - s));
    ymh >>= s;
    yml |= (uint32_t)(lost != 0);

    o->xl = xml; o->xh = xmh;
    o->yl = yml; o->yh = ymh;
    o->ex = ex;
    o->sx = xh;
}

/* x + y (dm = 0) or x - y (dm = all ones) on aligned mantissas, then
   normalization, rounding to nearest even, and packing. sign: bit 31. */
static inline __attribute__((always_inline)) uint64_t
c3_fpr_finish(const c3_fpr_ops *o, uint32_t dm, uint32_t sign)
{
    /* z = x + y, or x + ~y + 1. */
    uint32_t yl = o->yl ^ dm, yh = o->yh ^ dm;
    uint32_t cin = dm & 1;
    uint32_t zl = o->xl + yl;
    uint32_t c1 = (uint32_t)(zl < yl);
    uint32_t zl2 = zl + cin;
    uint32_t c2 = (uint32_t)(zl2 < cin);
    uint32_t zh = o->xh + yh + c1 + c2;
    zl = zl2;

    /* Normalize: move the top bit of z to bit 63. If the high word is 0,
       move the low word up first; then count the leading zeros of the
       high word with a binary search and shift once. */
    uint32_t hz = (uint32_t)(zh == 0);
    uint32_t mh = 0u - hz;
    C3_OPAQUE(mh);
    zh |= zl & mh;
    zl &= ~mh;
    uint32_t c = hz << 5;

    uint32_t v = zh, k = 0, z;
    z = (uint32_t)((v >> 16) == 0) << 4; v <<= z; k += z;
    z = (uint32_t)((v >> 24) == 0) << 3; v <<= z; k += z;
    z = (uint32_t)((v >> 28) == 0) << 2; v <<= z; k += z;
    z = (uint32_t)((v >> 30) == 0) << 1; v <<= z; k += z;
    z = (uint32_t)((v >> 31) == 0);      v <<= z; k += z;
    zh = v | ((zl >> 1) >> (31 - k));
    zl <<= k;
    c += k;

    /* z is now in [2^63, 2^64), or 0. The 53-bit mantissa is z >> 11; bit
       10 is the rounding bit and bits 9..0 are sticky. */
    uint32_t lo = (zl >> 11) | (zh << 21);
    uint32_t hi = zh >> 11;
    uint32_t cc = (zl >> 10) & (lo | (uint32_t)((zl << 22) != 0)) & 1;
    uint32_t nz = 0u - (zh >> 31);
    C3_OPAQUE(nz);
    uint32_t e = (o->ex - c + 7) & nz;

    lo += cc;
    hi += (uint32_t)(lo < cc);
    /* The implicit bit of the mantissa (bit 20 of hi) adds 1 to the
       exponent field, and so does a carry out of the rounding. */
    hi += (sign & 0x80000000u) + (e << 20);
    return ((uint64_t)hi << 32) | lo;
}

fpr
fpr_add(fpr x, fpr y)
{
    c3_fpr_ops o;

    c3_fpr_prepare(&o, x, y);
    return c3_fpr_finish(&o, o.dm, o.sx);
}

/* a <- x + y, b <- x - y. For x - y the smaller-or-exchanged operand has
   its sign flipped: the "signs differ" flag is inverted, and the sign of
   the result is flipped if the operands were exchanged. */
#define fpr_add_sub   fndsa_fpr_add_sub
void
fpr_add_sub(fpr *a, fpr *b, fpr x, fpr y)
{
    c3_fpr_ops o;

    c3_fpr_prepare(&o, x, y);
    *a = c3_fpr_finish(&o, o.dm, o.sx);
    *b = c3_fpr_finish(&o, ~o.dm, o.sx ^ (o.sw << 31));
}

fpr
fpr_mul(fpr x, fpr y)
{
    uint32_t xl = (uint32_t)x, xh = (uint32_t)(x >> 32);
    uint32_t yl = (uint32_t)y, yh = (uint32_t)(y >> 32);
    uint32_t ex = (xh >> 20) & 0x7FF;
    uint32_t ey = (yh >> 20) & 0x7FF;
    uint32_t sign = (xh ^ yh) & 0x80000000u;

    /* 53-bit mantissas with the implicit bit, as (21 bits : 32 bits). */
    uint32_t xm = ((xh << 12) >> 12) | 0x00100000u;
    uint32_t ym = ((yh << 12) >> 12) | 0x00100000u;

    /* 106-bit product in w3:w2:w1:w0. */
    uint64_t p = (uint64_t)xl * yl;
    uint64_t q = (uint64_t)xm * yl;
    uint64_t r = (uint64_t)xl * ym;
    uint64_t u = (uint64_t)xm * ym;
    uint32_t w0 = (uint32_t)p;
    uint32_t s1 = (uint32_t)(p >> 32) + (uint32_t)q;
    uint32_t c1 = (uint32_t)(s1 < (uint32_t)q);
    uint32_t w1 = s1 + (uint32_t)r;
    uint32_t c2 = (uint32_t)(w1 < (uint32_t)r);
    uint32_t s2 = (uint32_t)(q >> 32) + (uint32_t)(r >> 32) + c1 + c2;
    uint32_t w2 = s2 + (uint32_t)u;
    uint32_t w3 = (uint32_t)(u >> 32) + (uint32_t)(w2 < s2);

    /* z = product >> 50, in [2^54, 2^56), lowest bit sticky. */
    uint32_t zl = (w1 >> 18) | (w2 << 14);
    uint32_t zh = (w2 >> 18) | (w3 << 14);
    zl |= (uint32_t)((w0 | (w1 << 14)) != 0);

    /* If z >= 2^55, shift it right by one bit (the lowest bit stays
       sticky) and add one to the exponent. */
    uint32_t es = zh >> 23;
    uint32_t me = 0u - es;
    C3_OPAQUE(me);
    zl = (zl & ~me) | (((zl >> 1) | (zh << 31) | (zl & 1)) & me);
    zh >>= es;

    /* A zero operand gives a zero with the product's sign. */
    uint32_t nz = ~(uint32_t)((int32_t)((ex - 1) | (ey - 1)) >> 31);
    C3_OPAQUE(nz);
    zl &= nz;
    zh &= nz;
    uint32_t e = (ex + ey - 1024 + es) & nz;

    /* Mantissa z >> 2, rounded to nearest even on the low three bits. */
    uint32_t lo = (zl >> 2) | (zh << 30);
    uint32_t hi = zh >> 2;
    uint32_t cc = (0xC8u >> (zl & 7)) & 1;

    lo += cc;
    hi += (uint32_t)(lo < cc);
    hi += sign + (e << 20);
    return ((uint64_t)hi << 32) | lo;
}
