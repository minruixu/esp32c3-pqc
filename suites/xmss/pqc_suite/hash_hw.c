/* XMSS hash layer on the SHA accelerator of the ESP32-C3 (device only), for
 * the SHA-256 parameter sets with n = 32 and n = 24. See
 * components/prims/opt/sha256_hw.h for the register interface.
 *
 * The accelerator's H and M registers hold hash values and message blocks as
 * plain byte strings, so the outputs of one hash go into the next block as
 * words, without conversion and without passing through a buffer:
 *  - n = 32: PRF(pub_seed, ADRS) starts from the cached state after the key
 *    block (loaded into H), and the two PRF calls of F share one message
 *    block that differs in a single word. The key is written straight into
 *    the first block of F, the mask is XORed into the second.
 *  - n = 24: the inputs are 60, 52 and 76 bytes; the blocks are assembled
 *    from words in the same way.
 *  - A block is written while the previous one is being compressed (the
 *    accelerator copies the M registers when it starts).
 */
#include <string.h>
#include "hash_address.h"
#include "params.h"
#include "pqc_prims.h"
#include "sha256_hw.h"
#include "xmss_port.h"

#define M PQC_SHA_M
#define H PQC_SHA_H

/* padding words of a message of `bits` bits whose last block ends here */
#define LEN_WORD(bits) ((((uint32_t)(bits) >> 8) & 0xff) << 16 | ((uint32_t)(bits) & 0xff) << 24)
#define PAD_WORD 0x00000080u

static inline __attribute__((always_inline)) uint32_t bswap32(uint32_t x)
{
    return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24);
}

static inline __attribute__((always_inline)) void start_iv(void)
{
    REG_WRITE(SHA_START_REG, 1);
}

static inline __attribute__((always_inline)) void start_cont(void)
{
    REG_WRITE(SHA_CONTINUE_REG, 1);
}

static inline __attribute__((always_inline)) void zero_m(int from, int to)
{
    for (int i = from; i < to; i++) {
        M[i] = 0;
    }
}

static int enabled(const xmss_params *params)
{
    return params->func == XMSS_SHA2 && (params->n == 32 || params->n == 24) &&
           pqc_sha256_compress_fn == pqc_sha256_compress_opt;
}

/* ---------- n = 32: state after toByte(3,32) || key, per key ---------- */
typedef struct {
    uint32_t key[8];
    uint32_t st[8];
    unsigned char valid;
} hmid_t;

static hmid_t hmids[2];
static unsigned hmid_next;

void xmss_hw_cache_clear(void)
{
    memset(hmids, 0, sizeof(hmids));
}

static const uint32_t *mid32(const unsigned char *key)
{
    uint32_t kw[8];
    hmid_t *m;
    int i;

    memcpy(kw, key, 32);
    for (i = 0; i < 2; i++) {
        m = &hmids[i];
        if (m->valid &&
            ((kw[0] ^ m->key[0]) | (kw[1] ^ m->key[1]) | (kw[2] ^ m->key[2]) | (kw[3] ^ m->key[3]) |
             (kw[4] ^ m->key[4]) | (kw[5] ^ m->key[5]) | (kw[6] ^ m->key[6]) | (kw[7] ^ m->key[7])) == 0) {
            return m->st;
        }
    }
    m = &hmids[hmid_next];
    hmid_next ^= 1;
    zero_m(0, 7);
    M[7] = 0x03000000u; /* toByte(3, 32) */
    for (i = 0; i < 8; i++) {
        M[8 + i] = kw[i];
    }
    start_iv();
    pqc_sha256_hw_wait();
    pqc_sha256_hw_read(m->st, 8);
    memcpy(m->key, kw, sizeof(kw));
    m->valid = 1;
    return m->st;
}

/* The second block of PRF for n = 32: 32 input bytes and the padding of a
 * 96-byte message. Leaves the accelerator running. */
static inline __attribute__((always_inline)) void prf32_block(const uint32_t *mid, const uint32_t in[8])
{
    pqc_sha256_hw_load(mid);
    for (int i = 0; i < 8; i++) {
        M[i] = in[i];
    }
    M[8] = PAD_WORD;
    zero_m(9, 15);
    M[15] = LEN_WORD(96 * 8);
    start_cont();
}

/* ---------- n = 24: PRF is two blocks from the initial value ---------- */
/* toByte(3,4) || key(24) || in(32) || 0x80, then a block with the length */
static inline __attribute__((always_inline)) void prf24(const uint32_t key[6], const uint32_t in[8],
                                                        uint32_t out[6])
{
    int i;

    M[0] = 0x03000000u;
    for (i = 0; i < 6; i++) {
        M[1 + i] = key[i];
    }
    for (i = 0; i < 8; i++) {
        M[7 + i] = in[i];
    }
    M[15] = PAD_WORD;
    start_iv();
    zero_m(0, 15);
    M[15] = LEN_WORD(60 * 8);
    pqc_sha256_hw_wait();
    start_cont();
    pqc_sha256_hw_wait();
    pqc_sha256_hw_read(out, 6);
}

/* ---------- the three hooks ---------- */
int xmss_sha_hw_prf(const xmss_params *params, unsigned char *out, const unsigned char in[32],
                    const unsigned char *key)
{
    uint32_t iw[8], o[8];

    if (!enabled(params)) {
        return 0;
    }
    pqc_sha256_hw_init();
    memcpy(iw, in, 32);
    if (params->n == 32) {
        prf32_block(mid32(key), iw);
        pqc_sha256_hw_wait();
        pqc_sha256_hw_read(o, 8);
    } else {
        uint32_t kw[6];
        memcpy(kw, key, 24);
        prf24(kw, iw, o);
    }
    memcpy(out, o, params->n);
    return 1;
}

