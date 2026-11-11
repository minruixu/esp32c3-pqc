/* Hash primitives: time per block and self-tests.
 *
 * Variants in the job names:
 *   sha256/ref   PQClean portable C (the project's reference backend)
 *   sha256/opt   ESP32-C3 SHA accelerator behind pqc_sha256_compress()
 *   sha256/raw   the accelerator used directly, state kept in register format
 *   sha256/up    slhdsa-c's own portable C
 *   sm3/ref      straightforward SM3      sm3/opt   tuned for RV32IMC
 *   sha512/ref   PQClean portable C       sha512/up slhdsa-c's portable C
 *   keccak/ref   PQClean portable C       keccak/opt the optimized backend
 *   keccak/up    slhdsa-c's portable C
 */
#include <string.h>
#include "bench.h"
#include "pqc_prims.h"
#include "pqc_sm3.h"
#include "sha2.h"

/* slhdsa-c's primitives (third_party/slhdsa-c, built with SLH_PQC_PRIMS) */
void sha2_256_compress_c(void *v);
void keccak_f1600_c(uint64_t x[25]);
void sha2_512_compress_c(void *v);
void sha2_256_compress(void *v) { sha2_256_compress_c(v); }
void sha2_512_compress(void *v) { sha2_512_compress_c(v); }


#define STACK (8 * 1024)

static uint32_t st32[8];
static uint64_t st64[25];
static uint8_t blocks[16 * 64 + 4];
static uint32_t upbuf32[8 + 16];
static uint64_t upbuf64[8 + 16];
static sha512ctx c512;

#ifndef PQC_HOST
#include "sha256_hw.h"
void hw_probe(void);
/* components/prims/opt/sha512_rv32imc.S */
void pqc_sha512_compress_rv32(uint32_t state[16], const uint32_t block[32]);
static uint32_t st512[16], w512[32];
static int j_sha512_opt_1(void *a) { for (int i = 0; i < 200; i++) pqc_sha512_compress_rv32(st512, w512); (void)a; return 0; }
/* the same with the conversion from a block of bytes that a caller without
 * word access to its data has to do */
static int j_sha512_opt_bytes(void *a)
{
    (void)a;
    for (int i = 0; i < 200; i++) {
        for (int k = 0; k < 32; k++) {
            w512[k] = ((uint32_t)blocks[4 * k] << 24) | ((uint32_t)blocks[4 * k + 1] << 16) |
                      ((uint32_t)blocks[4 * k + 2] << 8) | blocks[4 * k + 3];
        }
        pqc_sha512_compress_rv32(st512, w512);
    }
    return 0;
}
#endif

/* ---------- timing jobs ---------- */
static int j_sha256_ref_1(void *a) { for (int i = 0; i < 1000; i++) pqc_sha256_compress_ref(st32, blocks, 1); (void)a; return 0; }
static int j_sha256_opt_1(void *a) { for (int i = 0; i < 1000; i++) pqc_sha256_compress_opt(st32, blocks, 1); (void)a; return 0; }
static int j_sha256_ref_16(void *a) { for (int i = 0; i < 64; i++) pqc_sha256_compress_ref(st32, blocks, 16); (void)a; return 0; }
static int j_sha256_opt_16(void *a) { for (int i = 0; i < 64; i++) pqc_sha256_compress_opt(st32, blocks, 16); (void)a; return 0; }
static int j_sha256_up_1(void *a) { for (int i = 0; i < 1000; i++) sha2_256_compress_c(upbuf32); (void)a; return 0; }
/* A whole 55-byte message, the largest that fits one block: what one step of
 * a hash chain costs through the ordinary hash interface. */
