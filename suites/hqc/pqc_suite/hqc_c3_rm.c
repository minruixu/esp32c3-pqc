/* Duplicated Reed-Muller RM(1,7) code of HQC for the ESP32-C3. Included by
 * hqc_unit_opt.inc in place of the reference ref/reed_muller.c; it defines
 * reed_muller_encode and reed_muller_decode with the same results.
 *
 * Encoding is the reference code. Decoding does the same three steps as the
 * reference (sum the copies of a codeword bit by bit, Hadamard transform,
 * pick the peak), written for this core:
 *   - the reference extracts every bit of every copy with a variable shift;
 *     here a byte is spread to one bit per 4-bit field (c3_gf_spread), the
 *     spread copies are added as integers, and the eight sums are read out
 *     of the fields;
 *   - the transform works in place, two levels per pass;
 *   - the peak search has no data-dependent branch.
 * Like the reference, it runs the same instructions for every input. */
#include <stdint.h>
#include <string.h>
#include "data_structures.h"
#include "parameters.h"
#include "reed_muller.h"

#define C3_RM_COPIES CEIL_DIVIDE(PARAM_N2, 128)

#if defined(__GNUC__) || defined(__clang__)
#define C3_RM_OPAQUE(v) __asm__("" : "+r"(v))
#else
#define C3_RM_OPAQUE(v) ((void)0)
#endif

#define C3_RM_BIT0MASK(x) (uint32_t)(-((x) & 1))

/* Encode one byte into one RM(1,7) codeword (as in the reference). */
static void c3_rm_encode_byte(rm_codeword_t *word, uint32_t message)
{
    uint32_t first_word;

    first_word = C3_RM_BIT0MASK(message >> 7);
    first_word ^= C3_RM_BIT0MASK(message >> 0) & 0x55555555;
    first_word ^= C3_RM_BIT0MASK(message >> 1) & 0x33333333;
    first_word ^= C3_RM_BIT0MASK(message >> 2) & 0x0f0f0f0f;
    first_word ^= C3_RM_BIT0MASK(message >> 3) & 0xff00ff00;
    first_word ^= C3_RM_BIT0MASK(message >> 4) & 0xffff0000;
    word->u32[0] = first_word;
    first_word ^= C3_RM_BIT0MASK(message >> 5);
    word->u32[1] = first_word;
    first_word ^= C3_RM_BIT0MASK(message >> 6);
    word->u32[3] = first_word;
    first_word ^= C3_RM_BIT0MASK(message >> 5);
    word->u32[2] = first_word;
}

void reed_muller_encode(uint64_t *cdw, const uint64_t *msg)
{
    const uint8_t *message_array = (const uint8_t *)msg;
    rm_codeword_t *code_array = (rm_codeword_t *)cdw;

    for (size_t i = 0; i < VEC_N1_SIZE_BYTES; i++) {
        size_t pos = i * C3_RM_COPIES;

        c3_rm_encode_byte(&code_array[pos], message_array[i]);
        for (size_t copy = 1; copy < C3_RM_COPIES; copy++) {
            memcpy(&code_array[pos + copy], &code_array[pos], sizeof(rm_codeword_t));
        }
    }
}

/* Two levels of the Hadamard transform at once, strides h and 2h: four
   loads, eight additions, four stores for four butterflies. */
static inline __attribute__((always_inline)) void c3_rm_hadamard_2levels(int16_t *t, size_t h)
{
    for (size_t j = 0; j < 128; j += 4 * h) {
        for (size_t k = j; k < j + h; k++) {
            int32_t a = t[k], b = t[k + h], c = t[k + 2 * h], d = t[k + 3 * h];
            int32_t ab = a + b, amb = a - b, cd = c + d, cmd = c - d;

            t[k] = (int16_t)(ab + cd);
            t[k + h] = (int16_t)(amb + cmd);
            t[k + 2 * h] = (int16_t)(ab - cd);
            t[k + 3 * h] = (int16_t)(amb - cmd);
        }
    }
}

void reed_muller_decode(uint64_t *msg, const uint64_t *cdw)
{
    uint8_t *message_array = (uint8_t *)msg;
    const uint8_t *code = (const uint8_t *)cdw;
    int16_t t[128];

    for (size_t i = 0; i < VEC_N1_SIZE_BYTES; i++) {
        const uint8_t *block = code + 16 * C3_RM_COPIES * i;

        /* t[8*b + j] = number of copies in which bit (7 - j) of byte b is
           set. One 4-bit field per bit; the fields cannot overflow because
           there are at most 5 copies. */
        for (size_t b = 0; b < 16; b++) {
            uint32_t sum = 0;
            int16_t *d = t + 8 * b;

            for (size_t copy = 0; copy < C3_RM_COPIES; copy++) {
                sum += c3_gf_spread(block[16 * copy + b]);
            }
            d[0] = (int16_t)(sum >> 28);
            d[1] = (int16_t)((sum >> 24) & 15);
            d[2] = (int16_t)((sum >> 20) & 15);
            d[3] = (int16_t)((sum >> 16) & 15);
            d[4] = (int16_t)((sum >> 12) & 15);
            d[5] = (int16_t)((sum >> 8) & 15);
            d[6] = (int16_t)((sum >> 4) & 15);
            d[7] = (int16_t)(sum & 15);
        }

        /* Hadamard transform, in place: strides 1 and 2, 4 and 8, 16 and
           32, then 64. */
        c3_rm_hadamard_2levels(t, 1);
        c3_rm_hadamard_2levels(t, 4);
        c3_rm_hadamard_2levels(t, 16);
        for (size_t k = 0; k < 64; k++) {
            int32_t a = t[k], c = t[k + 64];
            t[k] = (int16_t)(a + c);
            t[k + 64] = (int16_t)(a - c);
        }
        t[0] -= 64 * C3_RM_COPIES;

        /* First position with the largest absolute value; bit 7 of the
           decoded byte is set if the value there is positive. */
        uint32_t best = 0, result = 0;
        for (uint32_t k = 0; k < 128; k++) {
            int32_t x = t[k];
            int32_t sign = x >> 31;
            uint32_t a = (uint32_t)((x ^ sign) - sign);
            uint32_t take = (uint32_t)((int32_t)(best - a) >> 31);
            uint32_t cand = k + ((uint32_t)(sign + 1) << 7);

            C3_RM_OPAQUE(take);
            best ^= (best ^ a) & take;
            result ^= (result ^ cand) & take;
        }
        message_array[i] = (uint8_t)result;
    }
}
