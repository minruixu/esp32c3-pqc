/* GmSSL's own SM3 (src/sm3.c, unchanged) under gmssl_ref_* names. The names
 * the rest of GmSSL calls are provided by gmssl_port.c, which chooses
 * between this code and the project's SM3 backend at run time. */
#define sm3_compress_blocks gmssl_ref_sm3_compress_blocks
#define sm3_init gmssl_ref_sm3_init
#define sm3_update gmssl_ref_sm3_update
#define sm3_finish gmssl_ref_sm3_finish
#include "src/sm3.c"
