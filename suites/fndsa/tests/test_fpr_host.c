/* Differential test of pqc_suite/fpr_c3.c on the build machine.
 *
 * Compares fpr_add, fpr_mul and fpr_add_sub of fpr_c3.c with (1) c-fn-dsa's
 * emulated versions (sign_fpr.c built with FNDSA_NEON=0 FNDSA_SSE2=0) and
 * (2) the machine's own IEEE-754 double arithmetic, on random and on
 * hand-picked operands. Build and run with ./run_fpr_host.sh [millions]. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t fpr;

fpr fndsa_fpr_add(fpr x, fpr y); /* c-fn-dsa, emulated */
fpr fndsa_fpr_mul(fpr x, fpr y);

#define fpr_add c3_add
#define fpr_mul c3_mul
#define fndsa_fpr_add_sub c3_add_sub
#include "../pqc_suite/fpr_c3.c"

static uint64_t s[4] = {0x243f6a8885a308d3ull, 0x13198a2e03707344ull, 0xa4093822299f31d0ull,
                        0x082efa98ec4e6c89ull};
static uint64_t rnd(void)
{
    uint64_t r = ((s[1] * 5) << 7 | (s[1] * 5) >> 57) * 9, t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3]; s[2] ^= t;
    s[3] = (s[3] << 45) | (s[3] >> 19);
    return r;
}

static fpr mk(uint64_t sign, int e, uint64_t m) /* e = unbiased exponent */
{
    return (sign << 63) | ((uint64_t)(e + 1023) << 52) | (m & 0xFFFFFFFFFFFFFull);
}

static uint64_t mant(void)
{
    uint64_t r = rnd();
    switch (r >> 60) {
    case 0: return 0;
    case 1: return 0xFFFFFFFFFFFFFull;
    case 2: return 1ull << ((r >> 8) % 52);
    case 3: return 0xFFFFFFFFFFFFFull ^ (1ull << ((r >> 8) % 52));
    case 4: return rnd() & (rnd() | 0xFFFFF00000000ull);  /* sparse low part */
    case 5: return rnd() & 0xFFFFFFFF00000ull;            /* low bits zero */
    default: return rnd();
    }
}

static double f(fpr x) { double d; memcpy(&d, &x, 8); return d; }
static fpr b(double d) { fpr x; memcpy(&x, &d, 8); return x; }

static unsigned long fails;
static void check(const char *what, fpr x, fpr y, fpr got, fpr ref, fpr native)
{
    if ((got != ref || got != native) && fails++ < 20) {
        printf("FAIL %s x=%016llx y=%016llx got=%016llx c-fn-dsa=%016llx native=%016llx\n", what,
               (unsigned long long)x, (unsigned long long)y, (unsigned long long)got,
               (unsigned long long)ref, (unsigned long long)native);
    }
}

static void one(fpr x, fpr y)
{
    fpr a, d, ny = y ^ (1ull << 63);
    volatile double fx = f(x), fy = f(y);

    check("add", x, y, c3_add(x, y), fndsa_fpr_add(x, y), b(fx + fy));
    c3_add_sub(&a, &d, x, y);
    check("add_sub.sum", x, y, a, fndsa_fpr_add(x, y), b(fx + fy));
    check("add_sub.diff", x, y, d, fndsa_fpr_add(x, ny), b(fx - fy));
    check("mul", x, y, c3_mul(x, y), fndsa_fpr_mul(x, y), b(fx * fy));
}

int main(int argc, char **argv)
{
    unsigned long n = (argc > 1 ? strtoul(argv[1], NULL, 10) : 20) * 1000000ul;
    static const fpr zeros[2] = {0, 1ull << 63};

    for (unsigned long i = 0; i < n; i++) {
        uint64_t r = rnd();
        int ex = (int)(r % 601) - 300, ey;
        fpr x, y;

        switch ((r >> 16) & 7) {
        case 0: ey = ex; break;                                 /* same exponent */
        case 1: ey = ex + (int)((r >> 20) % 3) - 1; break;      /* neighbours */
        case 2: ey = ex + (int)((r >> 20) % 141) - 70; break;   /* around the 60-bit cut */
        default: ey = (int)((r >> 20) % 601) - 300; break;
        }
        x = mk(r >> 63, ex, mant());
        y = mk((r >> 62) & 1, ey, mant());
        switch ((r >> 40) & 31) {
        case 0: y = x; break;
        case 1: y = x ^ (1ull << 63); break;
        case 2: y = zeros[(r >> 50) & 1]; break;
        case 3: x = zeros[(r >> 50) & 1]; break;
        case 4: x = zeros[(r >> 50) & 1]; y = zeros[(r >> 51) & 1]; break;
        case 5: y = mk((r >> 62) & 1, ex, (x & 0xFFFFFFFFFFFFFull) + ((r >> 50) & 3) - 1); break;
        default: break;
        }
        one(x, y);
    }
    printf("%lu operand pairs, %lu mismatches\n", n, fails);
    return fails != 0;
}
