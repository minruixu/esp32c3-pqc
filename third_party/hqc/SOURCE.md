# HQC reference implementation

- Upstream: https://gitlab.com/pqc-hqc/hqc (the HQC team)
- Commit: 06567ccbad10580d78dd7863b15d23f41076d57f (2026-10-26), release
  v5.1.0
- License: public domain, see `LICENSE`
- What it is: the reference implementation of HQC-1, HQC-3 and HQC-5 (the
  former HQC-128/192/256) that the designers maintain while NIST prepares
  the standard. See `CHANGELOG` for how v5.1.0 differs from v5.0.0.

Copied without change: `src/common/` and `src/ref/` (as `src/common`,
`src/ref`), `LICENSE`, `README.md`, `CHANGELOG`.

Not copied: `src/x86_64/` (AVX2 code), `lib/fips202/` (the project's
`components/prims` provides the same interface), `lib/nist-rng/`, `tests/`,
`tools/`, `packaging/`, and `kats/` (4 MB of known-answer files; the suite
keeps fingerprints of them, see `suites/hqc/gen_kat.py`).

No file here was modified. Two problems of the reference code on a 32-bit
RISC-V target are worked around in `suites/hqc/pqc_suite/` instead (an empty
`immintrin.h`, and a corrected `BITMASK` macro); `suites/hqc/NOTES.md`
describes them.
