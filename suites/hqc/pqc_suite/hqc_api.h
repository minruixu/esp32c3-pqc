/* KEM entry points of every HQC build in this suite.
 * hqc<level><variant>_: r = upstream reference, o = optimized for the ESP32-C3. */
#ifndef HQC_API_H
#define HQC_API_H

#include <stddef.h>
#include <stdint.h>

#define HQC_DECLARE(p)                                                       \
    int8_t p##hqc_kem_keygen(uint8_t *ek, uint8_t *dk);                      \
    int8_t p##hqc_kem_encaps(uint8_t *ss, uint8_t *ct, const uint8_t *ek);   \
    int8_t p##hqc_kem_decaps(uint8_t *ss, const uint8_t *dk, const uint8_t *ct);

HQC_DECLARE(hqc1r_)
HQC_DECLARE(hqc3r_)
HQC_DECLARE(hqc5r_)
HQC_DECLARE(hqc1o_)
HQC_DECLARE(hqc3o_)
HQC_DECLARE(hqc5o_)

#define HQC_SK_BYTES 32
#define HQC_SS_BYTES 32
#define HQC1_PK_BYTES 2241
#define HQC1_CT_BYTES 4433
#define HQC3_PK_BYTES 4514
#define HQC3_CT_BYTES 8978
#define HQC5_PK_BYTES 7237
#define HQC5_CT_BYTES 14421

/* hqc_glue.c: the next randombytes() calls of the KEM return these bytes
 * instead of the harness RNG output (used by the known-answer tests). */
void hqc_rng_script(const uint8_t *data, size_t len);
int hqc_randombytes(uint8_t *out, size_t outlen);

#endif /* HQC_API_H */
