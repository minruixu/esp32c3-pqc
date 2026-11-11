/* The reference hash layer, compiled under xmss_ref_* names so that
 * hash_opt.c can choose between it and the optimized one at run time. */
#define addr_to_bytes xmss_ref_addr_to_bytes
#define prf xmss_ref_prf
#define prf_keygen xmss_ref_prf_keygen
#define h_msg xmss_ref_h_msg
#define hash_message xmss_ref_hash_message
#define thash_h xmss_ref_thash_h
#define thash_f xmss_ref_thash_f
#include "hash.c"
