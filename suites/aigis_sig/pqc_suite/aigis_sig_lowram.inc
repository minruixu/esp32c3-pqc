/* Aigis-sig with less stack: the public matrix is never held as a whole.
 *
 * PQMagic's key generation, signing and verification expand the K x L matrix
 * into K*L polynomials (12 to 30 KB) and keep it for the whole operation.
 * Here one row is expanded at a time, right before it is used. Key
 * generation and verification use every row once, so they lose nothing;
 * signing re-expands the rows in every round of its rejection loop and gets
 * slower. Keys and signatures are the same bytes as upstream's.
 *
 * The functions below are PQMagic's (sig/aigis-sig/std/sign.c, Apache-2.0),
 * generated from that file by the changes listed in suites/aigis_sig/NOTES.md:
 * expand_mat() restricted to one row, and the three callers using it. */

static void lr_expand_row(polyvecl *row, const unsigned char rho[SEEDBYTES], unsigned int i) {
  unsigned int j, pos, ctr;
  unsigned char inbuf[SEEDBYTES + 2];
#if QBITS == 21
  unsigned char outbuf[6*SHAKE128_RATE]; /* Probability that we need more than 6 blocks: < 2^{-137}.*/
#elif QBITS == 22
  unsigned char outbuf[7*SHAKE128_RATE]; /* Probability that we need more than 7 blocks: < 2^{-174}.*/
#endif
  uint32_t val;

  memcpy(inbuf, rho, SEEDBYTES); /* upstream copies with a loop over i */

  {
	  for(j = 0; j < PARAM_L; ++j) {
		  ctr = pos = 0;
		  inbuf[SEEDBYTES] = i + (j << 4);

#ifdef USE_SHAKE
      keccak_state state;
      shake128_absorb_once(&state, inbuf, SEEDBYTES + 1);
      shake128_squeezeblocks(outbuf, 6, &state);
#else
      inbuf[SEEDBYTES + 1] = 0;
      sm3_extended(outbuf, 6*SHAKE128_RATE, inbuf, SEEDBYTES+2);
#endif

#if QBITS == 21 
		  while(ctr < PARAM_N) {
			  val  = outbuf[pos++];
			  val |= (uint32_t)outbuf[pos++] << 8;
			  val |= (uint32_t)outbuf[pos++] << 16;

			  val &= 0x1FFFFF;
			  /* Rejection sampling */
			  if(val < PARAM_Q)
				  row->vec[j].coeffs[ctr++] = val;
		  }

#elif QBITS == 22 
		  /* Probability that we need more than 6 blocks: < 2^{-65}.
		  * Probability that we need more than 7 blocks: < 2^{-174}. */

		  do {
			  val  = outbuf[pos++];
			  val |= (uint32_t)outbuf[pos++] << 8;
			  val |= (uint32_t)outbuf[pos++] << 16;
			  val &= 0x3FFFFF;

			  if(val < PARAM_Q)
				  row->vec[j].coeffs[ctr++] = val;
		  }while(ctr < 225);
		  /* Probability we need more than 6 blocks to generate 225 elements: < 2^{-135}.*/
		  /* Probability we need more than 258 bytes to generate the last 31 elements: < 2^{-133}.*/
		  /// TODO: ??? Goes in this if condition has a huge probability. 
      if(6*SHAKE128_RATE- pos < 258) {
  #ifdef USE_SHAKE
        shake128_squeezeblocks(outbuf + 6 * SHAKE128_RATE, 1, &state);
  #else
        inbuf[SEEDBYTES + 1] = 1;
        sm3_extended(outbuf + 6*SHAKE128_RATE, SHAKE128_RATE, inbuf, SEEDBYTES+2);
  #endif
      }

		  do {
			  val  = outbuf[pos++];
			  val |= (uint32_t)outbuf[pos++] << 8;
			  val |= (uint32_t)outbuf[pos++] << 16;

			  val &= 0x3FFFFF;

			  if(val < PARAM_Q)
				  row->vec[j].coeffs[ctr++] = val;
		  }while(ctr < PARAM_N);
#endif
	  }
  }
}

