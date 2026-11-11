/* Parts of the harness shared by the device and the host build. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "bench_priv.h"
#include "pqc_prims.h"

char bench_filter[BENCH_FILTER_MAX];
int bench_fails;
int bench_results;

/* ---------- filter: comma-separated substrings, empty = everything ---------- */
int bench_selected(const char *name)
{
    const char *p = bench_filter;

    if (*p == '\0') {
        return 1;
    }
    while (*p) {
        const char *q = strchr(p, ',');
        size_t n = q ? (size_t)(q - p) : strlen(p);
        char pat[BENCH_FILTER_MAX];

        memcpy(pat, p, n);
        pat[n] = '\0';
        if (n && strstr(name, pat)) {
            return 1;
        }
        p += n + (q ? 1 : 0);
    }
    return 0;
}

const char *bench_name(const char *alg, const char *variant, const char *op)
{
    static char buf[BENCH_NAME_MAX];
    snprintf(buf, sizeof(buf), "%s/%s/%s", alg, variant, op);
    return buf;
}

/* ---------- deterministic RNG: xoshiro128** seeded by splitmix32 ---------- */
static uint32_t rng_s[4] = {0x9e3779b9u, 0x243f6a88u, 0xb7e15162u, 0x85a308d3u};
static uint32_t rng_word;
static int rng_left;

static uint32_t rotl32(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }

void bench_rng_seed(uint32_t seed)
{
    uint32_t z = seed;
    for (int i = 0; i < 4; i++) {
        uint32_t x = (z += 0x9e3779b9u);
        x = (x ^ (x >> 16)) * 0x85ebca6bu;
        x = (x ^ (x >> 13)) * 0xc2b2ae35u;
        rng_s[i] = x ^ (x >> 16);
    }
    rng_left = 0;
}

static uint32_t rng_next(void)
{
    uint32_t result = rotl32(rng_s[1] * 5, 7) * 9;
    uint32_t t = rng_s[1] << 9;
    rng_s[2] ^= rng_s[0];
    rng_s[3] ^= rng_s[1];
    rng_s[1] ^= rng_s[2];
    rng_s[0] ^= rng_s[3];
    rng_s[2] ^= t;
    rng_s[3] = rotl32(rng_s[3], 11);
    return result;
}

int randombytes(uint8_t *out, size_t outlen)
{
    while (outlen--) {
        if (rng_left == 0) {
            rng_word = rng_next();
            rng_left = 4;
        }
        *out++ = (uint8_t)rng_word;
        rng_word >>= 8;
        rng_left--;
    }
    return 0;
}

/* ---------- SHA-256 fingerprint, always with the reference backend ---------- */
static void sha256_ref(uint8_t out[32], const uint8_t *in, size_t len)
{
    uint32_t st[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    uint8_t last[128];
    size_t full = len / 64, rem = len % 64, n;
    uint64_t bits = (uint64_t)len * 8;

    pqc_sha256_compress_ref(st, in, full);
    memset(last, 0, sizeof(last));
    memcpy(last, in + 64 * full, rem);
    last[rem] = 0x80;
    n = (rem < 56) ? 64 : 128;
    for (int i = 0; i < 8; i++) {
        last[n - 1 - i] = (uint8_t)(bits >> (8 * i));
    }
    pqc_sha256_compress_ref(st, last, n / 64);
    for (int i = 0; i < 8; i++) {
        out[4 * i] = (uint8_t)(st[i] >> 24);
        out[4 * i + 1] = (uint8_t)(st[i] >> 16);
        out[4 * i + 2] = (uint8_t)(st[i] >> 8);
        out[4 * i + 3] = (uint8_t)st[i];
    }
}

void bench_digest(const char *label, const void *data, size_t len)
{
    uint8_t h[32];
    char hex[33];

    if (!bench_selected(label)) {
        return;
    }
    sha256_ref(h, (const uint8_t *)data, len);
    for (int i = 0; i < 16; i++) {
        snprintf(hex + 2 * i, 3, "%02x", h[i]);
    }
    printf("@DIGEST|%s|%s|%u\n", label, hex, (unsigned)len);
    fflush(stdout);
}

void bench_check(const char *label, int ok)
{
    if (!bench_selected(label)) {
        return;
    }
    if (!ok) {
        bench_fails++;
    }
    printf("@CHECK|%s|%s\n", label, ok ? "PASS" : "FAIL");
    fflush(stdout);
}

void bench_info(const char *key, const char *fmt, ...)
{
    char buf[160];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("@INFO|%s|%s\n", key, buf);
    fflush(stdout);
}
