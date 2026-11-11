/* A Keccak sponge for 32-bit little-endian targets: input is xored into the
 * state a word at a time and output is copied out with memcpy. mlkem-native
 * and mldsa-native move every byte in and out of the state one at a time,
 * which costs 7 to 9 cycles per byte on the ESP32-C3; this takes about one.
 * The permutation is pqc_keccakf1600() from components/prims. */
#ifndef PQC_C3_SPONGE_H
#define PQC_C3_SPONGE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    union {
        uint64_t s[25];
        uint8_t b[200];
    } u;
    unsigned pos; /* absorbing: bytes in the current block; squeezing: bytes taken from it */
} c3_sponge;

void c3_sponge_init(c3_sponge *c);
void c3_sponge_absorb(c3_sponge *c, unsigned rate, const uint8_t *in, size_t len);
void c3_sponge_finalize(c3_sponge *c, unsigned rate, uint8_t pad);
void c3_sponge_squeeze(c3_sponge *c, unsigned rate, uint8_t *out, size_t len);
void c3_sponge_wipe(c3_sponge *c);

/* one-shot: out = Keccak[rate, pad](in), outlen bytes */
void c3_sponge_hash(unsigned rate, uint8_t pad, uint8_t *out, size_t outlen, const uint8_t *in,
                    size_t inlen);

#define C3_SHAKE128_RATE 168
#define C3_SHAKE256_RATE 136
#define C3_SHA3_256_RATE 136
#define C3_SHA3_512_RATE 72
#define C3_PAD_SHAKE 0x1F
#define C3_PAD_SHA3 0x06

#endif /* PQC_C3_SPONGE_H */