static int lr_keypair_internal(
  unsigned char *pk, 
  unsigned char *sk,
  const unsigned char *coins)
{
  unsigned int i;
  unsigned char buf[3*SEEDBYTES + CRHBYTES]; //buf = r|rho|key|hash(pk)
  uint16_t nonce = 0;
  polyvecl row; /* one row of the matrix at a time */
  polyvecl s1, s1hat;
  polyveck s2, t, t1, t0;
  
  // randombytes(buf, SEEDBYTES);
  // sm3_extended(buf, 3*SEEDBYTES, buf, SEEDBYTES);
#ifdef USE_SHAKE
  shake256(buf, 3*SEEDBYTES, coins, SEEDBYTES);
#else
  sm3_extended(buf, 3*SEEDBYTES, coins, SEEDBYTES); 
#endif
  


  for(i = 0; i < PARAM_L; ++i)
    poly_uniform_eta1(&s1.vec[i], buf, nonce++);
  for(i = 0; i < PARAM_K; ++i)
    poly_uniform_eta2(&s2.vec[i], buf, nonce++);

  s1hat = s1;
  polyvecl_ntt(&s1hat);
  for(i = 0; i < PARAM_K; ++i) {
    lr_expand_row(&row, &buf[SEEDBYTES], i);
    polyvecl_pointwise_acc_invmontgomery(&t.vec[i], &row, &s1hat);//output coefficient  <=2*Q
    poly_invntt_montgomery(t.vec+i);//output coefficient  <=2*Q
  }

  polyveck_add(&t, &t, &s2);//output coefficient <=4*Q
  polyveck_freeze4q(&t);
  polyveck_power2round(&t1, &t0, &t);
  pack_pk(pk, &buf[SEEDBYTES], &t1);

#ifdef USE_SHAKE
  shake256(buf + 3*SEEDBYTES, CRHBYTES, pk, AIGIS_SIG_PUBLICKEYBYTES);
#else
  sm3_extended(&buf[3*SEEDBYTES], CRHBYTES, pk, AIGIS_SIG_PUBLICKEYBYTES);
#endif
  pack_sk(sk,&buf[SEEDBYTES], &s1, &s2, &t0);

  return 0;

}

