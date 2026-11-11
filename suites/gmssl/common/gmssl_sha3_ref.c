/* GmSSL's own SHA-3 and SHAKE (src/sha3.c, unchanged) under gmssl_ref_*
 * names; see gmssl_port.c. */
#define sha3_256 gmssl_ref_sha3_256
#define sha3_512 gmssl_ref_sha3_512
#define shake128_init gmssl_ref_shake128_init
#define shake256_init gmssl_ref_shake256_init
#define shake_update gmssl_ref_shake_update
#define shake_finish gmssl_ref_shake_finish
#define shake_squeeze gmssl_ref_shake_squeeze
#define shake128 gmssl_ref_shake128
#define shake256 gmssl_ref_shake256
#include "src/sha3.c"
