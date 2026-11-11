<!-- Final report of this line of work, as delivered to the integrator. -->

Fork C: FN-DSA (Falcon-512/1024) and HQC-1/3/5 are ported, verified on the device against the host, and optimized; nothing committed, no shared file edited.

All `opt` times below were measured with the reference Keccak. In my builds `pqc_keccakf1600_opt` was still the weak default, so fork A's assembly is not in these numbers. Rerun `fndsa`, `hqc` and `hqc3ref` after integrating it: an HQC-1 decapsulation spends about 100 permutations (close to half its time), a signature 161 (512) or 313 (1024).

## FN-DSA (suite `fndsa`)

Source is Thomas Pornin's c-fn-dsa (commit 1b858880, Unlicense), vendored in `third_party/c-fn-dsa`. I chose it over PQClean's Falcon because it tracks the FIPS 206 draft, has a caller-provided work area for every operation, and emulates floating point bit-exactly without an FPU. One local change: `sha3.c` gained `FNDSA_EXTERNAL_KECCAK` so the permutation goes through `pqc_prims.h`.

| | ref | opt | Speedup | Stack | Work area (static) |
|---|---|---|---|---|---|
| 512 keygen (4 seeds) | 533 ms (431–666) | 533 ms | 1.00 | 1.0 KB | 11295 B |
| 512 sign | 288.6 ms | 228.5 ms | 1.26× | 2.3 KB | 30239 B |
| 512 verify | 4.10 ms | 4.10 ms | 1.00 | 1.1 KB | 2079 B |
| 1024 keygen (2 seeds) | 1713 ms (1286–2140) | 1713 ms | 1.00 | 1.0 KB | 22559 B |
| 1024 sign | 623.5 ms | 490.2 ms | 1.27× | 2.4 KB | 60447 B |
| 1024 verify | 8.22 ms | 8.22 ms | 1.00 | 1.1 KB | 4127 B |

Sizes: 512 is sk 1345, vk 897, sig 666 bytes; 1024 is sk 2369, vk 1793, sig 1280. No heap use.

- **Optimization:** `fpr_c3.c` redoes the emulated double addition and multiplication on 32-bit halves and adds a combined x+y / x−y routine used by every FFT butterfly. Instruction counts drop from 197 to 139 (add), 2×197 to 222 (add and sub together), and 103 to 80 (mul). It is used only in a second copy of the signing code (`fndsa_sign_c3.c`); keygen and verify are shared.
- **Verification on the device:** upstream KAT (2 vectors per set on the device, 10 on the host), the verification vector plus a bit-flip, and a 20000-pair comparison against upstream's routines. `tools/pqc check fndsa` gives 12 digests equal; the host signs with hardware floating point, the device with emulation.
- **Host test:** `suites/fndsa/tests/run_fpr_host.sh` compared the new routines against upstream and against native doubles on one billion pairs with no mismatch.
- **Compiler finding:** GCC 14.2 for RV32IMC turned my masked selects into conditional branches on secret operands. `fpr_c3.c` now uses value barriers and its object code has no branches. Upstream's own `fpr_add` also gets such branches from this compiler, so the `ref` variant is not branch-free on this target. This probably affects other suites' "constant-time" C as well.
- **Not done:** division and square root are still upstream's bit-serial loops (about 9% and 2% of an optimized signature); keygen was not touched.
- **`-O3` dropped:** for the whole library it does not link (2 KB over the 321 KB RAM-image limit). For the signing unit alone the unit grows from 32 KB to 60 KB and the image crashed at startup; I did not investigate, so there is no timing.

## HQC (suites `hqc`, `hqc3ref`)

Source is the HQC team's reference v5.1.0 (commit 06567ccb, 2026-10-26, public domain), vendored unmodified in `third_party/hqc`. PQClean carries the 2023 round-4 version, whose keys and vectors differ. I could not confirm whether NIST's draft matches v5.1.0.

| | ref | opt | Speedup | Stack ref | Stack opt |
|---|---|---|---|---|---|
| HQC-1 keygen | 276.9 ms | 5.52 ms | 50× | 52.9 KB | 10.8 KB |
| HQC-1 encaps | 553.3 ms | 11.46 ms | 48× | 59.1 KB | 13.3 KB |
| HQC-1 decaps | 1117.7 ms | 27.85 ms | 40× | 66.1 KB | 20.4 KB |
| HQC-3 keygen | 837.4 ms | 12.61 ms | 66× | 105.4 KB | 20.2 KB |
| HQC-3 encaps | 1672.5 ms | 27.13 ms | 62× | 118.4 KB | 25.4 KB |
| HQC-3 decaps | 3360.2 ms | 61.27 ms | 55× | 132.3 KB | 39.3 KB |
| HQC-5 keygen | not run | 22.85 ms | n/a | ~166 KB (estimate) | 31.3 KB |
| HQC-5 encaps | not run | 50.19 ms | n/a | ~188 KB (estimate) | 39.6 KB |
| HQC-5 decaps | not run | 112.1 ms | n/a | ~211 KB (estimate) | 61.6 KB |

Sizes (pk / sk / ct / ss): HQC-1 2241 / 32 / 4433 / 32; HQC-3 4514 / 32 / 8978 / 32; HQC-5 7237 / 32 / 14421 / 32. No heap use.

