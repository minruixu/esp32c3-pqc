/* GmSSL's one-shot SHA-3 functions have the same names as those of
 * components/prims (fips202.h) but other argument orders. Inside the GmSSL
 * code they are renamed, so both can live in one program. */
#ifndef GMSSL_RENAME_H
#define GMSSL_RENAME_H

#define sha3_256 gmssl_sha3_256
#define sha3_512 gmssl_sha3_512
#define shake128 gmssl_shake128
#define shake256 gmssl_shake256

#endif
