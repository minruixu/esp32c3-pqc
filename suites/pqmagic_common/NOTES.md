# Shared code of the suites for Chinese post-quantum algorithms

This directory is not a suite. It holds what the suites built from PQMagic,
SCloud+ and LAC have in common.

## Files

| File | Purpose |
|---|---|
| `pqmagic.cmake` | paths and hash-layer sources for suites that build `third_party/pqmagic` |
| `component.cmake` | registers the ESP-IDF component of a suite from its `suite.cmake` |
| `port/pqm_sm3.c` | PQMagic's SM3 interface on `pqc_sm3_compress()`; lean one-shot and counter-mode paths for the optimized variant |
| `port/pqm_sm3_extended.c` | PQMagic's `sm3_extended()` under another name, for the reference variant |
| `port/pqm_fips202.c` | PQMagic's `fips202.c` with Keccak-f[1600] from `pqc_prims.h` |
| `kemsig.h`, `kemsig.c` | table-driven runner for KEMs and signature schemes: timing, tamper test, fingerprints |
| `hwaes.h`, `hwaes.c` | AES counter blocks on the ESP32-C3 AES accelerator |
| `check_upstream_hash.sh` | host: build a suite with the project's primitives and with the upstream hash code, compare all outputs |
| `sanitize_host.sh` | host: run a suite under AddressSanitizer and UndefinedBehaviorSanitizer |
| `results_table.py` | print saved device results as reference/optimized tables |

## How the upstream code is built

Each parameter set and hash mode is one compilation unit: a small `.c` file
sets the parameters and includes a `*_unity.inc` that includes the upstream
sources. This is what lets one image hold, for example, Aigis-enc in four
parameter sets and two hash modes: PQMagic's symbol prefix does not
distinguish the hash mode, and SCloud+ and LAC have no prefix at all, so the
unit redefines the prefix macro or includes a rename header. No upstream file
is edited for this.

A side effect: the compiler sees a whole scheme at once and may inline across
what upstream compiles as separate files. That exposed one upstream bug (the
Aigis-Sig+ stack overflow below).

## What "ref" and "opt" mean here

- `ref`: the upstream algorithm code, with the reference Keccak, SHA-256 and
  SM3 of `components/prims`.
- `opt`: the same code with the optimized primitives, plus what the suite
  adds: lean SM3 paths (all PQMagic SM3 variants), paired-coefficient matrix
  products and hardware AES (SCloud+), hardware AES (LAC). The low-stack
  Aigis-sig is the `opt` of its own suite, `aigis_sig_lowram`.

Both variants draw the same deterministic random bytes, so their keys,
ciphertexts and signatures must be identical; `tools/pqc check` compares the
device with the host for both.

## Linking the optimized primitives

The optimized Keccak, SHA-256 and SM3 of `components/prims/opt` are strong
definitions in the same static library as their weak defaults in `prims.c`.
A linker does not go back into a library to replace a weak definition it
has already loaded, so an image only gets the optimized code if something
names a symbol of those object files. `component.cmake` therefore passes
`-u pqc_keccakf1600_rv32imc_present -u pqc_sha256_hw_init
-u pqc_sm3_compress_rv32`, the same symbols the suites of the primitives'
authors use. Device runs made before that line was added (noted where it
matters) measured only this family's own optimizations.

## Lean SM3 paths (optimized variant)

PQMagic turns SM3 into an extendable-output function by hashing
`counter || input` once per 32 output bytes, each time through
init/update/final with two copies of the data. For the seeds that dominate
(at most 51 bytes) the padded block never changes except for the counter, so
the optimized `sm3_extended()` lays the block out once and calls the
compression function directly; the optimized one-shot `sm3()` compresses full
blocks straight from the input. The reference variant keeps PQMagic's flow.
Measured effect: see `suites/aigis_enc/NOTES.md`.

## Memory facts found on the way

- The heap of the RAM image is two regions; the largest block is 112 KB when
  the image is small and shrinks once the image passes about 240 KB. A job
  stack must fit one block. PQMagic's ML-DSA-87 needs 125 KB of stack, so its
  suite defines `KEMSIG_EXTRA_HEAP`: `kemsig.c` then hands a static buffer to
  the heap as one more region (`heap_caps_add_region`).
- Aigis-sig parameter set 3 needs 110.9 KB of stack for signing with the
  upstream code, just under that limit; `aigis_sig_lowram` brings it down.

## Findings in upstream code

Found by the tamper test, the sanitizer sweep and the single-unit build.
None of them was caused by the port; the first is fixed in the vendored copy,
the others are left as they are and reported.

1. **Aigis-Sig+ (PQMagic copy), stack overflow.** `polyvecl_uniform_gamma1()`
   calls `polyz_unpack()` once more after its loop and writes a whole
   polynomial (2 KB) past the end of the vector `y` in `msig_sign()`. Built
   file by file the write lands on a variable that is overwritten later;
   built as one unit it broke every signature of parameter set I. The stray
   call is disabled in `third_party/pqmagic` (see its `SOURCE.md`).
2. **Aigis-Enc+ (PQMagic copy), decapsulation.** `mkem_dec()` copies the
   shared secret out before the re-encryption check and never replaces it, so
   a modified ciphertext that still decrypts to the same message is accepted
   with the same key (seen for the 512 set with a one-bit change). It also
   reads `sk - SEED_BYTES`, before the start of the secret key. The suite
   reports the tamper outcome as information, not as a failed check.
3. **LAC-v3b, BCH decoder.** `compute_error_locator_polynomial()` reads and
   writes back `elp->c[j + k]` up to one element past the array. The value
   written is the value read, so results are not affected.
4. **Aigis-Sig+ (PQMagic copy), crash on the device.** `poly_uniform()` keeps
   its counter in an `int` and passes its address to `reject_uniform()`,
   which takes an `int32_t *`. With the ESP-IDF toolchain `int32_t` is
   `long`, so under strict aliasing GCC removed the initialization of the
   counter and the first key generation wrote through a garbage index
   (store access fault). The same mismatch between `int`/`unsigned int` and
   the 32-bit typedefs appears all over the upstream code bases, so every
   suite here is compiled with `-fno-strict-aliasing`
   (`component.cmake`).
5. Undefined behaviour that the compilers used here handle as the authors
   intended: signed overflow in Montgomery reductions (PQMagic ML-KEM), left
   shifts of negative values (NEV, NEV-AKE, Aigis-Sig+), unaligned 32-bit
   loads (Aigis-enc `cbd.c`, LAC `bin-lwe.c`). Device and host outputs agree.

## Not ported

- **QIMEN-PIKE** (PQMagic `kem/qimen-pike`): isogeny-based, built on the
  SQIsign code base, needs GMP and the LGPL DPE library; 225 files. A bignum
  replacement is not a small job, so it was left out.
- **ReSolveD-alpha** (PQMagic `sig/resolved-alpha`): the reference keeps the
  whole seed tree and all commitments on the heap. From the allocation sizes
  in `bavc.c` and `vole.c` and the parameters, the 160f set needs about
  0.4 MB (3328 leaves: 133 KB of tree nodes, 133 KB or more of commitments,
  67 KB of seeds, plus the VOLE matrices) and 160s over 1 MB (28672 leaves).
  The device has about 0.2 MB of heap. These numbers are computed from the
  source, not measured.
- **SPHINCS-alpha 192s, 256f, 256s**: the encoding table needs 332 KB,
  364 KB and 769 KB even in compact form.
- **SCloud+ 384 and 512**: see `suites/scloudplus/NOTES.md`.