What `opt` replaces (`hqc_unit_opt.inc`); the Reed-Solomon coder, sampling, parsing and KEM layer stay upstream:
1. **Sparse × dense polynomial products** (`hqc_c3_pke.c`, `hqc_c3_mul.S`) instead of dense Karatsuba. One product takes 375K cycles for HQC-1 against about 270 ms in the reference; the work area is 2n bits instead of 18n. The assembly loop is 1.6× faster than the same loop in C.
2. **GF(2^8) multiplication via the hardware multiplier** (`hqc_c3_gf.c`), no tables. Reed-Solomon decoding drops from 1039K to 206K cycles (HQC-1).
3. **Word-wise SHAKE/SHA-3 sponge** (`hqc_c3_sym.c`), about 10% off each hash.
4. **Reed-Muller decoder** (`hqc_c3_rm.c`), 648K to 340K cycles (HQC-1).

- **Side channel, called out in NOTES.md:** the sparse product runs a fixed instruction sequence, but the secret positions choose memory addresses and shift counts. On the C3 this does not affect timing: the on-device job `ct-mul` times it with interrupts off for 24 supports, including adversarial ones, and gets identical cycle counts on all three levels. On a cached CPU, or against power analysis, it leaks where the reference does not.
- **Verification on the device:**
  - Official KAT vectors pass: 1 for ref and 3 for opt on the device, all 100 per level on the host.
  - Corrupted-ciphertext decapsulation takes the same time as a valid one to the microsecond and returns the reference rejection key.
  - GF(2^8) is checked exhaustively against the reference.
  - Reed-Muller and Reed-Solomon are checked against the reference on random words (8 rounds on the device, 200000 on the host).
  - `tools/pqc check hqc` gives 20 digests equal and `hqc3ref` 5, with the host running reference code for both variants.
- **Upstream bug found:** `BITMASK` uses `1UL`, so HQC-5 (shift of 37) produces wrong keys and ciphertexts on any 32-bit target, in the reference code too. I override the macro in the unit files, and add an empty `immintrin.h` because the ref headers include it needlessly. No upstream file was changed.
- **RAM limits found:**
  - The heap is made of regions of at most 128 KB, so the largest stack the harness can allocate was about 115 KB in my images.
  - A RAM image may occupy 321 KB statically.
  - HQC-3 ref decaps needs 132 KB of stack, so it runs in its own image `hqc3ref` on a static stack (`hqc_bigstack.c`, which prints its own `@RESULT` lines).
  - HQC-5 ref needs about 211 KB plus firmware and cannot run RAM-loaded; it is host-verified only.
- **Not done:** h and y are derived twice per decapsulation (about 19 of the 100 permutations), and a cached-key decapsulation would save about a quarter; both need changes to the upstream KEM layer.

## Files

- `third_party/c-fn-dsa/` (library sources, LICENSE, README.md, SOURCE.md)
- `third_party/hqc/` (`src/common`, `src/ref`, LICENSE, README.md, CHANGELOG, SOURCE.md)
- `suites/fndsa/`:
  - NOTES.md
  - gen_kat.py
  - tests/test_fpr_host.c
  - tests/run_fpr_host.sh
  - pqc_suite/: CMakeLists.txt, suite.cmake, suite.c, fndsa_glue.c, fndsa_sign_c3.c, fpr_c3.c, fndsa_c3_rename.h, kat_fndsa.h
- `suites/hqc/`:
  - NOTES.md
  - gen_kat.py
  - pqc_suite/: CMakeLists.txt, suite.cmake, suite.c, hqc_api.h, hqc_glue.c, hqc_rename.h, hqc_unit_ref.inc, hqc_unit_opt.inc, hqc1_ref.c, hqc3_ref.c, hqc5_ref.c, hqc1_opt.c, hqc3_opt.c, hqc5_opt.c, hqc_c3_pke.c, hqc_c3_mul.S, hqc_c3_gf.c, hqc_c3_rm.c, hqc_c3_sym.c, hqc_probe.c, hqc_selftest.c, hqc_bigstack.c, immintrin.h, kat_hqc.h (273 KB, all 300 official vectors)
- `suites/hqc3ref/`:
  - NOTES.md
  - pqc_suite/CMakeLists.txt
  - pqc_suite/suite.cmake
- `results/`: fndsa.json, fndsa.host.json, hqc.json, hqc.host.json, hqc3ref.json, hqc3ref.host.json
- Git-ignored clones `downloads/c-fn-dsa` and `downloads/hqc-official`, needed only to regenerate the KAT headers.

## Shared files

No change is required. Things you may want to pick up:
- **Harness:** a `bench_run` variant with a caller-provided static stack would replace my hand-printed `@RESULT` lines in `hqc_bigstack.c`.
- **`components/prims/fips202.c`:** the incremental absorb/squeeze is byte-wise with 64-bit shifts, costing about 10% in hash-heavy schemes. `hqc_c3_sym.c` shows the word-wise version.
- **`tools/pqc host`:** `-D` options persist in `build/host/<suite>`. I left `hqc` at `HQC_HOST_OPT=0`, which is the reference for both variants.
