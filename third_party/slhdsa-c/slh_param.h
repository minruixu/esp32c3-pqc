/*
 * Copyright (c) The slhdsa-c project authors
 * SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT
 */

/* === Internal parameter definition structure. */

#ifndef _SLH_PARAM_H_
#define _SLH_PARAM_H_

#include "plat_local.h"

/* address */
typedef union
{
  uint8_t u8[32];
  uint32_t u32[8];
} adrs_t;

/* Parameter sets */

#ifndef _SLH_DSA_H_
typedef struct slh_param_s slh_param_t;
#endif
typedef struct slh_var_s slh_var_t;

/* used to indicate _internal mode */
#define SLH_CTX_SZ_NO_CONTEXT 0x100

struct slh_param_s
{
  const char *alg_id; /* Algorithm name */

  /* core parameters */
  uint32_t n;    /* Security level / hash size { 16,24,32 }. */
  uint32_t h;    /* Bits h used to select FORS key. */
  uint32_t d;    /* Number of hypertree layers d. */
  uint32_t hp;   /* Merkle tree height h' (XMSS). */
  uint32_t a;    /* String length t = 2**a (FORS). */
  uint32_t k;    /* Number of indices (FORS). */
  uint32_t lg_w; /* Number of bits in chain index (WOTS+) */
  uint32_t m;    /* Length in bytes of message digest. */

  /* hash function instantation */
  void (*mk_var)(slh_var_t *var, const uint8_t *pk, const uint8_t *sk,
                 const slh_param_t *prm);
  void (*chain)(slh_var_t *var, uint8_t *tmp, const uint8_t *x, uint32_t i,
                uint32_t s);
  void (*wots_chain)(slh_var_t *var, uint8_t *tmp, uint32_t s);
  void (*fors_hash)(slh_var_t *var, uint8_t *tmp, uint32_t s);
  void (*h_msg)(slh_var_t *var, uint8_t *h, const uint8_t *r, const uint8_t *m,
                size_t m_sz, const uint8_t *ctx, size_t ctx_sz);
  void (*prf)(slh_var_t *var, uint8_t *h);
  void (*prf_msg)(slh_var_t *var, uint8_t *h, const uint8_t *opt_rand,
                  const uint8_t *m, size_t m_sz, const uint8_t *ctx,
                  size_t ctx_sz);
  void (*h_f)(slh_var_t *var, uint8_t *h, const uint8_t *m1);
  void (*h_h)(slh_var_t *var, uint8_t *h, const uint8_t *m1, const uint8_t *m2);
  void (*h_t)(slh_var_t *var, uint8_t *h, const uint8_t *m, size_t m_sz);

#ifdef SLH_PQC_PRIMS
  /* esp32c3-pqc: optional replacements for three inner loops, NULL to use the
   * generic code. They let an instantiation work on several independent
   * hashes at once.
   *
   * wots_pkgen: Algorithm 6, the WOTS+ public key of the key pair selected
   * in ADRS (all chains and their compression), written to pk. Leaves ADRS
   * as the generic code does.
   *
   * fors_leaf2: the two FORS leaves with tree index ADRS.tree_index and
   * ADRS.tree_index + 1 (secret values hashed once), written to out[0..2n).
   * Leaves ADRS as fors_hash(.., 1) does. */
  void (*wots_pkgen)(slh_var_t *var, uint8_t *pk);
  void (*fors_leaf2)(slh_var_t *var, uint8_t *out);
  /* wots_chains: the loop of Algorithm 8. For every chain i, take the n
   * bytes at in + i * n from hash address start[i] to the end of the chain
   * and write the result to out + i * n. */
  void (*wots_chains)(slh_var_t *var, uint8_t *out, const uint8_t *in,
                      const uint32_t *start);
#endif
};

/* _SLH_PARAM_H_ */
#endif
