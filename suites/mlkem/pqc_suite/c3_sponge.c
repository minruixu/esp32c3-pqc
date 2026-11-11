#include <string.h>
#include "c3_sponge.h"
#include "pqc_prims.h"

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error c3_sponge.c assumes a little-endian target
#endif

#if defined(PQC_HOST)
/* On the build machine the permutations are counted, to tell how much of an
 * operation is Keccak (the count is the same on the device). */
extern unsigned long pqc_perm_count;
#define PERMUTE(c) (pqc_perm_count++, pqc_keccakf1600((c)->u.s))
#else
#define PERMUTE(c) pqc_keccakf1600((c)->u.s)
#endif

typedef uint32_t __attribute__((may_alias)) u32a;

void c3_sponge_init(c3_sponge *c)
{
    memset(c, 0, sizeof(*c));
}

void c3_sponge_wipe(c3_sponge *c)
{
    memset(c, 0, sizeof(*c));
    __asm__ volatile("" : : "r"(c) : "memory"); /* keep the memset */
}

/* dst ^= src for n bytes; dst lies inside the state */
static void xor_in(uint8_t *dst, const uint8_t *src, size_t n)
{
    size_t words;

    while (n && ((uintptr_t)dst & 3)) {
        *dst++ ^= *src++;
        n--;
    }
    words = n / 4;
    if (words) {
        u32a *d = (u32a *)dst;
        if (((uintptr_t)src & 3) == 0) {
            const u32a *s = (const u32a *)src;
            for (size_t i = 0; i < words; i++) {
                d[i] ^= s[i];
            }
        } else {
            uint32_t tmp[50];
            memcpy(tmp, src, 4 * words);
            for (size_t i = 0; i < words; i++) {
                d[i] ^= tmp[i];
            }
        }
        dst += 4 * words;
        src += 4 * words;
        n -= 4 * words;
    }
    while (n--) {
        *dst++ ^= *src++;
    }
}

void c3_sponge_absorb(c3_sponge *c, unsigned rate, const uint8_t *in, size_t len)
{
    while (len) {
        size_t n = rate - c->pos;
        if (n > len) {
            n = len;
        }
        xor_in(c->u.b + c->pos, in, n);
        c->pos += (unsigned)n;
        in += n;
        len -= n;
        if (c->pos == rate) {
            PERMUTE(c);
            c->pos = 0;
        }
    }
}

void c3_sponge_finalize(c3_sponge *c, unsigned rate, uint8_t pad)
{
    c->u.b[c->pos] ^= pad;
    c->u.b[rate - 1] ^= 0x80;
    c->pos = rate; /* nothing to take yet: the first squeeze permutes */
}

void c3_sponge_squeeze(c3_sponge *c, unsigned rate, uint8_t *out, size_t len)
{
    while (len) {
        size_t n;
        if (c->pos == rate) {
            PERMUTE(c);
            c->pos = 0;
        }
        n = rate - c->pos;
        if (n > len) {
            n = len;
        }
        memcpy(out, c->u.b + c->pos, n);
        c->pos += (unsigned)n;
        out += n;
        len -= n;
    }
}

void c3_sponge_hash(unsigned rate, uint8_t pad, uint8_t *out, size_t outlen, const uint8_t *in,
                    size_t inlen)
{
    c3_sponge c;
    c3_sponge_init(&c);
    c3_sponge_absorb(&c, rate, in, inlen);
    c3_sponge_finalize(&c, rate, pad);
    c3_sponge_squeeze(&c, rate, out, outlen);
    c3_sponge_wipe(&c);
}
