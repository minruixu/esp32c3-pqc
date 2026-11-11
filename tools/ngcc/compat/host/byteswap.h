/* Compatibility header for host builds on systems without glibc's <byteswap.h>. */
#ifndef NGCC_COMPAT_BYTESWAP_H
#define NGCC_COMPAT_BYTESWAP_H
#define bswap_16(x) __builtin_bswap16(x)
#define bswap_32(x) __builtin_bswap32(x)
#define bswap_64(x) __builtin_bswap64(x)
#endif
