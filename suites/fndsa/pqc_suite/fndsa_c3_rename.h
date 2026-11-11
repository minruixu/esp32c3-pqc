/* Prefix fndsa_c3_ for every external symbol defined by c-fn-dsa's signing
 * files (sign.c, sign_core.c, sign_fpoly.c, sign_fpr.c, sign_sampler.c), so
 * that the ESP32-C3 build of the signing code (fndsa_sign_c3.c) can be linked
 * next to the unchanged one. The list is the output of `nm -g --defined-only`
 * on those five object files, plus two internal names. */
#ifndef FNDSA_C3_RENAME_H
#define FNDSA_C3_RENAME_H

#define fndsa_ffsamp_fft fndsa_c3_ffsamp_fft
#define fndsa_fpoly_FFT fndsa_c3_fpoly_FFT
#define fndsa_fpoly_LDL_fft fndsa_c3_fpoly_LDL_fft
#define fndsa_fpoly_add fndsa_c3_fpoly_add
#define fndsa_fpoly_apply_basis fndsa_c3_fpoly_apply_basis
#define fndsa_fpoly_gram_fft fndsa_c3_fpoly_gram_fft
#define fndsa_fpoly_iFFT fndsa_c3_fpoly_iFFT
#define fndsa_fpoly_merge_fft fndsa_c3_fpoly_merge_fft
#define fndsa_fpoly_mul_fft fndsa_c3_fpoly_mul_fft
#define fndsa_fpoly_mulconst fndsa_c3_fpoly_mulconst
#define fndsa_fpoly_neg fndsa_c3_fpoly_neg
#define fndsa_fpoly_set_small fndsa_c3_fpoly_set_small
#define fndsa_fpoly_split_fft fndsa_c3_fpoly_split_fft
#define fndsa_fpoly_split_selfadj_fft fndsa_c3_fpoly_split_selfadj_fft
#define fndsa_fpoly_sub fndsa_c3_fpoly_sub
#define fndsa_fpr_add fndsa_c3_fpr_add
#define fndsa_fpr_div fndsa_c3_fpr_div
#define fndsa_fpr_mul fndsa_c3_fpr_mul
#define fndsa_fpr_scaled fndsa_c3_fpr_scaled
#define fndsa_fpr_sqrt fndsa_c3_fpr_sqrt
#define fndsa_sampler_next fndsa_c3_sampler_next
#define fndsa_sign fndsa_c3_sign
#define fndsa_sign_core fndsa_c3_sign_core
#define fndsa_sign_seeded fndsa_c3_sign_seeded
#define fndsa_sign_seeded_temp fndsa_c3_sign_seeded_temp
#define fndsa_sign_temp fndsa_c3_sign_temp
#define fndsa_sign_weak fndsa_c3_sign_weak
#define fndsa_sign_weak_seeded fndsa_c3_sign_weak_seeded
#define fndsa_sign_weak_seeded_temp fndsa_c3_sign_weak_seeded_temp
#define fndsa_sign_weak_temp fndsa_c3_sign_weak_temp
#define fndsa_ffsamp_fft_inner fndsa_c3_ffsamp_fft_inner
#define fndsa_fpr_add_sub fndsa_c3_fpr_add_sub

#endif
