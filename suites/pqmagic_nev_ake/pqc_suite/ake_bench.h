/* Descriptor of one NEV-AKE parameter set for the suite's runner. */
#ifndef AKE_BENCH_H
#define AKE_BENCH_H

#include <stdint.h>

typedef struct {
    const char *name;
    uint32_t pkb, skb, m1b, stb, m2b, keyb;
    int (*party_keygen)(uint8_t *pk, uint8_t *sk);
    void (*init)(uint8_t *m1, uint8_t *st, const uint8_t *ski, const uint8_t *pkj);
    void (*der_response)(uint8_t *m2, uint8_t *key, const uint8_t *idi, const uint8_t *idj,
                         const uint8_t *skj, const uint8_t *pki, const uint8_t *m1);
    void (*der_init)(uint8_t *key, const uint8_t *idi, const uint8_t *idj, const uint8_t *ski,
                     const uint8_t *pkj, const uint8_t *st, const uint8_t *m2);
    uint32_t stack;
    int iters;
} ake_alg;

#endif
