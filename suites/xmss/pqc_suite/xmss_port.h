/* Glue between the unchanged XMSS reference code and this project. */
#ifndef XMSS_PORT_H
#define XMSS_PORT_H

#include <stddef.h>
#include <stdint.h>
#include "params.h"

/* 0: the reference hash layer (hash.c of xmss-reference).
 * 1: the hash layer of hash_opt.c. Same outputs either way. */
extern int xmss_fast;

/* Forget the cached SHA-256 states, which are derived from key material. */
void xmss_hash_cache_clear(void);

/* Optional replacements for PRF, F and H when xmss_fast is set. Each returns
 * 1 if it did the work and 0 to fall back. hash_hw.c provides them for the
 * SHA accelerator of the ESP32-C3 (xmss_sha_hw_*). */
extern int (*xmss_hw_prf)(const xmss_params *params, unsigned char *out, const unsigned char in[32],
                          const unsigned char *key);
extern int (*xmss_hw_thash_f)(const xmss_params *params, unsigned char *out, const unsigned char *in,
                              const unsigned char *pub_seed, uint32_t addr[8]);
extern int (*xmss_hw_thash_h)(const xmss_params *params, unsigned char *out, const unsigned char *in,
                              const unsigned char *pub_seed, uint32_t addr[8]);
int xmss_sha_hw_prf(const xmss_params *params, unsigned char *out, const unsigned char in[32],
                    const unsigned char *key);
int xmss_sha_hw_thash_f(const xmss_params *params, unsigned char *out, const unsigned char *in,
                        const unsigned char *pub_seed, uint32_t addr[8]);
int xmss_sha_hw_thash_h(const xmss_params *params, unsigned char *out, const unsigned char *in,
                        const unsigned char *pub_seed, uint32_t addr[8]);
void xmss_hw_cache_clear(void);

/* If set, the next key generation takes its 3n seed bytes from here instead
 * of the random generator (SK_SEED || SK_PRF || PUB_SEED). */
extern const uint8_t *xmss_port_seed;

/* The reference hash layer under other names (hash_ref.c). */
void xmss_ref_addr_to_bytes(unsigned char *bytes, const uint32_t addr[8]);
int xmss_ref_prf(const xmss_params *params, unsigned char *out, const unsigned char in[32],
                 const unsigned char *key);
int xmss_ref_prf_keygen(const xmss_params *params, unsigned char *out, const unsigned char *in,
                        const unsigned char *key);
int xmss_ref_hash_message(const xmss_params *params, unsigned char *out, const unsigned char *R,
                          const unsigned char *root, unsigned long long idx,
                          unsigned char *m_with_prefix, unsigned long long mlen);
int xmss_ref_thash_h(const xmss_params *params, unsigned char *out, const unsigned char *in,
                     const unsigned char *pub_seed, uint32_t addr[8]);
int xmss_ref_thash_f(const xmss_params *params, unsigned char *out, const unsigned char *in,
                     const unsigned char *pub_seed, uint32_t addr[8]);

/* The reference core without tree traversal state (xmss_core.c), used on
 * the host to reproduce the published signature test vectors. */
int xmss_slow_core_sign(const xmss_params *params, unsigned char *sk, unsigned char *sm,
                        unsigned long long *smlen, const unsigned char *m, unsigned long long mlen);
int xmssmt_slow_core_seed_keypair(const xmss_params *params, unsigned char *pk, unsigned char *sk,
                                  unsigned char *seed);
int xmssmt_slow_core_sign(const xmss_params *params, unsigned char *sk, unsigned char *sm,
                          unsigned long long *smlen, const unsigned char *m, unsigned long long mlen);

#endif
