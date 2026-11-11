/* SCloud+: known-answer test against the submission's own vectors, then
 * timings. The levels and hash families in the image are chosen by the
 * build (SC_HAVE_<level>_<family>). */
#include <string.h>
#include "bench.h"
#include "kemsig.h"
#include "pqc_prims.h"
#include "sha2.h"
#include "drng.h"
#include "kat_expected.h"

/* SCloud+ asks for random bytes through this function. During the
 * known-answer test it is the deterministic generator of the submission,
 * seeded from the test vector; otherwise the harness generator. */
static DRNG_ctx kat_drng;
static int kat_mode;

int sc_randombytes(unsigned char *buffer, unsigned int size)
{
    if (kat_mode) {
        return get_random_number(&kat_drng, buffer, (unsigned long long)size * 8ULL);
    }
    return randombytes(buffer, size);
}

typedef struct {
    const kem_alg *alg;
    const scloudplus_kat *kat;
} kat_case;

#ifdef SC_HAVE_128_sm3
extern const kem_alg sc128_sm3_alg;
#endif
#ifdef SC_HAVE_128_shake
extern const kem_alg sc128_shake_alg;
#endif
#ifdef SC_HAVE_128_aes
extern const kem_alg sc128_aes_alg;
#endif
#ifdef SC_HAVE_192_sm3
extern const kem_alg sc192_sm3_alg;
#endif
#ifdef SC_HAVE_192_shake
extern const kem_alg sc192_shake_alg;
#endif
#ifdef SC_HAVE_192_aes
extern const kem_alg sc192_aes_alg;
#endif
#ifdef SC_HAVE_256_sm3
extern const kem_alg sc256_sm3_alg;
#endif
#ifdef SC_HAVE_256_shake
extern const kem_alg sc256_shake_alg;
#endif
#ifdef SC_HAVE_256_aes
extern const kem_alg sc256_aes_alg;
#endif
#ifdef SC_HAVE_384_sm3
extern const kem_alg sc384_sm3_alg;
#endif
#ifdef SC_HAVE_384_shake
extern const kem_alg sc384_shake_alg;
#endif
#ifdef SC_HAVE_384_aes
extern const kem_alg sc384_aes_alg;
#endif
#ifdef SC_HAVE_512_sm3
extern const kem_alg sc512_sm3_alg;
#endif
#ifdef SC_HAVE_512_shake
extern const kem_alg sc512_shake_alg;
#endif
#ifdef SC_HAVE_512_aes
extern const kem_alg sc512_aes_alg;
#endif

static const kat_case cases[] = {
#ifdef SC_HAVE_128_sm3
    {&sc128_sm3_alg, &scloudplus_kat_128_sm3},
#endif
#ifdef SC_HAVE_128_shake
    {&sc128_shake_alg, &scloudplus_kat_128_shake},
#endif
#ifdef SC_HAVE_128_aes
    {&sc128_aes_alg, &scloudplus_kat_128_aes},
#endif
#ifdef SC_HAVE_192_sm3
    {&sc192_sm3_alg, &scloudplus_kat_192_sm3},
#endif
#ifdef SC_HAVE_192_shake
    {&sc192_shake_alg, &scloudplus_kat_192_shake},
#endif
#ifdef SC_HAVE_192_aes
    {&sc192_aes_alg, &scloudplus_kat_192_aes},
#endif
#ifdef SC_HAVE_256_sm3
    {&sc256_sm3_alg, &scloudplus_kat_256_sm3},
#endif
#ifdef SC_HAVE_256_shake
    {&sc256_shake_alg, &scloudplus_kat_256_shake},
#endif
#ifdef SC_HAVE_256_aes
    {&sc256_aes_alg, &scloudplus_kat_256_aes},
#endif
#ifdef SC_HAVE_384_sm3
    {&sc384_sm3_alg, &scloudplus_kat_384_sm3},
#endif
#ifdef SC_HAVE_384_shake
    {&sc384_shake_alg, &scloudplus_kat_384_shake},
#endif
#ifdef SC_HAVE_384_aes
    {&sc384_aes_alg, &scloudplus_kat_384_aes},
#endif
#ifdef SC_HAVE_512_sm3
    {&sc512_sm3_alg, &scloudplus_kat_512_sm3},
#endif
#ifdef SC_HAVE_512_shake
    {&sc512_shake_alg, &scloudplus_kat_512_shake},
#endif
#ifdef SC_HAVE_512_aes
    {&sc512_aes_alg, &scloudplus_kat_512_aes},
#endif
};
#define NCASES (sizeof(cases) / sizeof(cases[0]))

static int same_sha256(const uint8_t *data, size_t len, const unsigned char expect[32])
{
    uint8_t h[32];
    sha256(h, data, len);
    return memcmp(h, expect, 32) == 0;
}

/* First vector of the submission's KAT file: seed the generator, then key
 * generation, encapsulation and decapsulation must give the listed bytes. */
static int job_kat(void *arg)
{
    const kat_case *c = arg;
    const kem_alg *a = c->alg;
    size_t size;
    uint8_t *pk = kemsig_arena(&size);
    uint8_t *sk = pk + a->pkb, *ct = sk + a->skb, *ss = ct + a->ctb, *ss2 = ss + a->ssb;
    int rc;

    if ((size_t)a->pkb + a->skb + a->ctb + 2 * (size_t)a->ssb > size) return 1;
    if (init_random_number(&kat_drng, c->kat->seed, 64) != 0) return 2;
    kat_mode = 1;
    rc = a->ref.keypair(pk, sk);
    rc = rc || a->ref.enc(ct, ss, pk);
    rc = rc || a->ref.dec(ss2, ct, sk);
    kat_mode = 0;
    if (rc) return 3;
    if (!same_sha256(pk, a->pkb, c->kat->pk)) return 4;
    if (!same_sha256(sk, a->skb, c->kat->sk)) return 5;
    if (!same_sha256(ct, a->ctb, c->kat->ct)) return 6;
    if (!same_sha256(ss, a->ssb, c->kat->ss)) return 7;
    if (memcmp(ss, ss2, a->ssb) != 0) return 8;
    return 0;
}

void suite_main(void)
{
    static const char *const variants[] = {"ref", "opt"};
    const kem_alg *algs[NCASES];

    kemsig_heap_info();
    for (int v = 0; v < 2; v++) {
        pqc_prims_select(v);
        for (size_t i = 0; i < NCASES; i++) {
            const char *nm = bench_name(cases[i].alg->name, variants[v], "kat");
            if (bench_selected(nm)) {
                bench_check(nm, bench_run(nm, job_kat, (void *)&cases[i], 1, cases[i].alg->stack) == 0);
            }
        }
    }
    for (size_t i = 0; i < NCASES; i++) {
        algs[i] = cases[i].alg;
    }
    kem_run(algs, NCASES);
}
