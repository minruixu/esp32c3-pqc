/* Stand-in for the reference code's randombytes.h. Key generation asks for
 * the secret seeds and the public seed through this function; the suite can
 * hand it fixed bytes (known-answer tests) or the harness generator. */
#ifndef XMSS_SHIM_RANDOMBYTES_H
#define XMSS_SHIM_RANDOMBYTES_H

#include <stddef.h>
#include <stdint.h>

int xmss_port_randombytes(uint8_t *out, size_t outlen);
#define randombytes xmss_port_randombytes

#endif