static uint8_t md[32];
static int j_sha256_msg55(void *a) { for (int i = 0; i < 1000; i++) sha256(md, blocks, 55); (void)a; return 0; }
static int j_sm3_msg55(void *a) { for (int i = 0; i < 1000; i++) pqc_sm3(md, blocks, 55); (void)a; return 0; }
static int j_sm3_ref_1(void *a) { for (int i = 0; i < 1000; i++) pqc_sm3_compress_ref(st32, blocks, 1); (void)a; return 0; }
static int j_sm3_opt_1(void *a) { for (int i = 0; i < 1000; i++) pqc_sm3_compress_opt(st32, blocks, 1); (void)a; return 0; }
static int j_sha512_ref_1(void *a) { for (int i = 0; i < 200; i++) sha512_inc_blocks(&c512, blocks, 1); (void)a; return 0; }
static int j_sha512_up_1(void *a) { for (int i = 0; i < 200; i++) sha2_512_compress_c(upbuf64); (void)a; return 0; }
static int j_keccak_ref(void *a) { for (int i = 0; i < 200; i++) pqc_keccakf1600_ref(st64); (void)a; return 0; }
static int j_keccak_opt(void *a) { for (int i = 0; i < 200; i++) pqc_keccakf1600_opt(st64); (void)a; return 0; }
static int j_keccak_up(void *a) { for (int i = 0; i < 200; i++) keccak_f1600_c(st64); (void)a; return 0; }

#ifndef PQC_HOST
/* What one SLH-DSA F call costs on the accelerator: load a stored state,
 * write one block, read n bytes of the result. */
static uint32_t raw_out[8];
static int j_sha256_short55(void *a) { for (int i = 0; i < 1000; i++) pqc_sha256_hw_short(md, blocks, 55); (void)a; return 0; }
static int j_sha256_raw_n32(void *a)
{
    const uint32_t *b = (const uint32_t *)(const void *)blocks;
    (void)a;
    for (int i = 0; i < 1000; i++) {
        pqc_sha256_hw_load(st32);
        pqc_sha256_hw_block(b);
        pqc_sha256_hw_read(raw_out, 8);
    }
    return 0;
}
static int j_sha256_raw_n16(void *a)
{
    const uint32_t *b = (const uint32_t *)(const void *)blocks;
    (void)a;
    for (int i = 0; i < 1000; i++) {
        pqc_sha256_hw_load(st32);
        pqc_sha256_hw_block(b);
        pqc_sha256_hw_read(raw_out, 4);
    }
    return 0;
}
#endif

/* ---------- self-tests ---------- */
static int eq_sha256, eq_sm3, eq_sha512 = 1;

