# Source

- Upstream: https://github.com/XMSS/xmss-reference (the XMSS reference code
  accompanying RFC 8391, with the NIST SP 800-208 parameter sets)
- Commit: 171ccbd26f098542a67eb5d2b128281c80bd71a6 (2021-03-16)
- License: CC0 1.0 Universal, see `LICENSE`

The files here are unchanged copies. Not copied: `fips202.c`/`fips202.h` and
`randombytes.c`/`randombytes.h` (this project supplies its own, see
`suites/xmss/pqc_suite/`), and the `ui/` and `test/` programs. The upstream
`hash.c` includes `<openssl/sha.h>`; the suite provides a small header of that
name that maps `SHA256()` and `SHA512()` to `components/prims`.
