# Source

- Upstream: https://github.com/guanzhi/GmSSL
- Commit: 24ae482701a7b124826c382fffc55c19f76d475d (2026-06-30)
- License: Apache-2.0, see `LICENSE`

Only the files needed for GmSSL's post-quantum algorithms are here, all of
them unchanged:

- `src/lms.c`, `src/xmss.c`, `src/sphincs.c`, `src/kyber.c`: LMS/HSS,
  XMSS/XMSS^MT and SPHINCS+ on SM3, and Kyber-768.
- `src/sm3.c`, `src/sm3_hmac.c`, `src/sha3.c`, `src/hex.c`: what those four
  call.
- `include/gmssl/*.h`: the headers the files above include.
- `tests/lmstest.c`, `tests/xmsstest.c`, `tests/sphincstest.c`,
  `tests/kybertest.c`, `tests/kybertest_mlkem768.h`: GmSSL's own tests, run as
  self-tests by `suites/gmssl`.

The glue that replaces the rest of GmSSL (random bytes, print helpers, the
choice between GmSSL's hash code and this project's) is in
`suites/gmssl/common/`.
