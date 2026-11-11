/* SHAKE256 and SHA-3 for HQC on the ESP32-C3. Included by hqc_unit_opt.inc
 * in place of the reference common/symmetric.c; it defines the same
 * functions with the same results.
 *
 * The reference file goes through the incremental FIPS 202 interface, which
 * moves data in and out of the Keccak state one byte at a time with 64-bit
 * shifts. HQC hashes and expands kilobytes per operation (the public key,
 * the ciphertext, the random polynomial h), so here the state is treated as
 * bytes of memory: input is XOR-ed in and output copied out a word at a
 * time. The permutation itself is pqc_keccakf1600() from pqc_prims.h. */
#include <stdint.h>
#include <string.h>
#include "pqc_prims.h"
#include "symmetric.h"

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "hqc_c3_sym.c needs a little-endian machine"
#endif

typedef uint32_t __attribute__((may_alias)) c3_sym_u32;

/* ctx[0..24] is the Keccak state. ctx[25] counts the bytes absorbed into
   the current block, or, after finalization, the bytes of the current block
   not yet squeezed out. */
#define C3_POS(s) ((s)[25])

static void c3_sponge_absorb(uint64_t *s, uint32_t rate, const uint8_t *m, size_t len)
{
    uint8_t *sb = (uint8_t *)s;
    uint32_t pos = (uint32_t)C3_POS(s);

    while (len > 0) {
        size_t n = rate - pos;
        uint8_t *d = sb + pos;

        if (n > len) {
            n = len;
        }
        pos += (uint32_t)n;
        len -= n;
        while (n > 0 && ((uintptr_t)d & 3) != 0) {
            *d++ ^= *m++;
            n--;
        }
        if (((uintptr_t)m & 3) == 0) {
            for (; n >= 4; n -= 4, d += 4, m += 4) {
                *(c3_sym_u32 *)d ^= *(const c3_sym_u32 *)m;
            }
        }
        while (n > 0) {
            *d++ ^= *m++;
            n--;
        }
        if (pos == rate) {
            pqc_keccakf1600(s);
            pos = 0;
        }
    }
    C3_POS(s) = pos;
}

static void c3_sponge_finalize(uint64_t *s, uint32_t rate, uint8_t pad)
{
    uint8_t *sb = (uint8_t *)s;

    sb[(uint32_t)C3_POS(s)] ^= pad;
    sb[rate - 1] ^= 0x80;
    C3_POS(s) = 0;
}

static void c3_sponge_squeeze(uint64_t *s, uint32_t rate, uint8_t *out, size_t len)
{
    const uint8_t *sb = (const uint8_t *)s;
    uint32_t avail = (uint32_t)C3_POS(s);

    while (len > 0) {
        size_t n;

        if (avail == 0) {
            pqc_keccakf1600(s);
            avail = rate;
        }
        n = avail < len ? avail : len;
        memcpy(out, sb + rate - avail, n);
        out += n;
        len -= n;
        avail -= (uint32_t)n;
    }
    C3_POS(s) = avail;
}

void xof_init(shake256_xof_ctx *xof_ctx, const uint8_t *seed, uint32_t seed_size)
{
    memset(xof_ctx->ctx, 0, sizeof(xof_ctx->ctx));
    c3_sponge_absorb(xof_ctx->ctx, SHAKE256_RATE, seed, seed_size);
    c3_sponge_finalize(xof_ctx->ctx, SHAKE256_RATE, 0x1F);
}

void xof_init_dom_sep(shake256_xof_ctx *xof_ctx, const uint8_t *seed, uint32_t seed_size, uint8_t domain_separator)
{
    memset(xof_ctx->ctx, 0, sizeof(xof_ctx->ctx));
    c3_sponge_absorb(xof_ctx->ctx, SHAKE256_RATE, seed, seed_size);
    c3_sponge_absorb(xof_ctx->ctx, SHAKE256_RATE, &domain_separator, 1);
    c3_sponge_finalize(xof_ctx->ctx, SHAKE256_RATE, 0x1F);
}

void xof_get_bytes(shake256_xof_ctx *xof_ctx, uint8_t *output, uint32_t output_size)
{
    c3_sponge_squeeze(xof_ctx->ctx, SHAKE256_RATE, output, output_size);
}

/* SHA3-512 of up to three parts followed by a one-byte domain separator. */
static void c3_sha3(uint8_t *output, uint32_t rate, uint32_t outlen, uint8_t domain,
                    const uint8_t *a, size_t alen, const uint8_t *b, size_t blen,
                    const uint8_t *c, size_t clen, const uint8_t *d, size_t dlen,
                    const uint8_t *e, size_t elen)
{
    uint64_t s[26];

    memset(s, 0, sizeof(s));
    c3_sponge_absorb(s, rate, a, alen);
    c3_sponge_absorb(s, rate, b, blen);
    c3_sponge_absorb(s, rate, c, clen);
    c3_sponge_absorb(s, rate, d, dlen);
    c3_sponge_absorb(s, rate, e, elen);
    c3_sponge_absorb(s, rate, &domain, 1);
    c3_sponge_finalize(s, rate, 0x06);
    c3_sponge_squeeze(s, rate, output, outlen);
    memset(s, 0, sizeof(s));
}

void hash_g(uint8_t *output, const uint8_t hash_ek_kem[SEED_BYTES], const uint8_t m[PARAM_K],
            const uint8_t salt[SALT_BYTES])
{
    c3_sha3(output, SHA3_512_RATE, 64, HQC_G_FCT_DOMAIN, hash_ek_kem, SEED_BYTES, m, PARAM_K, salt,
            SALT_BYTES, NULL, 0, NULL, 0);
}

void hash_h(uint8_t *output, const uint8_t ek_kem[PUBLIC_KEY_BYTES_KEM])
{
    c3_sha3(output, SHA3_256_RATE, 32, HQC_H_FCT_DOMAIN, ek_kem, PUBLIC_KEY_BYTES_KEM, NULL, 0, NULL, 0,
            NULL, 0, NULL, 0);
}

void hash_i(uint8_t *output, const uint8_t *seed)
{
    c3_sha3(output, SHA3_512_RATE, 64, HQC_I_FCT_DOMAIN, seed, SEED_BYTES, NULL, 0, NULL, 0, NULL, 0,
            NULL, 0);
}

void hash_j(uint8_t *output, const uint8_t hash_ek_kem[SEED_BYTES], const uint8_t sigma[PARAM_K],
            const uint64_t *u, const uint64_t *v, const uint8_t salt[SALT_BYTES])
{
    c3_sha3(output, SHA3_256_RATE, 32, HQC_J_FCT_DOMAIN, hash_ek_kem, SEED_BYTES, sigma, PARAM_K,
            (const uint8_t *)u, VEC_N_SIZE_BYTES, (const uint8_t *)v, VEC_N1N2_SIZE_BYTES, salt,
            SALT_BYTES);
}
