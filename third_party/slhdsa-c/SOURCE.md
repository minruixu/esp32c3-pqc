# Source

- Project: slhdsa-c, https://github.com/pq-code-package/slhdsa-c
- Commit: 47451432bb04faf8217ffe8a961a75b542bf88e8 (2026-11-02)
- License: Apache-2.0 OR ISC OR MIT (see `LICENSE`)
- Copied: the library sources and headers from the repository root, `LICENSE`
  and `README.md`. The tests, proofs and build files were not copied.

## Local changes

All changes are inactive unless `SLH_PQC_PRIMS` is defined. Three functions can
be renamed so that the project can put its own, switchable version in front
of them, and three inner loops can be replaced by an instantiation:

- `sha2_256.c`: `sha2_256_compress()` becomes `sha2_256_compress_c()`.
- `sha2_512.c`: `sha2_512_compress()` becomes `sha2_512_compress_c()`.
- `sha3_f1600.c`: `keccak_f1600()` becomes `keccak_f1600_c()`.

- `slh_param.h`: three optional members at the end of `slh_param_s`,
  `wots_pkgen`, `fors_leaf2` and `wots_chains`. The parameter sets of
  slhdsa-c do not set them, so they are NULL there.
- `slh_dsa.c`: `xmss_node()` calls `wots_pkgen` instead of its loop over the
  WOTS+ chains when the member is set, `fors_node()` takes its leaves two at
  a time from `fors_leaf2` when that member is set, and `wots_pk_from_sig()`
  calls `wots_chains` instead of its loop over the chains when that member is
  set.

Nothing else was changed.