/* Random chaining values and 1..5 blocks at all four alignments. */
static int j_equal(void *a)
{
    (void)a;
    eq_sha256 = eq_sm3 = 1;
    for (int i = 0; i < 3000; i++) {
        uint32_t s1[8], s2[8];
        uint8_t n;
        const uint8_t *p = blocks + (i & 3);

        randombytes((uint8_t *)s1, sizeof(s1));
        randombytes(blocks, 5 * 64 + 4);
        randombytes(&n, 1);
        n = 1 + n % 5;
        memcpy(s2, s1, sizeof(s1));
        pqc_sha256_compress_ref(s1, p, n);
        pqc_sha256_compress_opt(s2, p, n);
        if (memcmp(s1, s2, sizeof(s1)) != 0) eq_sha256 = 0;

        randombytes((uint8_t *)s1, sizeof(s1));
        memcpy(s2, s1, sizeof(s1));
        pqc_sm3_compress_ref(s1, p, n);
        pqc_sm3_compress_opt(s2, p, n);
        if (memcmp(s1, s2, sizeof(s1)) != 0) eq_sm3 = 0;
    }
    /* Whole messages through the hash interface. They start from the initial
     * value, which the accelerator loads itself. */
    for (int i = 0; i < 300; i++) {
        uint8_t a[32], b[32];
        uint16_t len;

        randombytes((uint8_t *)&len, 2);
        len %= (i & 1) ? 700 : 56;
        randombytes(blocks, len);
        pqc_prims_select_ex(0, 0, 0);
        sha256(a, blocks, len);
        pqc_prims_select_ex(0, 1, 0);
        sha256(b, blocks, len);
        if (memcmp(a, b, 32) != 0) eq_sha256 = 0;
#ifndef PQC_HOST
        if (len <= 55) {
            pqc_sha256_hw_short(b, blocks + (i & 3), len);
            pqc_prims_select_ex(0, 0, 0);
            sha256(a, blocks + (i & 3), len);
            if (memcmp(a, b, 32) != 0) eq_sha256 = 0;
        }
#endif
    }
    pqc_prims_select(PQC_PRIMS_REF);
#ifndef PQC_HOST
    /* SHA-512: the assembly version against PQClean's C, 1 to 3 blocks, the
     * input at all four alignments. */
    for (int i = 0; i < 500; i++) {
        static sha512ctx c;
        uint32_t st[16];
        uint8_t n;
        const uint8_t *p = blocks + (i & 3);

        randombytes(c.ctx, 64);
        memset(c.ctx + 64, 0, 8);
        randombytes(blocks, 3 * 128 + 4);
        randombytes(&n, 1);
        n = 1 + n % 3;
        for (int k = 0; k < 16; k++) {
            st[k] = ((uint32_t)c.ctx[4 * k] << 24) | ((uint32_t)c.ctx[4 * k + 1] << 16) |
                    ((uint32_t)c.ctx[4 * k + 2] << 8) | c.ctx[4 * k + 3];
        }
        sha512_inc_blocks(&c, p, n);
        for (int b = 0; b < n; b++) {
            uint32_t w[32];
            for (int k = 0; k < 32; k++) {
                const uint8_t *q = p + 128 * b + 4 * k;
                w[k] = ((uint32_t)q[0] << 24) | ((uint32_t)q[1] << 16) | ((uint32_t)q[2] << 8) | q[3];
            }
            pqc_sha512_compress_rv32(st, w);
        }
        for (int k = 0; k < 16; k++) {
            uint32_t w = ((uint32_t)c.ctx[4 * k] << 24) | ((uint32_t)c.ctx[4 * k + 1] << 16) |
                         ((uint32_t)c.ctx[4 * k + 2] << 8) | c.ctx[4 * k + 3];
            if (w != st[k]) eq_sha512 = 0;
        }
    }
#endif
    return 0;
}

static int hexeq(const uint8_t *got, const char *hex, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned v = 0;
        for (int k = 0; k < 2; k++) {
            char c = hex[2 * i + k];
            v = v * 16 + (unsigned)(c <= '9' ? c - '0' : c - 'a' + 10);
        }
        if (got[i] != v) return 0;
    }
    return 1;
}

