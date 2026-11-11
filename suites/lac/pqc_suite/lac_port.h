/* What suites/lac supplies in place of LAC's OpenSSL-based rand.c. */
#ifndef LAC_PORT_H
#define LAC_PORT_H

#include <stddef.h>
#include <stdint.h>

/* AES-256 in counter mode with an all-zero initial counter block: the first
 * `len` bytes of key stream. Software AES for the reference variant, the
 * ESP32-C3 AES accelerator for the optimized one. */
void lac_aes256ctr(uint8_t *out, size_t len, const uint8_t *key);

#endif