static int lr_signature_internal(
  unsigned char *sig, 
  size_t *siglen, 
  const unsigned char *m, 
  size_t mlen, 
  const unsigned char *sk)
{
  size_t i, j;
  unsigned int n;
  unsigned char *buf; 
  uint16_t nonce = 0;
  poly     c, chat;
  polyvecl row, s1, y, yhat, z; /* one row of the matrix at a time */
  polyveck s2, t0, w, w1;
  polyveck h, wcs2, wcs20, ct0, tmp;
  
  buf = (unsigned char *)malloc(2*SEEDBYTES + CRHBYTES + mlen);
  if(buf==NULL)
  	return -1;
  unpack_sk(buf,&s1, &s2, &t0, sk);

  for(i=0;i<mlen;i++)
  	buf[2*SEEDBYTES + CRHBYTES + i] = m[i];

#ifdef USE_SHAKE
  shake256(buf + 2*SEEDBYTES, CRHBYTES, buf + 2*SEEDBYTES, CRHBYTES + mlen);
#else
  sm3_extended(buf + 2*SEEDBYTES, CRHBYTES, buf + 2*SEEDBYTES, CRHBYTES + mlen);
#endif

  polyvecl_ntt(&s1);
  polyveck_ntt(&s2);
  polyveck_ntt(&t0);

  rej:
  for(i = 0; i < PARAM_L; ++i)
    poly_uniform_gamma1m1(y.vec+i, &buf[SEEDBYTES], nonce++);

  yhat = y;
  polyvecl_ntt(&yhat);
  for(i = 0; i < PARAM_K; ++i) {
    lr_expand_row(&row, buf, i);
    polyvecl_pointwise_acc_invmontgomery(w.vec+i, &row, &yhat);//output coefficient  <=2*Q
    poly_invntt_montgomery(w.vec+i);//output coefficient  <= 2*Q
  }

  polyveck_freeze2q(&w);
  polyveck_decompose(&w1, &tmp, &w);
  challenge(&c, &buf[2*SEEDBYTES], &w1);

  chat = c;
  poly_ntt(&chat);
  for(i = 0; i < PARAM_L; ++i) {
    poly_pointwise_invmontgomery(z.vec+i, &chat, s1.vec+i);//output coefficient  <=2*Q
    poly_invntt_montgomery(z.vec+i);//output coefficient  <=2*Q
  }
  polyvecl_add(&z, &z, &y);//output coefficient  <=4*Q
  polyvecl_freeze4q(&z);
  if(polyvecl_chknorm(&z, GAMMA1 - BETA1))
    goto rej;

  for(i = 0; i < PARAM_K; ++i) {
    poly_pointwise_invmontgomery(wcs2.vec+i, &chat, s2.vec+i);//output coefficient  <=2*Q
    poly_invntt_montgomery(wcs2.vec+i);//output coefficient  <=2*Q
  }
  polyveck_sub(&wcs2, &w, &wcs2);//output coefficient  <=4*Q
  polyveck_freeze4q(&wcs2);
  polyveck_decompose(&tmp, &wcs20, &wcs2);//output coefficient  <=2*Q
  polyveck_freeze2q(&wcs20);
  if(polyveck_chknorm(&wcs20, GAMMA2 - BETA2))
    goto rej;

  for(i = 0; i < PARAM_K; ++i)
    for(j = 0; j < PARAM_N; ++j)
      if(tmp.vec[i].coeffs[j] != w1.vec[i].coeffs[j])
        goto rej;

  for(i = 0; i < PARAM_K; ++i) {
    poly_pointwise_invmontgomery(ct0.vec+i, &chat, t0.vec+i);//output coefficient  <=2*Q
    poly_invntt_montgomery(ct0.vec+i);//output coefficient  <=2*Q
  }
  polyveck_freeze2q(&ct0);
  if(polyveck_chknorm(&ct0, GAMMA2))
    goto rej;

  polyveck_add(&tmp, &wcs2, &ct0);//output coefficient  <=2*Q
  polyveck_neg(&ct0);
  polyveck_freeze2q(&tmp);
  n = polyveck_make_hint(&h, &tmp, &ct0);
  if(n > OMEGA)
    goto rej;

  pack_sig(sig, &z, &h, &c);

  *siglen = CRYPTO_BYTES;
  
  free(buf);
  return 0;
}

static int lr_signature(unsigned char *sig,
                          size_t *siglen,
                          const unsigned char *m,
                          size_t mlen,
                          const unsigned char *ctx,
                          size_t ctx_len,
                          const unsigned char *sk)
{

  if(ctx_len > 255) {
    return -1;
  }

//   unsigned char coins[RNDBYTES];
  unsigned char *m_extended = (unsigned char*)malloc(mlen + ctx_len + 2);
  
// #ifdef AIGIS_SIG_RANDOMIZED_SIGNING
//   randombytes(coins, RNDBYTES);
// #else
//   memset(coins, 0, RNDBYTES);
// #endif

  m_extended[0] = 0;
  m_extended[1] = (unsigned char)ctx_len;
  if (ctx) {
  	memcpy(m_extended + 2, ctx, ctx_len);
  }
  memcpy(m_extended + 2 + ctx_len, m, mlen);

  int ret = lr_signature_internal(sig, siglen, m_extended, mlen + ctx_len + 2, sk);
  free(m_extended);
  return ret;
}

