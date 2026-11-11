/* Stand-in for <openssl/sha.h>: the XMSS reference code only needs the
 * one-shot SHA256() and SHA512() calls, which components/prims provides. */
#ifndef XMSS_SHIM_OPENSSL_SHA_H
#define XMSS_SHIM_OPENSSL_SHA_H

#include "sha2.h"

#define SHA256(in, inlen, out) sha256((out), (in), (inlen))
#define SHA512(in, inlen, out) sha512((out), (in), (inlen))

#endif
