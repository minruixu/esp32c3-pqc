<!-- Final report of this line of work, as delivered to the integrator. -->

Fork B report: SLH-DSA (all 12 sets), the SHA-256 and SM3 primitives, and the `hashes` micro-benchmark suite.

All 12 parameter sets run on the board in both variants. All 88 checks pass (NIST ACVP keyGen and sigGen vectors, two ACVP sigVer vectors, tamper rejection), and all 72 key/signature fingerprints equal the host run of slhdsa-c's own code (`tools/pqc check slhdsa`). `hashes`: 11 checks pass, 2 fingerprints equal. Nothing is committed by me.

## SLH-DSA, ref vs opt on the board (ms)

| Set | pk/sk/sig bytes | keygen ref → opt | sign ref → opt | verify ref → opt | opt stack |
|---|---|---|---|---|---|
| SHA2-128s | 32/64/7856 | 8298 → 360 (23.0x) | 63481 → 2890 (22.0x) | 60.6 → 3.57 (17.0x) | 4.4 KB |
| SHA2-128f | 32/64/17088 | 130 → 5.64 (23.1x) | 3057 → 143 (21.4x) | 177 → 9.79 (18.1x) | 4.5 KB |
| SHA2-192s | 48/96/16224 | 12478 → 1022 (12.2x) | 130467 → 28863 (4.5x) | 112 → 32.8 (3.4x) | 5.6 KB |
| SHA2-192f | 48/96/35664 | 195 → 16.0 (12.2x) | 5587 → 1013 (5.5x) | 309 → 56.5 (5.5x) | 5.6 KB |
| SHA2-256s | 64/128/29792 | 8373 → 828 (10.1x) | 123245 → 35211 (3.5x) | 173 → 47.7 (3.6x) | 6.5 KB |
| SHA2-256f | 64/128/49856 | 523 → 51.8 (10.1x) | 11679 → 2303 (5.1x) | 322 → 69.5 (4.6x) | 6.5 KB |
| SHAKE-128s | 32/64/7856 | 34940 → 23255 (1.5x) | 265752 → 176747 (1.5x) | 272 → 181 (1.5x) | 4.2 KB |
| SHAKE-128f | 32/64/17088 | 546 → 363 (1.5x) | 12751 → 8482 (1.5x) | 760 → 506 (1.5x) | 4.2 KB |
| SHAKE-192s | 48/96/16224 | 51271 → 34008 (1.5x) | 463104 → 306182 (1.5x) | 380 → 252 (1.5x) | 4.2 KB |
| SHAKE-192f | 48/96/35664 | 801 → 531 (1.5x) | 20739 → 13726 (1.5x) | 1112 → 739 (1.5x) | 4.2 KB |
| SHAKE-256s | 64/128/29792 | 33985 → 22435 (1.5x) | 408292 → 267920 (1.5x) | 561 → 370 (1.5x) | 4.2 KB |
| SHAKE-256f | 64/128/49856 | 2124 → 1402 (1.5x) | 42833 → 28196 (1.5x) | 1113 → 736 (1.5x) | 4.2 KB |

- **Stack and heap:** ref stack is 4.0–5.0 KB; nothing uses the heap.
- **Long operations:** three single signing operations exceeded five minutes and were measured once: SHAKE-192s (463 s ref, 306 s opt) and SHAKE-256s ref (408 s).
- **How the rows were collected:** from separate runs of images built at different points, not one run of the final image (that would hold the board about an hour).
  - The SHA-2 `opt` rows and SHAKE-128f `opt` are from the final code.
  - The `ref` rows and the other SHAKE `opt` rows are from earlier images whose code for those paths did not change afterwards.
  - The final image rebuilt as `slhdsa` has the same size as the one measured.
- **SHA-512 bound:** at n = 24/32 the standard uses SHA-512 for H and T_l, which the chip has no hardware for. By count, about 27 of the 35 s of a 256s signature are SHA-512 on FORS nodes.

## Primitives, per block

| Primitive | ref | opt |
|---|---|---|
| SHA-256, one block per call | 27.6 µs (4422 cycles) | 3.94 µs (630), 7.0x |
| SHA-256, 16 blocks per call | 24.4 µs | 1.36 µs (218 cycles), 18x |
| SHA-256 of a 55-byte message via `sha256()` | 36.7 µs | 9.44 µs |
| Same with new `pqc_sha256_hw_short()` | n/a | 3.79 µs |
| SM3 | 39.1 µs (6249) | 26.3 µs (4215), 1.48x |
| SHA-512 | 80.8 µs PQClean, 85.3 µs slhdsa-c | 69.2 µs (11066), 72.7 µs from bytes |

- **Accelerator probe:** one register access costs 8–9 cycles; start plus wait is 88 cycles (7 polls).
- **Accelerator used directly:** 364 cycles for load state, one block, read 16 bytes. Inside SLH-DSA it is about 280 cycles per hash with only changed words written, and about 195 with two chains in flight, against about 4200 for slhdsa-c's C.