static int lr_verify_internal(
  const unsigned char *sig, size_t siglen,
  const unsigned char *m, size_t mlen,
  const unsigned char *pk)
{
  size_t i;
  unsigned char rho[SEEDBYTES];
  unsigned char *buf; 
  poly     c, chat, cp;
  polyvecl row, z; /* one row of the matrix at a time */
  polyveck t1, w1, h, tmp1, tmp2;

  if(siglen != CRYPTO_BYTES)
    return -1;

  unpack_pk(rho, &t1, pk);
  if(unpack_sig(&z, &h, &c, sig)) {
    return -1;
  }
  if(polyvecl_chknorm(&z, GAMMA1 - BETA1))
    return -1;
  
  buf = (unsigned char*)malloc(CRHBYTES + mlen);
  if(buf==NULL)
  	return -1;
  for(i=0;i<mlen;i++)
  	buf[CRHBYTES + i] = m[i];

#ifdef USE_SHAKE
  shake256(buf, CRHBYTES, pk, AIGIS_SIG_PUBLICKEYBYTES);
	shake256(buf, CRHBYTES, buf, CRHBYTES + mlen);
#else
  sm3_extended(buf, CRHBYTES, pk, AIGIS_SIG_PUBLICKEYBYTES);
	sm3_extended(buf, CRHBYTES, buf, CRHBYTES + mlen);
#endif

  
  polyvecl_ntt(&z);
  for(i = 0; i < PARAM_K ; ++i) {
      lr_expand_row(&row, rho, i);
      polyvecl_pointwise_acc_invmontgomery(tmp1.vec+i, &row, &z);
  }//output coefficient  <=2*Q

  chat = c;
  poly_ntt(&chat);
  polyveck_shiftl(&t1, PARAM_D);
  polyveck_ntt(&t1);
  for(i = 0; i < PARAM_K; ++i)
    poly_pointwise_invmontgomery(tmp2.vec+i, &chat, t1.vec+i);//output coefficient  <=2*Q

  polyveck_sub(&tmp1, &tmp1, &tmp2);//output coefficient  <=4*Q
  polyveck_invntt_montgomery(&tmp1);//output coefficient  <= 2*Q

  polyveck_freeze2q(&tmp1);
  polyveck_use_hint(&w1, &tmp1, &h);

  challenge(&cp, buf, &w1);
  for(i = 0; i < PARAM_N; ++i) {
    if(c.coeffs[i] != cp.coeffs[i]) {
      free(buf);
      return -1;
    }
  }

  free(buf);
  return 0;
}

static int lr_verify(const unsigned char *sig,
                       size_t siglen,
                       const unsigned char *m,
                       size_t mlen,
                       const unsigned char *ctx,
                       size_t ctx_len,
                       const unsigned char *pk)
{

  if(ctx_len > 255) {
    return -1;
  }

  unsigned char *m_extended = (unsigned char*)malloc(mlen + ctx_len + 2);

  m_extended[0] = 0;
  m_extended[1] = (unsigned char)ctx_len;
  if (ctx) {
  	memcpy(m_extended + 2, ctx, ctx_len);
  }
  memcpy(m_extended + 2 + ctx_len, m, mlen);

  int ret = lr_verify_internal(sig, siglen, m_extended, mlen + ctx_len + 2, pk);
  free(m_extended);
  return ret;
}

static int lr_keypair(unsigned char *pk, unsigned char *sk)
{
    unsigned char coins[SEEDBYTES];
    randombytes(coins, SEEDBYTES);
    return lr_keypair_internal(pk, sk, coins);
}

static int lr_sign(unsigned char *sig, size_t *siglen, const unsigned char *m, size_t mlen,
                   const unsigned char *sk)
{
    return lr_signature(sig, siglen, m, mlen, pqm_ctx, 0, sk);
}

static int lr_verify_noctx(const unsigned char *sig, size_t siglen, const unsigned char *m, size_t mlen,
                           const unsigned char *pk)
{
    return lr_verify(sig, siglen, m, mlen, pqm_ctx, 0, pk);
}

static const sig_ops pqm_lowram_ops = {lr_keypair, lr_sign, lr_verify_noctx};
