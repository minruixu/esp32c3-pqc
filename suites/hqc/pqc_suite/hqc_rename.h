/* Gives every external symbol of the HQC reference implementation a prefix,
 * so that several parameter sets and variants can be linked into one image.
 * HQC_NS(name) must be defined before this file is included, for example
 *   #define HQC_NS(x) hqc1r_##x
 * The list is the output of `nm -g` on the compiled upstream sources. */
#ifndef HQC_NS
#error "define HQC_NS(x) before including hqc_rename.h"
#endif

#define code_decode HQC_NS(code_decode)
#define code_encode HQC_NS(code_encode)
#define compute_generator_poly HQC_NS(compute_generator_poly)
#define encode HQC_NS(encode)
#define expand_and_sum HQC_NS(expand_and_sum)
#define find_peaks HQC_NS(find_peaks)
#define gf_carryless_mul HQC_NS(gf_carryless_mul)
#define gf_generate HQC_NS(gf_generate)
#define gf_inverse HQC_NS(gf_inverse)
#define gf_mul HQC_NS(gf_mul)
#define gf_square HQC_NS(gf_square)
#define hadamard HQC_NS(hadamard)
#define hash_g HQC_NS(hash_g)
#define hash_h HQC_NS(hash_h)
#define hash_i HQC_NS(hash_i)
#define hash_j HQC_NS(hash_j)
#define hqc_c_kem_from_string HQC_NS(hqc_c_kem_from_string)
#define hqc_c_kem_to_string HQC_NS(hqc_c_kem_to_string)
#define hqc_dk_pke_from_string HQC_NS(hqc_dk_pke_from_string)
#define hqc_ek_pke_from_string HQC_NS(hqc_ek_pke_from_string)
#define hqc_kem_decaps HQC_NS(hqc_kem_decaps)
#define hqc_kem_encaps HQC_NS(hqc_kem_encaps)
#define hqc_kem_keygen HQC_NS(hqc_kem_keygen)
#define hqc_pke_decrypt HQC_NS(hqc_pke_decrypt)
#define hqc_pke_encrypt HQC_NS(hqc_pke_encrypt)
#define hqc_pke_keygen HQC_NS(hqc_pke_keygen)
#define memset_volatile HQC_NS(memset_volatile)
#define reed_muller_decode HQC_NS(reed_muller_decode)
#define reed_muller_encode HQC_NS(reed_muller_encode)
#define reed_solomon_decode HQC_NS(reed_solomon_decode)
#define reed_solomon_encode HQC_NS(reed_solomon_encode)
#define vect_add HQC_NS(vect_add)
#define vect_compare HQC_NS(vect_compare)
#define vect_generate_random_support1 HQC_NS(vect_generate_random_support1)
#define vect_generate_random_support2 HQC_NS(vect_generate_random_support2)
#define vect_mul HQC_NS(vect_mul)
#define vect_print HQC_NS(vect_print)
#define vect_sample_fixed_weight1 HQC_NS(vect_sample_fixed_weight1)
#define vect_sample_fixed_weight2 HQC_NS(vect_sample_fixed_weight2)
#define vect_set_random HQC_NS(vect_set_random)
#define vect_truncate HQC_NS(vect_truncate)
#define vect_write_support_to_vector HQC_NS(vect_write_support_to_vector)
#define xof_get_bytes HQC_NS(xof_get_bytes)
#define xof_init HQC_NS(xof_init)
#define xof_init_dom_sep HQC_NS(xof_init_dom_sep)

/* Random bytes come from hqc_glue.c: the harness RNG, or a scripted
 * sequence while a known-answer test runs. */
#define randombytes hqc_randombytes
