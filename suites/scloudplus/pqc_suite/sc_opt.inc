/* ESP32-C3 optimizations of SCloud+, used by the "opt" variant only. The
 * upstream functions stay as they are (renamed *_upstream by the including
 * file) and remain the "ref" variant; the dispatchers at the end choose.
 *
 * 1. The two large matrix products work on pairs of coefficients. All
 *    arithmetic is modulo q = 2^10, the entries of A are 10-bit values and
 *    the secret entries are small, so two coefficients fit in the 16-bit
 *    halves of one 32-bit word and one multiplication does the work of two:
 *    with t = s + SC_BIAS (never negative) and a pair A2 = a0 | a1 << 16,
 *        (C2 + t * A2) & 0x03ff03ff
 *    updates two coefficients at once without a carry between the halves.
 *    The bias is taken out again at the end with the column (or row) sums
 *    of A. On a core without vector instructions this halves the loads,
 *    multiplications and stores of the inner loops.
 * 2. In the AES family the rows of A come from AES-128 in counter mode; the
 *    ESP32-C3 AES accelerator computes the counter blocks (hwaes.h).
 * 3. In the SM3 family a row of A is 24 one-block SM3 calls; the block is
 *    laid out once per row instead of allocated, copied and padded per call.
 *
 * Both give the same public keys and ciphertexts as the reference code;
 * the known-answer test of the suite runs on both variants. */
#include "hwaes.h"
#include "pqc_prims.h"

#define SC_BIAS 15u              /* secrets must lie in [-15, 15] */
#define SC_LANES 0x03ff03ffu     /* low 10 bits of both 16-bit halves */
#define SC_NPAIR ((scloudplus_nbar + 1) / 2)

/* 1 if every entry of s (a small value stored modulo 2^16) is in range. */
static int sc_small(const uint16_t *s, size_t count)
{
    uint32_t bad = 0;
    for (size_t i = 0; i < count; i++) {
        uint32_t t = (uint32_t)(int32_t)(int16_t)s[i] + SC_BIAS;
        bad |= t;
    }
    return bad <= 2 * SC_BIAS + 1; /* an OR of values in [0, 30] stays below 32 */
}

/* C = S*A + E with S of size mbar x m, C and E of size mbar x n. */
static void sc_mul_sa_e_opt(const uint8_t *seedA, const uint16_t *S, uint16_t *E, uint16_t *C)
{
    ref_a_generator gen;
    uint32_t a2[scloudplus_n / 2];   /* one row of A, two coefficients per word */
    uint32_t suma[scloudplus_n / 2]; /* column sums of A, same layout */
    uint32_t *c2 = (uint32_t *)(void *)C;

    if (C != E) {
        memcpy(C, E, (size_t)scloudplus_mbar * scloudplus_n * sizeof(uint16_t));
    }
    for (size_t w = 0; w < (size_t)scloudplus_mbar * scloudplus_n / 2; w++) {
        c2[w] &= SC_LANES;
    }
    memset(suma, 0, sizeof(suma));
    ref_a_generator_init(&gen, seedA);
    for (int i = 0; i < scloudplus_m; i++) {
        generate_a_row(&gen, (size_t)i, (uint16_t *)(void *)a2);
        for (int w = 0; w < scloudplus_n / 2; w++) {
            suma[w] = (suma[w] + a2[w]) & SC_LANES;
        }
        for (int p = 0; p < scloudplus_mbar; p++) {
            const uint32_t t = (uint32_t)(int32_t)(int16_t)S[(size_t)p * scloudplus_m + (size_t)i] + SC_BIAS;
            uint32_t *row = c2 + (size_t)p * (scloudplus_n / 2);

            for (int w = 0; w < scloudplus_n / 2; w += 2) {
                row[w] = (row[w] + t * a2[w]) & SC_LANES;
                row[w + 1] = (row[w + 1] + t * a2[w + 1]) & SC_LANES;
            }
        }
    }
    /* remove the bias: C -= SC_BIAS * (column sums), per 10-bit lane */
    for (int w = 0; w < scloudplus_n / 2; w++) {
        suma[w] = 0x04000400u - ((SC_BIAS * suma[w]) & SC_LANES);
    }
    for (int p = 0; p < scloudplus_mbar; p++) {
        uint32_t *row = c2 + (size_t)p * (scloudplus_n / 2);
        for (int w = 0; w < scloudplus_n / 2; w++) {
            row[w] = (row[w] + suma[w]) & SC_LANES;
        }
    }
    ref_a_generator_clear(&gen);
    memset(a2, 0, sizeof(a2));
    memset(suma, 0, sizeof(suma));
}

