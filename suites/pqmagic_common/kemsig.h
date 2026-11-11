/* Table-driven benchmark and self-test of KEMs and signature schemes that
 * have the usual keypair/enc/dec or keypair/sign/verify interface. Used by
 * the suites built from PQMagic, SCloud+ and LAC. */
#ifndef KEMSIG_H
#define KEMSIG_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int (*keypair)(unsigned char *pk, unsigned char *sk);
    int (*enc)(unsigned char *ct, unsigned char *ss, const unsigned char *pk);
    int (*dec)(unsigned char *ss, const unsigned char *ct, const unsigned char *sk);
} kem_ops;

typedef struct {
    const char *name;        /* job name prefix, for example "aigis-enc-2-sm3" */
    uint32_t pkb, skb, ctb, ssb;
    kem_ops ref;             /* the upstream code */
    const kem_ops *opt;      /* optimized build of the scheme; NULL = `ref`
                                code with the optimized primitives */
    uint32_t stack;          /* task stack of each job, bytes */
    int iters;
    unsigned flags;          /* KEM_* flags below */
} kem_alg;

/* The upstream decapsulation is known to accept a modified ciphertext.
 * Report what happens as information instead of as a failed check: the
 * checks of this project are about the port, not about the scheme. */
#define KEM_TAMPER_INFO 1u

typedef struct {
    int (*keypair)(unsigned char *pk, unsigned char *sk);
    int (*sign)(unsigned char *sig, size_t *siglen, const unsigned char *m, size_t mlen,
                const unsigned char *sk);
    int (*verify)(const unsigned char *sig, size_t siglen, const unsigned char *m, size_t mlen,
                  const unsigned char *pk);
} sig_ops;

typedef struct {
    const char *name;
    uint32_t pkb, skb, sigb; /* sigb: largest signature */
    sig_ops ref;
    const sig_ops *opt;
    uint32_t stack;
    int it_keygen, it_sign, it_verify;
} sig_alg;

/* Run every algorithm of the table twice: "ref" with the reference
 * primitives, "opt" with the optimized ones. */
/* The harness's randombytes() under the prototypes that some upstream
 * projects declare for their own; map their name to one of these. */
void kemsig_randombytes_ull(unsigned char *x, unsigned long long xlen);
void kemsig_randombytes_sz(unsigned char *x, size_t xlen);

/* The static buffer that kem_run() and sig_run() carve keys from, for
 * suite code that runs its own tests before them. */
uint8_t *kemsig_arena(size_t *size);

void kemsig_heap_info(void); /* device only: free heap and largest block */
void kem_run(const kem_alg *const *algs, size_t n);
void sig_run(const sig_alg *const *algs, size_t n);

#endif /* KEMSIG_H */
