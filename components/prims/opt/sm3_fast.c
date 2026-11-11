/* pqc_sm3_compress_opt(): SM3 compression tuned for RV32IMC (no rotate
 * instruction, 31 registers). Compared with the reference in ../sm3.c:
 * the 64 rounds are unrolled with the state rotating through the arguments
 * instead of being moved, the round constants are stored already rotated,
 * the two Boolean functions use their three- and four-operation forms, and
 * the message expansion is unrolled. */
#include <stddef.h>
#include <stdint.h>
#include "pqc_prims.h"

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define P0(x) ((x) ^ ROTL((x), 9) ^ ROTL((x), 17))
#define P1(x) ((x) ^ ROTL((x), 15) ^ ROTL((x), 23))

/* T_j <<< (j mod 32) */
static const uint32_t KT[64] = {
    0x79cc4519, 0xf3988a32, 0xe7311465, 0xce6228cb, 0x9cc45197, 0x3988a32f, 0x7311465e, 0xe6228cbc,
    0xcc451979, 0x988a32f3, 0x311465e7, 0x6228cbce, 0xc451979c, 0x88a32f39, 0x11465e73, 0x228cbce6,
    0x9d8a7a87, 0x3b14f50f, 0x7629ea1e, 0xec53d43c, 0xd8a7a879, 0xb14f50f3, 0x629ea1e7, 0xc53d43ce,
    0x8a7a879d, 0x14f50f3b, 0x29ea1e76, 0x53d43cec, 0xa7a879d8, 0x4f50f3b1, 0x9ea1e762, 0x3d43cec5,
    0x7a879d8a, 0xf50f3b14, 0xea1e7629, 0xd43cec53, 0xa879d8a7, 0x50f3b14f, 0xa1e7629e, 0x43cec53d,
    0x879d8a7a, 0x0f3b14f5, 0x1e7629ea, 0x3cec53d4, 0x79d8a7a8, 0xf3b14f50, 0xe7629ea1, 0xcec53d43,
    0x9d8a7a87, 0x3b14f50f, 0x7629ea1e, 0xec53d43c, 0xd8a7a879, 0xb14f50f3, 0x629ea1e7, 0xc53d43ce,
    0x8a7a879d, 0x14f50f3b, 0x29ea1e76, 0x53d43cec, 0xa7a879d8, 0x4f50f3b1, 0x9ea1e762, 0x3d43cec5};

#define FF0(x, y, z) ((x) ^ (y) ^ (z))
#define GG0(x, y, z) ((x) ^ (y) ^ (z))
#define FF1(x, y, z) (((x) & (y)) | (((x) | (y)) & (z)))
#define GG1(x, y, z) ((((y) ^ (z)) & (x)) ^ (z))

/* One round. On return d holds the new A, h the new E, b the new C and f the
 * new G, so the caller rotates the argument list instead of moving words. */
#define ROUND(a, b, c, d, e, f, g, h, FF, GG, j)        \
    do {                                                \
        uint32_t a12 = ROTL(a, 12);                     \
        uint32_t ss1 = a12 + e + KT[j];                 \
        uint32_t wj = w[j];                             \
        ss1 = ROTL(ss1, 7);                             \
        d += FF(a, b, c) + (ss1 ^ a12) + (wj ^ w[j + 4]); \
        h += GG(e, f, g) + ss1 + wj;                    \
        b = ROTL(b, 9);                                 \
        f = ROTL(f, 19);                                \
        h = P0(h);                                      \
    } while (0)

#define EXPAND(j)                                                        \
    do {                                                                 \
        uint32_t x = w[(j) - 16] ^ w[(j) - 9] ^ ROTL(w[(j) - 3], 15);    \
        w[j] = P1(x) ^ ROTL(w[(j) - 13], 7) ^ w[(j) - 6];                \
    } while (0)

#define ROUNDS8(FF, GG, j)                           \
    ROUND(a, b, c, d, e, f, g, h, FF, GG, (j) + 0);  \
    ROUND(d, a, b, c, h, e, f, g, FF, GG, (j) + 1);  \
    ROUND(c, d, a, b, g, h, e, f, FF, GG, (j) + 2);  \
    ROUND(b, c, d, a, f, g, h, e, FF, GG, (j) + 3);  \
    ROUND(a, b, c, d, e, f, g, h, FF, GG, (j) + 4);  \
    ROUND(d, a, b, c, h, e, f, g, FF, GG, (j) + 5);  \
    ROUND(c, d, a, b, g, h, e, f, FF, GG, (j) + 6);  \
    ROUND(b, c, d, a, f, g, h, e, FF, GG, (j) + 7)

void pqc_sm3_compress_rv32(uint32_t state[8], const uint8_t *blocks, size_t nblocks)
{
    uint32_t w[68];

    while (nblocks--) {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        int j;

        for (j = 0; j < 16; j++) {
            w[j] = ((uint32_t)blocks[4 * j] << 24) | ((uint32_t)blocks[4 * j + 1] << 16) |
                   ((uint32_t)blocks[4 * j + 2] << 8) | blocks[4 * j + 3];
        }
        for (j = 16; j < 68; j += 4) {
            EXPAND(j);
            EXPAND(j + 1);
            EXPAND(j + 2);
            EXPAND(j + 3);
        }
        ROUNDS8(FF0, GG0, 0);
        ROUNDS8(FF0, GG0, 8);
        ROUNDS8(FF1, GG1, 16);
        ROUNDS8(FF1, GG1, 24);
        ROUNDS8(FF1, GG1, 32);
        ROUNDS8(FF1, GG1, 40);
        ROUNDS8(FF1, GG1, 48);
        ROUNDS8(FF1, GG1, 56);
        state[0] ^= a; state[1] ^= b; state[2] ^= c; state[3] ^= d;
        state[4] ^= e; state[5] ^= f; state[6] ^= g; state[7] ^= h;
        blocks += 64;
    }
}

/* The optimized backend of pqc_prims.h. pqc_sm3_compress_rv32 is also a name
 * that only this file defines, so a build can ask the linker for it. */
void pqc_sm3_compress_opt(uint32_t state[8], const uint8_t *blocks, size_t nblocks)
    __attribute__((alias("pqc_sm3_compress_rv32")));
