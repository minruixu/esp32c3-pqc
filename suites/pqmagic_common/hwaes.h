/* AES on the ESP32-C3 hardware accelerator, for schemes that expand seeds
 * with AES in counter mode (SCloud+ in its AES family, LAC). */
#ifndef PQM_HWAES_H
#define PQM_HWAES_H

#include <stddef.h>
#include <stdint.h>

/* Encrypt `nblocks` counter blocks with AES-128 (keybytes 16) or AES-256
 * (keybytes 32) and write the 16-byte results to `out`. Block i is all zero
 * except for the 32-bit counter first+i, stored little-endian in bytes 0..3
 * (little_endian != 0) or big-endian in bytes 12..15 (little_endian == 0).
 * Returns 1 when the accelerator did the work, 0 when there is none (host
 * build); the caller then uses its software AES. */
int pqm_hwaes_ctr(uint8_t *out, size_t nblocks, const uint8_t *key, unsigned keybytes,
                  uint32_t first, int little_endian);

#endif
