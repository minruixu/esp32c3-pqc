# Source

PQMagic, https://github.com/pqcrypto-cn/PQMagic, commit
`42f5b6e6fe7e4951ffc9437ecd77c9a5af93ff08` (2026-10-29), Apache License 2.0
(`LICENSE`). `tools/fetch/pqmagic.sh` fetches the same commit into
`downloads/` for comparison.

## What is here

A subset of the repository, with its directory layout kept:

| Directory | Content |
|---|---|
| `hash/`, `include/`, `utils/` | PQMagic's SM3, Keccak and common headers |
| `kem/aigis-enc`, `sig/aigis-sig` | Aigis-enc and Aigis-sig |
| `kem/ml_kem`, `sig/ml_dsa`, `sig/slh_dsa` | ML-KEM, ML-DSA and SLH-DSA with their SM3 and SHAKE modes |
| `sig/sphincs-a` | SPHINCS-alpha |
| `kem/aigis-enc-plus`, `sig/aigis-sig-plus`, `kem/nev`, `kex/nev-ake` | reference code of four NGCC round-1 candidates |

Not copied: `kem/kyber`, `sig/dilithium` (round-3 versions of the standards),
`kem/qimen-pike` (needs GMP and an LGPL library), `sig/resolved-alpha` (does
not fit the target, see `suites/pqmagic_common/NOTES.md`), the AVX2 Keccak in
`kem/aigis-enc-plus/std/keccak4x`, the x86 SM3 assembly in
`sig/aigis-sig-plus/std/ippcpsm3`, figures and the top-level build files.

## Local changes

Every change is marked with a comment that starts with `esp32c3-pqc:` and is
inside an `#ifdef`, so the files still build unchanged without the macro,
except for the last one, which is a bug fix.

1. `hash/keccak/fips202.c`, `kem/aigis-enc-plus/std/fips202.c`,
   `kem/nev/std/symmetrics/fips202.c`, `kex/nev-ake/std/symmetrics/fips202.c`,
   `sig/aigis-sig-plus/std/fips202.c`: with `PQC_PRIMS_KECCAK`,
   `KeccakF1600_StatePermute()` calls this project's `pqc_keccakf1600()`.
2. `auxfunc.c` in `kem/nev/std/symmetrics`, `kex/nev-ake/std/symmetrics`,
   `sig/aigis-sig-plus/std` and `kem/aigis-enc-plus/std/kat_test`: with
   `PQC_PRIMS_SM3`, `sm3_bit_compress()` calls `pqc_sm3_compress()`.
3. `sig/sphincs-a/std/sha2.c`: with `PQC_PRIMS_SHA256`, the SHA-256 block
   function calls `pqc_sha256_compress()`.
4. `sig/sphincs-a/std/wots.c`: with `PQC_COMPACT_DP`, the constant-sum
   encoding reads its table through `sphincsa_dp.h` (compact storage) instead
   of a static array of 278 KB to 2.2 MB. Same values, same signatures.
5. `hash/sm3/sm3_extended.c`: the architecture check that selects the
   portable SM3 header also accepts RISC-V.
6. `sig/aigis-sig-plus/std/polyvec.c`, `polyvecl_uniform_gamma1()`: one call
   of `polyz_unpack()` after the loop wrote a whole polynomial (2 KB) past
   the end of its output vector, a stack overflow inside `msig_sign()`. The
   call is disabled; signatures are unchanged.

PQMagic's own `hash/sm3/x86-64/sm3.c` is not built: `suites/pqmagic_common/
port/pqm_sm3.c` provides the same functions on top of `pqc_sm3_compress()`.
`suites/pqmagic_common/check_upstream_hash.sh` builds a suite both ways on the
host and compares every output.