/* B = A*S + E with S of size n x nbar (stored by columns), B and E m x nbar. */
static void sc_mul_as_e_opt(const uint8_t *seedA, const uint16_t *S, const uint16_t *E, uint16_t *B)
{
    ref_a_generator gen;
    uint16_t a_row[scloudplus_n];
    /* the secret, biased, two columns per word: st[j][k / 2] */
    uint32_t st[scloudplus_n][SC_NPAIR];

    for (int j = 0; j < scloudplus_n; j++) {
        for (int k2 = 0; k2 < SC_NPAIR; k2++) {
            uint32_t lo = (uint32_t)(int32_t)(int16_t)S[(size_t)(2 * k2) * scloudplus_n + (size_t)j] + SC_BIAS;
            uint32_t hi = 0;
            if (2 * k2 + 1 < scloudplus_nbar) {
                hi = (uint32_t)(int32_t)(int16_t)S[(size_t)(2 * k2 + 1) * scloudplus_n + (size_t)j] + SC_BIAS;
            }
            st[j][k2] = lo | (hi << 16);
        }
    }
    ref_a_generator_init(&gen, seedA);
    for (int i = 0; i < scloudplus_m; i++) {
        uint32_t acc[SC_NPAIR];
        uint32_t rowsum = 0, corr;

        generate_a_row(&gen, (size_t)i, a_row);
        for (int k2 = 0; k2 < SC_NPAIR; k2++) {
            acc[k2] = 0;
        }
        for (int j = 0; j < scloudplus_n; j++) {
            const uint32_t a = a_row[j];
            const uint32_t *s2 = st[j];

            rowsum += a;
            for (int k2 = 0; k2 < SC_NPAIR; k2++) {
                acc[k2] = (acc[k2] + a * s2[k2]) & SC_LANES;
            }
        }
        corr = 1024u - ((SC_BIAS * rowsum) & 1023u);
        for (int k = 0; k < scloudplus_nbar; k++) {
            const uint32_t lane = (acc[k / 2] >> (16 * (k & 1))) & 1023u;
            const size_t idx = (size_t)i * scloudplus_nbar + (size_t)k;
            B[idx] = (uint16_t)((E[idx] + lane + corr) & 1023u);
        }
    }
    ref_a_generator_clear(&gen);
    memset(a_row, 0, sizeof(a_row));
    memset(st, 0, sizeof(st));
}

/* ---- dispatchers: these carry the names the rest of the code calls ---- */
void mul_sa_e(const uint8_t *seedA, const uint16_t *S, uint16_t *E, uint16_t *C)
{
    if (pqc_prims_selected() == PQC_PRIMS_OPT &&
        sc_small(S, (size_t)scloudplus_mbar * scloudplus_m)) {
        sc_mul_sa_e_opt(seedA, S, E, C);
    } else {
        SC_NAME(mul_sa_e_upstream)(seedA, S, E, C);
    }
}

void mul_as_e(const uint8_t *seedA, const uint16_t *S, const uint16_t *E, uint16_t *B)
{
    if (pqc_prims_selected() == PQC_PRIMS_OPT &&
        sc_small(S, (size_t)scloudplus_n * scloudplus_nbar)) {
        sc_mul_as_e_opt(seedA, S, E, B);
    } else {
        SC_NAME(mul_as_e_upstream)(seedA, S, E, B);
    }
}

#if defined(PQM_SC_SM3)
/* A row of A in the SM3 family is SM3(seedA || row || counter) for counter =
 * 1, 2, ...: 24 bytes, one block. The upstream helper allocates, copies and
 * pads for every 32 bytes; here the padded block is laid out once per row
 * and only the counter changes. */
int sm3_bounded_xof_seed_row(uint8_t *out, size_t outlen, const uint8_t *seed, uint32_t row)
{
    static const uint32_t iv[8] = {0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
                                   0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E};
    uint8_t block[64];
    uint32_t ct = 1;
    size_t done = 0;

    if (pqc_prims_selected() != PQC_PRIMS_OPT || scloudplus_seedA_bytes != 16) {
        return SC_NAME(sm3_bounded_xof_seed_row_upstream)(out, outlen, seed, row);
    }
    memset(block, 0, sizeof(block));
    memcpy(block, seed, 16);
    block[16] = (uint8_t)row;
    block[17] = (uint8_t)(row >> 8);
    block[18] = (uint8_t)(row >> 16);
    block[19] = (uint8_t)(row >> 24);
    block[24] = 0x80;
    block[63] = 24 * 8; /* message length in bits */
    while (done < outlen) {
        uint32_t st[8];
        uint8_t d[32];
        size_t take = outlen - done;

        block[20] = (uint8_t)(ct >> 24);
        block[21] = (uint8_t)(ct >> 16);
        block[22] = (uint8_t)(ct >> 8);
        block[23] = (uint8_t)ct;
        ct++;
        memcpy(st, iv, sizeof(st));
        pqc_sm3_compress(st, block, 1);
        for (int i = 0; i < 8; i++) {
            d[4 * i] = (uint8_t)(st[i] >> 24);
            d[4 * i + 1] = (uint8_t)(st[i] >> 16);
            d[4 * i + 2] = (uint8_t)(st[i] >> 8);
            d[4 * i + 3] = (uint8_t)st[i];
        }
        if (take > 32) {
            take = 32;
        }
        memcpy(out + done, d, take);
        done += take;
    }
    return 0;
}
#endif

#if defined(PQM_SC_AES)
void AES128_CTR_zero_sch(uint32_t counter_start, size_t block_count, const uint8_t *schedule,
                         uint8_t *ciphertext)
{
    /* the expanded key starts with the 16 key bytes */
    if (pqc_prims_selected() == PQC_PRIMS_OPT &&
        pqm_hwaes_ctr(ciphertext, block_count, schedule, 16, counter_start, 1)) {
        return;
    }
    SC_NAME(AES128_CTR_zero_sch_upstream)(counter_start, block_count, schedule, ciphertext);
}
#endif
