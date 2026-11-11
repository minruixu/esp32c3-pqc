/* Winternitz chains of LMS on the SHA accelerator of the ESP32-C3 (device
 * only). See components/prims/opt/sha256_hw.h for the register interface.
 *
 * A chain step hashes I || q || i || j || previous output: 23 + n bytes,
 * exactly one SHA-256 block with its padding. The accelerator starts every
 * step from the SHA-256 initial value by itself, and its H registers hold
 * the output as the byte string the next block needs. So a step is: read
 * the n/4 output words, write them back into the message registers shifted
 * by three bytes (the output starts at byte 23 of the block), write j, and
 * start. Nothing is converted and nothing goes through memory. The words of
 * the block that do not change are written once per chain.
 */
#include <string.h>
#include "lms.h"
#include "pqc_prims.h"
#include "sha256_hw.h"

int lms_chain_sha256_hw(const lms_param_t *P, const uint8_t *pre, unsigned i, unsigned from,
                        unsigned to, uint8_t *tmp)
{
    uint8_t blk[64];
    uint32_t w[16], h[8], base5;
    unsigned n = P->n, nw = n / 4, j, k;

    if (P->shake != 0 || pqc_sha256_compress_fn != pqc_sha256_compress_opt) {
        return 0; /* not SHA-256, or the hardware backend is not selected */
    }
    if (from >= to) {
        return 1;
    }
    /* the block as bytes, then as the words the message registers take */
    memcpy(blk, pre, 20);
    blk[20] = (uint8_t)(i >> 8);
    blk[21] = (uint8_t)i;
    blk[22] = 0;
    memset(blk + 23, 0, sizeof(blk) - 23);
    blk[62] = (uint8_t)(((23 + n) * 8) >> 8);
    blk[63] = (uint8_t)((23 + n) * 8);
    memcpy(w, blk, sizeof(w));
    base5 = w[5]; /* bytes 20 and 21; byte 22 is j, byte 23 the first output byte */
    memcpy(h, tmp, n);

    pqc_sha256_hw_init();
    for (k = 0; k < 16; k++) {
        if (k < 5 || k > 5 + nw) {
            PQC_SHA_M[k] = w[k];
        }
    }
#ifndef LMS_HW_CHAIN_LOOP
    /* Straight-line code for the two output sizes; the general loop below
     * is the same thing for any n. */
    if (nw == 8) {
        uint32_t h0 = h[0], h1 = h[1], h2 = h[2], h3 = h[3], h4 = h[4], h5 = h[5], h6 = h[6], h7 = h[7];

        for (j = from; j < to; j++) {
            PQC_SHA_M[5] = base5 | (j << 16) | (h0 << 24);
            PQC_SHA_M[6] = (h0 >> 8) | (h1 << 24);
            PQC_SHA_M[7] = (h1 >> 8) | (h2 << 24);
            PQC_SHA_M[8] = (h2 >> 8) | (h3 << 24);
            PQC_SHA_M[9] = (h3 >> 8) | (h4 << 24);
            PQC_SHA_M[10] = (h4 >> 8) | (h5 << 24);
            PQC_SHA_M[11] = (h5 >> 8) | (h6 << 24);
            PQC_SHA_M[12] = (h6 >> 8) | (h7 << 24);
            PQC_SHA_M[13] = (h7 >> 8) | 0x80000000u;
            REG_WRITE(SHA_START_REG, 1);
            pqc_sha256_hw_wait();
            h0 = PQC_SHA_H[0];
            h1 = PQC_SHA_H[1];
            h2 = PQC_SHA_H[2];
            h3 = PQC_SHA_H[3];
            h4 = PQC_SHA_H[4];
            h5 = PQC_SHA_H[5];
            h6 = PQC_SHA_H[6];
            h7 = PQC_SHA_H[7];
        }
        h[0] = h0; h[1] = h1; h[2] = h2; h[3] = h3;
        h[4] = h4; h[5] = h5; h[6] = h6; h[7] = h7;
        memcpy(tmp, h, n);
        return 1;
    }
    if (nw == 6) {
        uint32_t h0 = h[0], h1 = h[1], h2 = h[2], h3 = h[3], h4 = h[4], h5 = h[5];

        for (j = from; j < to; j++) {
            PQC_SHA_M[5] = base5 | (j << 16) | (h0 << 24);
            PQC_SHA_M[6] = (h0 >> 8) | (h1 << 24);
            PQC_SHA_M[7] = (h1 >> 8) | (h2 << 24);
            PQC_SHA_M[8] = (h2 >> 8) | (h3 << 24);
            PQC_SHA_M[9] = (h3 >> 8) | (h4 << 24);
            PQC_SHA_M[10] = (h4 >> 8) | (h5 << 24);
            PQC_SHA_M[11] = (h5 >> 8) | 0x80000000u;
            REG_WRITE(SHA_START_REG, 1);
            pqc_sha256_hw_wait();
            h0 = PQC_SHA_H[0];
            h1 = PQC_SHA_H[1];
            h2 = PQC_SHA_H[2];
            h3 = PQC_SHA_H[3];
            h4 = PQC_SHA_H[4];
            h5 = PQC_SHA_H[5];
        }
        h[0] = h0; h[1] = h1; h[2] = h2; h[3] = h3; h[4] = h4; h[5] = h5;
        memcpy(tmp, h, n);
        return 1;
    }
#endif
    for (j = from; j < to; j++) {
        PQC_SHA_M[5] = base5 | (j << 16) | (h[0] << 24);
        for (k = 1; k < nw; k++) {
            PQC_SHA_M[5 + k] = (h[k - 1] >> 8) | (h[k] << 24);
        }
        PQC_SHA_M[5 + nw] = (h[nw - 1] >> 8) | 0x80000000u;
        REG_WRITE(SHA_START_REG, 1);
        pqc_sha256_hw_wait();
        for (k = 0; k < nw; k++) {
            h[k] = PQC_SHA_H[k];
        }
    }
    memcpy(tmp, h, n);
    return 1;
}
