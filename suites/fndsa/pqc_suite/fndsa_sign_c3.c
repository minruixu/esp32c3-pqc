/* FN-DSA signing as built for the ESP32-C3: c-fn-dsa's signing code as one
 * compilation unit, with the emulated floating-point addition and
 * multiplication replaced by fpr_c3.c and with every x+y / x-y pair of the
 * FFT code computed by one call. Everything else (key generation,
 * verification, hashing, encoding) is shared with the unchanged library.
 *
 * Public entry points: fndsa_c3_sign_temp() and friends, same parameters as
 * the functions without "c3_" in fndsa.h. */
#include "fndsa_c3_rename.h"
#include "sign_inner.h"

/* c-fn-dsa's fpr_scaled, fpr_div and fpr_sqrt are used as they are. Its
   fpr_add and fpr_mul are compiled under other names; they are kept for the
   on-device comparison in fndsa_c3_fpr_selftest(). */
#undef fpr_add
#undef fpr_mul
#define fpr_add fndsa_c3_fpr_add_upstream
#define fpr_mul fndsa_c3_fpr_mul_upstream
fpr fpr_add(fpr x, fpr y);
fpr fpr_mul(fpr x, fpr y);
#include "sign_fpr.c"
#undef fpr_add
#undef fpr_mul
#define fpr_add fndsa_fpr_add
#define fpr_mul fndsa_fpr_mul

#include "fpr_c3.c"

/* One call for a sum and a difference of the same two values. */
#undef FPR_ADD_SUB
#define FPR_ADD_SUB(a, b, x, y)   do { \
		fpr t_add_sub_a, t_add_sub_b; \
		fpr_add_sub(&t_add_sub_a, &t_add_sub_b, (x), (y)); \
		(a) = t_add_sub_a; \
		(b) = t_add_sub_b; \
	} while (0)

#include "sign_fpoly.c"
#include "sign_sampler.c"
#include "sign_core.c"
#include "sign.c"

/* Compare fpr_c3.c with c-fn-dsa's functions on `count` pseudo-random
   operand pairs of the kinds that occur in signing (and some that do not:
   zeros, equal magnitudes, far-apart exponents). Returns the number of
   mismatches. The same comparison, with more cases and against hardware
   floating point, is in tests/test_fpr_host.c. */
uint32_t
fndsa_c3_fpr_selftest(uint32_t count)
{
	uint32_t s0 = 0x9E3779B9u, s1 = 0x243F6A88u, s2 = 0xB7E15162u, s3 = 0x85A308D3u;
	uint32_t bad = 0;

	for (uint32_t i = 0; i < count; i ++) {
		uint32_t w[5];
		for (int j = 0; j < 5; j ++) {
			uint32_t r = ((s1 * 5) << 7 | (s1 * 5) >> 25) * 9;
			uint32_t t = s1 << 9;
			s2 ^= s0; s3 ^= s1; s1 ^= s2; s0 ^= s3; s2 ^= t;
			s3 = (s3 << 11) | (s3 >> 21);
			w[j] = r;
		}
		uint32_t ex = 723 + (w[4] % 601);
		uint32_t ey = ((w[4] >> 12) & 3) == 0 ? ex
			: ((w[4] >> 12) & 3) == 1 ? ex + ((w[4] >> 16) % 141) - 70
			: 723 + ((w[4] >> 16) % 601);
		fpr x = ((uint64_t)(w[0] & 0x800FFFFFu) << 32) | w[1];
		fpr y = ((uint64_t)(w[2] & 0x800FFFFFu) << 32) | w[3];
		x |= (uint64_t)ex << 52;
		y |= (uint64_t)ey << 52;
		switch (w[4] >> 27) {
		case 0: y = x; break;
		case 1: y = x ^ ((uint64_t)1 << 63); break;
		case 2: y &= (uint64_t)1 << 63; break;
		case 3: x &= (uint64_t)1 << 63; break;
		case 4: y = (y & ~(uint64_t)0xFFFFFFFF) | (uint32_t)x; break;
		default: break;
		}

		fpr a, b;
		fpr ra = fndsa_c3_fpr_add_upstream(x, y);
		fpr rb = fndsa_c3_fpr_add_upstream(x, y ^ ((uint64_t)1 << 63));
		fpr_add_sub(&a, &b, x, y);
		bad += (uint32_t)(fpr_add(x, y) != ra);
		bad += (uint32_t)(a != ra);
		bad += (uint32_t)(b != rb);
		bad += (uint32_t)(fpr_mul(x, y) != fndsa_c3_fpr_mul_upstream(x, y));
	}
	return bad;
}