int xmss_sha_hw_thash_f(const xmss_params *params, unsigned char *out, const unsigned char *in,
                        const unsigned char *pub_seed, uint32_t addr[8])
{
    uint32_t aw[8], k[8], m[8], x[8], o[8];
    int i;

    if (!enabled(params)) {
        return 0;
    }
    pqc_sha256_hw_init();
    for (i = 0; i < 7; i++) {
        aw[i] = bswap32(addr[i]);
    }
    aw[7] = 0; /* key_and_mask = 0 */
    if (params->n == 32) {
        const uint32_t *mid = mid32(pub_seed);

        prf32_block(mid, aw);
        memcpy(x, in, 32);
        pqc_sha256_hw_wait();
        pqc_sha256_hw_read(k, 8);
        pqc_sha256_hw_load(mid);
        M[7] = 0x01000000u; /* key_and_mask = 1, the rest of the block is still there */
        start_cont();
        pqc_sha256_hw_wait();
        pqc_sha256_hw_read(m, 8);
        /* F: toByte(0,32) || key, then (in xor mask) and the padding */
        zero_m(0, 8);
        for (i = 0; i < 8; i++) {
            M[8 + i] = k[i];
        }
        start_iv();
        for (i = 0; i < 8; i++) {
            M[i] = x[i] ^ m[i];
        }
        M[8] = PAD_WORD;
        zero_m(9, 15);
        M[15] = LEN_WORD(96 * 8);
        pqc_sha256_hw_wait();
        start_cont();
        pqc_sha256_hw_wait();
        pqc_sha256_hw_read(o, 8);
        memcpy(out, o, 32);
    } else {
        uint32_t sw[6];

        memcpy(sw, pub_seed, 24);
        memcpy(x, in, 24);
        prf24(sw, aw, k);
        aw[7] = 0x01000000u;
        prf24(sw, aw, m);
        /* F: toByte(0,4) || key(24) || (in xor mask)(24), 52 bytes, one block */
        M[0] = 0;
        for (i = 0; i < 6; i++) {
            M[1 + i] = k[i];
            M[7 + i] = x[i] ^ m[i];
        }
        M[13] = PAD_WORD;
        M[14] = 0;
        M[15] = LEN_WORD(52 * 8);
        start_iv();
        pqc_sha256_hw_wait();
        pqc_sha256_hw_read(o, 6);
        memcpy(out, o, 24);
    }
    addr[7] = 1; /* as the reference function leaves it */
    return 1;
}

int xmss_sha_hw_thash_h(const xmss_params *params, unsigned char *out, const unsigned char *in,
                        const unsigned char *pub_seed, uint32_t addr[8])
{
    uint32_t aw[8], k[8], m[16], x[16], o[8];
    int i;

    if (!enabled(params)) {
        return 0;
    }
    pqc_sha256_hw_init();
    for (i = 0; i < 7; i++) {
        aw[i] = bswap32(addr[i]);
    }
    aw[7] = 0;
    if (params->n == 32) {
        const uint32_t *mid = mid32(pub_seed);

        prf32_block(mid, aw);
        memcpy(x, in, 64);
        pqc_sha256_hw_wait();
        pqc_sha256_hw_read(k, 8);
        pqc_sha256_hw_load(mid);
        M[7] = 0x01000000u;
        start_cont();
        pqc_sha256_hw_wait();
        pqc_sha256_hw_read(m, 8);
        pqc_sha256_hw_load(mid);
        M[7] = 0x02000000u;
        start_cont();
        pqc_sha256_hw_wait();
        pqc_sha256_hw_read(m + 8, 8);
        /* H: toByte(1,32) || key, then (in xor mask), then the padding */
        zero_m(0, 7);
        M[7] = 0x01000000u;
        for (i = 0; i < 8; i++) {
            M[8 + i] = k[i];
        }
        start_iv();
        for (i = 0; i < 16; i++) {
            M[i] = x[i] ^ m[i];
        }
        pqc_sha256_hw_wait();
        start_cont();
        M[0] = PAD_WORD;
        zero_m(1, 15);
        M[15] = LEN_WORD(128 * 8);
        pqc_sha256_hw_wait();
        start_cont();
        pqc_sha256_hw_wait();
        pqc_sha256_hw_read(o, 8);
        memcpy(out, o, 32);
    } else {
        uint32_t sw[6];

        memcpy(sw, pub_seed, 24);
        memcpy(x, in, 48);
        prf24(sw, aw, k);
        aw[7] = 0x01000000u;
        prf24(sw, aw, m);
        aw[7] = 0x02000000u;
        prf24(sw, aw, m + 6);
        /* H: toByte(1,4) || key(24) || (in xor mask)(48), 76 bytes, two blocks */
        M[0] = 0x01000000u;
        for (i = 0; i < 6; i++) {
            M[1 + i] = k[i];
        }
        for (i = 0; i < 9; i++) {
            M[7 + i] = x[i] ^ m[i];
        }
        start_iv();
        for (i = 0; i < 3; i++) {
            M[i] = x[9 + i] ^ m[9 + i];
        }
        M[3] = PAD_WORD;
        zero_m(4, 15);
        M[15] = LEN_WORD(76 * 8);
        pqc_sha256_hw_wait();
        start_cont();
        pqc_sha256_hw_wait();
        pqc_sha256_hw_read(o, 6);
        memcpy(out, o, 24);
    }
    addr[7] = 2;
    return 1;
}
