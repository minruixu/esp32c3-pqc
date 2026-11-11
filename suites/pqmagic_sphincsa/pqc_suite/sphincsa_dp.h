/* Compact storage of the table that SPHINCS-alpha's constant-sum encoding
 * uses: dp[i][j] = number of ways to write j as a sum of i digits in
 * [0, w-1]. The reference code keeps (l+1) x (s+1) 256-bit entries in static
 * memory, 278 KB for the smallest parameter set and up to 2.2 MB, which
 * no RAM-only ESP32-C3 image can hold. Here an entry takes only the bytes
 * its row needs and the symmetry dp[i][j] = dp[i][i(w-1)-j] halves the
 * rows: 50 KB (128f), 102 KB (128s), 128 KB (192f). One table is held at a
 * time, on the heap, and rebuilt when the parameter set changes. The values are the same,
 * so signatures are identical to those of the reference code. */
#ifndef SPHINCSA_DP_H
#define SPHINCSA_DP_H

#include <stdint.h>

/* Make the table for (l, w) current. Returns 0, or -1 if it does not fit. */
int sphincsa_dp_select(int l, int w);

/* out = dp[i][j] of the current table, as four little-endian 64-bit limbs. */
void sphincsa_dp_get(uint64_t out[4], int i, int j);

#endif