static void known_answers(const char *vn)
{
    static const char abc448[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    static uint8_t msg[1000], abcd64[64];
    uint8_t h[64];
    int ok;

    /* FIPS 180-4 examples */
    sha256(h, (const uint8_t *)"abc", 3);
    ok = hexeq(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 32);
    sha256(h, (const uint8_t *)abc448, 56);
    ok &= hexeq(h, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", 32);
    for (int i = 0; i < 1000; i++) msg[i] = 'a';
    sha256(h, msg, 1000);
    ok &= hexeq(h, "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3", 32);
    bench_check(bench_name("sha256", vn, "fips180-vectors"), ok);

    /* GB/T 32905-2016 examples */
    pqc_sm3(h, (const uint8_t *)"abc", 3);
    ok = hexeq(h, "66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0", 32);
    for (int i = 0; i < 64; i++) abcd64[i] = (uint8_t)("abcd"[i & 3]);
    pqc_sm3(h, abcd64, 64);
    ok &= hexeq(h, "debe9ff92275b8a138604889c18e5a4d6fdb70e5387e5765293dcba39c0c5732", 32);
    bench_check(bench_name("sm3", vn, "gbt32905-vectors"), ok);

    /* Longer inputs, fingerprinted for the comparison with the host. */
    for (int i = 0; i < 1000; i++) msg[i] = (uint8_t)(i * 7 + 1);
    sha256(h, msg, 1000);
    pqc_sm3(h + 32, msg, 999);
    bench_digest(bench_name("sha256+sm3", vn, "1000-byte-message"), h, 64);
}

void suite_main(void)
{
    bench_rng_seed(7);
    randombytes((uint8_t *)st32, sizeof(st32));
    randombytes((uint8_t *)st64, sizeof(st64));
    randombytes(blocks, sizeof(blocks));
    randombytes((uint8_t *)upbuf32, sizeof(upbuf32));
    randombytes((uint8_t *)upbuf64, sizeof(upbuf64));
    sha512_inc_init(&c512);

    bench_run("sha256/ref/1block-x1000", j_sha256_ref_1, NULL, 5, STACK);
    bench_run("sha256/opt/1block-x1000", j_sha256_opt_1, NULL, 5, STACK);
    bench_run("sha256/ref/16blocks-x64", j_sha256_ref_16, NULL, 5, STACK);
    bench_run("sha256/opt/16blocks-x64", j_sha256_opt_16, NULL, 5, STACK);
    bench_run("sha256/up/1block-x1000", j_sha256_up_1, NULL, 5, STACK);
#ifndef PQC_HOST
    bench_run("sha256/raw/55-byte-message-x1000", j_sha256_short55, NULL, 5, STACK);
    bench_run("sha256/raw/1block-read32-x1000", j_sha256_raw_n32, NULL, 5, STACK);
    bench_run("sha256/raw/1block-read16-x1000", j_sha256_raw_n16, NULL, 5, STACK);
#endif
    pqc_prims_select(PQC_PRIMS_REF);
    bench_run("sha256/ref/55-byte-message-x1000", j_sha256_msg55, NULL, 5, STACK);
    bench_run("sm3/ref/55-byte-message-x1000", j_sm3_msg55, NULL, 5, STACK);
    pqc_prims_select(PQC_PRIMS_OPT);
    bench_run("sha256/opt/55-byte-message-x1000", j_sha256_msg55, NULL, 5, STACK);
    bench_run("sm3/opt/55-byte-message-x1000", j_sm3_msg55, NULL, 5, STACK);
    pqc_prims_select(PQC_PRIMS_REF);
    bench_run("sm3/ref/1block-x1000", j_sm3_ref_1, NULL, 5, STACK);
    bench_run("sm3/opt/1block-x1000", j_sm3_opt_1, NULL, 5, STACK);
    bench_run("sha512/ref/1block-x200", j_sha512_ref_1, NULL, 5, STACK);
    bench_run("sha512/up/1block-x200", j_sha512_up_1, NULL, 5, STACK);
#ifndef PQC_HOST
    bench_run("sha512/opt/1block-x200", j_sha512_opt_1, NULL, 5, STACK);
    bench_run("sha512/opt/1block-from-bytes-x200", j_sha512_opt_bytes, NULL, 5, STACK);
#endif
    bench_run("keccak/ref/x200", j_keccak_ref, NULL, 5, STACK);
    bench_run("keccak/opt/x200", j_keccak_opt, NULL, 5, STACK);
    bench_run("keccak/up/x200", j_keccak_up, NULL, 5, STACK);

    bench_rng_seed(11);
    bench_run("hashes/opt/equal-to-ref-3000-random", j_equal, NULL, 1, STACK);
    bench_check("sha256/opt/equal-to-ref-3000-random", eq_sha256);
    bench_check("sm3/opt/equal-to-ref-3000-random", eq_sm3);
#ifndef PQC_HOST
    bench_check("sha512/opt/equal-to-ref-500-random", eq_sha512);
#endif

    pqc_prims_select(PQC_PRIMS_REF);
    known_answers("ref");
    pqc_prims_select(PQC_PRIMS_OPT);
    known_answers("opt");
    pqc_prims_select(PQC_PRIMS_REF);

#ifndef PQC_HOST
    hw_probe();
#endif
}
