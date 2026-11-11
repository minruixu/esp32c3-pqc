/* LAC gives its functions and tables fixed global names. To hold all
 * security levels in one image, every one of them gets the prefix PQM_NS
 * of the compilation unit that includes this header. */
#ifndef LAC_RENAME_H
#define LAC_RENAME_H

#define LAC_CAT2(a, b) a##b
#define LAC_CAT(a, b) LAC_CAT2(a, b)
#define LAC_NAME(s) LAC_CAT(PQM_NS, _##s)

#define a_log_tab LAC_NAME(a_log_tab)
#define a_pow_tab LAC_NAME(a_pow_tab)
#define bch LAC_NAME(bch)
#define crypto_encrypt LAC_NAME(crypto_encrypt)
#define crypto_encrypt_keypair LAC_NAME(crypto_encrypt_keypair)
#define crypto_encrypt_open LAC_NAME(crypto_encrypt_open)
#define crypto_kem_dec LAC_NAME(crypto_kem_dec)
#define crypto_kem_enc LAC_NAME(crypto_kem_enc)
#define crypto_kem_keypair LAC_NAME(crypto_kem_keypair)
#define decode_bch LAC_NAME(decode_bch)
#define ecc_dec LAC_NAME(ecc_dec)
#define ecc_enc LAC_NAME(ecc_enc)
#define encode_bch LAC_NAME(encode_bch)
#define g LAC_NAME(g)
#define gen_a LAC_NAME(gen_a)
#define gen_e LAC_NAME(gen_e)
#define gen_r LAC_NAME(gen_r)
#define kem_dec_fo LAC_NAME(kem_dec_fo)
#define kem_enc_fo LAC_NAME(kem_enc_fo)
#define kem_enc_fo_seed LAC_NAME(kem_enc_fo_seed)
#define kg LAC_NAME(kg)
#define kg_seed LAC_NAME(kg_seed)
#define mod8_tab_half LAC_NAME(mod8_tab_half)
#define mul_core LAC_NAME(mul_core)
#define pke_dec LAC_NAME(pke_dec)
#define pke_enc LAC_NAME(pke_enc)
#define pke_enc_seed LAC_NAME(pke_enc_seed)
#define poly_aff LAC_NAME(poly_aff)
#define poly_compress LAC_NAME(poly_compress)
#define poly_decompress LAC_NAME(poly_decompress)
#define poly_mul LAC_NAME(poly_mul)
#define random_bytes LAC_NAME(random_bytes)
#define pseudo_random_bytes LAC_NAME(pseudo_random_bytes)
#define hash LAC_NAME(hash)
#define hash_to_k LAC_NAME(hash_to_k)
#define gen_seed LAC_NAME(gen_seed)

#endif