## What was optimized

- **`pqc_sha256_compress_opt`** (`sha256_hw.c/.h`):
  - loads and reads the state once per call;
  - swaps bytes inline (the libgcc bswap cost 430 cycles per call);
  - uses `SHA_START` when the state is the initial value;
  - writes the next block while the previous one is compressed. The probe confirms on the chip that the accelerator copies the block at start, keeps its message registers, and accepts partial updates.
- **SLH-DSA SHA-2** (`slh_sha2_c3.c`, `slh_sha2_c3_tmpl.h`):
  - the midstate goes straight to the H registers with no byte swaps;
  - only changed message words are written (5 of 16 in a chain);
  - only n bytes are read back;
  - two hashes are kept in flight for WOTS+ key generation, FORS leaf pairs and verification chains.
- **Upstream hooks:** the two-in-flight scheme needed three optional hooks in slhdsa-c (`wots_pkgen`, `fors_leaf2`, `wots_chains`). All changes are guarded by `SLH_PQC_PRIMS` and listed in `third_party/slhdsa-c/SOURCE.md`.
- **SHA-512** (`sha512_gen.py` → `sha512_rv32imc.S`): generated assembly with no spills and no load-use stalls, 11,042 instructions per block. The generator runs it in a small RV32 interpreter against Python's SHA-512, and it was correct on the board at the first run. H and T_l for n = 24/32 are built on it from a native midstate.
- **SM3** (`sm3_fast.c`): unrolled C with pre-rotated constants, 40 instructions per round.
- **SHAKE sets** (`slh_shake_fast.c`): F, PRF, H and the chain step written directly on the 25 lanes, plus fork A's Keccak. The split between the two was not measured separately.
- **Host model:** `suites/slhdsa/hosttest/run.sh` runs the ESP32-C3 code paths against a register-level model of the accelerator, with no board.

## Failed or left undone

- Reading the accelerator without waiting: wrong 2000 of 2000 times.
- SHA-512 in C with 32-bit halves: 83.2 µs, no gain, removed.
- PQClean's SHA-512 for `opt`: 42 KB of code, image did not fit.
- Not done: SM3 assembly, DMA mode, specialised SHA-512 for H (no RAM left), interleaving tree nodes, pre-hash SLH-DSA, any lock around the accelerator.
- The fully stored ACVP sigVer vectors were cut from four to two to fix a start-up crash (below).

## Files

- **Added:**
  - `third_party/slhdsa-c/`
  - `suites/slhdsa/`: `NOTES.md`, `gen_kat.py`, `make_table.py`, `hosttest/{run.sh,mock.c,sha256_hw.h}`, `pqc_suite/{CMakeLists.txt,suite.cmake,suite.c,slh_glue.c,slh_c3.h,slh_sha2_c3.c,slh_sha2_c3_tmpl.h,slh_shake_fast.c,kat_vectors.h}`
  - `suites/hashes/`: `NOTES.md`, `make_table.py`, `pqc_suite/{CMakeLists.txt,suite.cmake,suite.c,hw_probe.c}`
  - `components/prims/opt/{sha256_hw.c,sha256_hw.h,sm3_fast.c,sha512_gen.py,sha512_rv32imc.S}`
  - `journal/experiments/hashes.jsonl` (9 attempts) and `journal/experiments/slhdsa.jsonl` (10 attempts), rejected ones included
  - `results/{slhdsa,hashes}.json` and their `.host.json`
- **Shared files:** none edited. My `-u` linker workarounds are removed now that prims is `WHOLE_ARCHIVE`; no `opt` number of mine predates having the objects linked.

## For you

- **Start-up crash:** an image that leaves about 10 KB of the main RAM segment free crashes at boot with an instruction access fault and no output; 20 KB runs. `slhdsa` now leaves 28 KB. `WHOLE_ARCHIVE` adds about 14 KB to every image, so other large suites may hit this.
- **`--no-build` race:** such a run loads whatever binary is in the build directory when it gets the lock. I rebuilt once while a run was queued and got a boot crash that fits a half-written image.
- **Board lock:** it is not fair. My runs waited 10–40 minutes each, and one other run held the board for 25 minutes.
- **SHA-512 in prims:** `components/prims/sha2.c` could call `pqc_sha512_compress_rv32` (72.7 µs from bytes against 80.8). There is no SHA-512 switch in `pqc_prims.h`.
- **Hash-chain suites:** through `sha256()` the hash-interface bookkeeping, not the accelerator, is now the cost (9.4 µs against 3.8 µs with `pqc_sha256_hw_short()`). LMS and XMSS could use the helper or `sha256_hw.h` directly.
- **License check before publishing:** `kat_vectors.h` is generated from NIST ACVP-Server sample vectors. I did not check that repository's licence terms.
