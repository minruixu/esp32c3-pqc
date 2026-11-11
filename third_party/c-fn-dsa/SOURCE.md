# c-fn-dsa

- Upstream: https://github.com/pornin/c-fn-dsa (Thomas Pornin)
- Commit: 1b858880dcac091b5e5f84f0e4d8617c35ba1392 (2026-09-21)
- License: Unlicense (public domain), see `LICENSE`
- What it is: FN-DSA (Falcon) in portable C, adjusted on 2026-07-22 to the
  author's best estimate of the FIPS 206 draft (see `README.md`).

Copied: the library sources and headers (`*.c`, `*.h` except tests and
benchmarks), `LICENSE`, `README.md`. Not copied: `test_*.c`, `speed_fndsa.c`,
`sysrng.c` (operating-system RNG), the Cortex-M4 assembly files, `bench_cm4/`,
the Makefiles.

Local change, one place: `sha3.c` gained the option `FNDSA_EXTERNAL_KECCAK`.
When it is 1 the built-in Keccak-f[1600] is left out and the application
provides `fndsa_sha3_process_block()`. The suite uses this to send the
permutation through `pqc_prims.h`. With the option at 0 (the default) the
file compiles exactly as upstream.
