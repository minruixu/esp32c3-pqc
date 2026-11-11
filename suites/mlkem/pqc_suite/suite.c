/* ML-KEM-512/768/1024: known-answer tests and timings. */
#include <string.h>
#include "bench.h"
#include "pqc_prims.h"
#include "mlkem_all.h"
#include "kat_vectors.h"

typedef struct {
    const char *name;
    size_t pkb, skb, ctb;
    int (*keypair)(uint8_t *pk, uint8_t *sk);
    int (*keypair_derand)(uint8_t *pk, uint8_t *sk, const uint8_t *coins);
    int (*enc)(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
    int (*enc_derand)(uint8_t *ct, uint8_t *ss, const uint8_t *pk, const uint8_t *coins);
    int (*dec)(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);
    const uint8_t *kat_pk, *kat_sk, *kat_ct, *kat_ss;
} kem_t;

#define KEM(L)                                                                      \
    {"mlkem" #L, MLKEM##L##_PUBLICKEYBYTES, MLKEM##L##_SECRETKEYBYTES,              \
     MLKEM##L##_CIPHERTEXTBYTES, mlkem##L##_keypair, mlkem##L##_keypair_derand,     \
     mlkem##L##_enc, mlkem##L##_enc_derand, mlkem##L##_dec, test_vector_pk_##L,     \
     test_vector_sk_##L, test_vector_ct_##L, test_vector_ss_##L}

static const kem_t kems[] = {KEM(512), KEM(768), KEM(1024)};

#define STACK (32 * 1024)
#define ITERS 10

static const kem_t *K;
static uint8_t pk[MLKEM1024_PUBLICKEYBYTES], sk[MLKEM1024_SECRETKEYBYTES];
static uint8_t ct[MLKEM1024_CIPHERTEXTBYTES], ss_a[MLKEM_BYTES], ss_b[MLKEM_BYTES];

/* Known-answer test against the vectors shipped with mlkem-native. */
static int job_kat(void *arg)
{
    uint8_t coins[2 * MLKEM_SYMBYTES];
    (void)arg;

    memcpy(coins, test_vector_d, MLKEM_SYMBYTES);
    memcpy(coins + MLKEM_SYMBYTES, test_vector_z, MLKEM_SYMBYTES);
    if (K->keypair_derand(pk, sk, coins) != 0) return 1;
    if (memcmp(pk, K->kat_pk, K->pkb) != 0) return 2;
    if (memcmp(sk, K->kat_sk, K->skb) != 0) return 3;
    if (K->enc_derand(ct, ss_a, K->kat_pk, test_vector_m) != 0) return 4;
    if (memcmp(ct, K->kat_ct, K->ctb) != 0) return 5;
    if (memcmp(ss_a, K->kat_ss, MLKEM_BYTES) != 0) return 6;
    if (K->dec(ss_b, K->kat_ct, K->kat_sk) != 0) return 7;
    if (memcmp(ss_b, K->kat_ss, MLKEM_BYTES) != 0) return 8;
    /* A corrupted ciphertext must not yield the real shared secret. */
    ct[0] ^= 1;
    if (K->dec(ss_b, ct, K->kat_sk) != 0) return 9;
    if (memcmp(ss_b, K->kat_ss, MLKEM_BYTES) == 0) return 10;
    return 0;
}

static int job_keygen(void *arg) { (void)arg; return K->keypair(pk, sk); }
static int job_encaps(void *arg) { (void)arg; return K->enc(ct, ss_a, pk); }
static int job_decaps(void *arg)
{
    (void)arg;
    if (K->dec(ss_b, ct, sk) != 0) return 1;
    return memcmp(ss_a, ss_b, MLKEM_BYTES) != 0;
}

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};

    for (int v = 0; v < 2; v++) {
        const char *vn = variants[v];
        pqc_prims_select(v);
        for (size_t i = 0; i < sizeof(kems) / sizeof(kems[0]); i++) {
            K = &kems[i];
            if (!bench_selected(bench_name(K->name, vn, ""))) {
                continue;
            }
            bench_info(K->name, "pk=%u sk=%u ct=%u ss=%u", (unsigned)K->pkb, (unsigned)K->skb,
                       (unsigned)K->ctb, (unsigned)MLKEM_BYTES);
            bench_check(bench_name(K->name, vn, "kat"),
                        bench_run(bench_name(K->name, vn, "kat"), job_kat, NULL, 1, STACK) == 0);

            bench_rng_seed(1);
            bench_run(bench_name(K->name, vn, "keygen"), job_keygen, NULL, ITERS, STACK);
            bench_rng_seed(2);
            bench_run(bench_name(K->name, vn, "encaps"), job_encaps, NULL, ITERS, STACK);
            bench_run(bench_name(K->name, vn, "decaps"), job_decaps, NULL, ITERS, STACK);

            bench_digest(bench_name(K->name, vn, "pk"), pk, K->pkb);
            bench_digest(bench_name(K->name, vn, "sk"), sk, K->skb);
            bench_digest(bench_name(K->name, vn, "ct"), ct, K->ctb);
            bench_digest(bench_name(K->name, vn, "ss"), ss_a, MLKEM_BYTES);
        }
    }
}
