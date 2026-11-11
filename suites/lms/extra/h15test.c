/* Host-only check of the three-layer (H15) signing key, which is too slow to
 * build on the ESP32-C3: every signature up to
 * q = 2200 verifies, and keys reloaded at several positions sign identically.
 *
 * Build and run from the repository root (about a minute):
 *   cc -O2 -o /tmp/h15test suites/lms/extra/h15test.c suites/lms/pqc_suite/lms.c \
 *      components/prims/prims.c components/prims/fips202.c components/prims/sha2.c \
 *      components/prims/sm3.c -Isuites/lms/pqc_suite -Icomponents/prims/include
 *   /tmp/h15test
 */
#include <stdio.h>
#include <string.h>
#include "lms.h"
#include "pqc_prims.h"

static lms_key_t a, b;
static uint8_t pk[64], sig[4000], sig2[4000];

int main(void)
{
    uint8_t seed[32], I[16];
    size_t n, n2;
    const uint8_t msg[] = "three layers";
    static const uint32_t reload_at[] = {31, 32, 1023, 1024, 1500, 2047, 2048, 2199};
    unsigned r = 0;

    for (int i = 0; i < 32; i++) seed[i] = (uint8_t)(i * 7 + 1);
    for (int i = 0; i < 16; i++) I[i] = (uint8_t)(0xc0 + i);
    lms_fast = 1;
    if (lms_key_load(&a, 0x07, 0x03, seed, I, 0) != 0) { puts("load failed"); return 1; }
    lms_public_key(&a, pk);
    for (uint32_t q = 0; q < 2200; q++) {
        if (lms_sign(&a, msg, sizeof(msg), sig, &n) != 0) { printf("sign failed at %u\n", q); return 1; }
        if (lms_verify(pk, lms_pk_bytes(&a.P), msg, sizeof(msg), sig, n) != 0) { printf("verify failed at %u\n", q); return 1; }
        if (r < sizeof(reload_at) / sizeof(reload_at[0]) && q == reload_at[r]) {
            if (lms_key_load(&b, 0x07, 0x03, seed, I, q) != 0) { puts("reload failed"); return 1; }
            if (lms_sign(&b, msg, sizeof(msg), sig2, &n2) != 0 || n2 != n || memcmp(sig, sig2, n) != 0) {
                printf("reloaded key differs at %u\n", q); return 1;
            }
            /* the reloaded key must also keep working across the next boundary */
            for (int j = 0; j < 40; j++) {
                if (lms_sign(&b, msg, sizeof(msg), sig2, &n2) != 0 ||
                    lms_verify(pk, lms_pk_bytes(&a.P), msg, sizeof(msg), sig2, n2) != 0) {
                    printf("reloaded key failed %d after %u\n", j, q); return 1;
                }
            }
            r++;
        }
    }
    printf("H15: 2200 signatures verified, %u reload points identical\n", r);
    return 0;
}
