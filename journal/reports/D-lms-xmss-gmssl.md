<!-- Final report of this line of work, as delivered to the integrator. -->

**Fork D: LMS/HSS, XMSS/XMSS^MT (SP 800-208) and GmSSL's SM3-based PQ algorithms on the ESP32-C3.** No commits, no flash writes. No device run of mine is still queued.

## Status

| Suite | State | Checks | Device = host fingerprints |
|---|---|---|---|
| `lms` | complete | 27 pass | 49 equal |
| `xmss` | complete | 50 pass | 39 equal |
| `gmssl_kyber` | complete | 4 pass | 4 equal |
| `gmssl_lms` | complete (H5, HSS H5/H5) | 10 pass | 8 equal |
| `gmssl_xmss` | complete (XMSS^MT 20/4) | 6 pass | 4 equal |
| `gmssl_sphincs` | partial: key generation only | 2 pass | 1 equal |

Nothing is throwaway. `gmssl_sphincs` is the only one to publish with a caveat.

- **Not feasible on this chip as GmSSL implements it:**
  - **SPHINCS+-SM3 signing:** it keeps 131 KB FORS trees in locals. With a 138 KB static stack the board gave no output for 20 minutes, so the job is now disabled on the device. That run held the board for those 20 minutes.
  - **`XMSS_SM3_10_256`:** key generation fails at once, because one 65.6 KB `malloc` finds no block.
- **Not run:**
  - `LMS_SM3_M32_H10` in GmSSL (it needs the same 65 KB allocation).
  - XMSS-SHAKE256_10_256 `ref` (about 10 minutes).
  - LMS height 15 and above on the device; height 15 is tested on the host only.
  - GmSSL SPHINCS+ `ref` key generation.
  - A host interop run against Cisco hash-sigs: it was prepared, but executing that external code was denied.

## Results (ESP32-C3, 160 MHz, from RAM; `ref` → `opt`)

**LMS/HSS** (own implementation, no heap, 10.7 KB per key, stack ≤ 3.1 KB, code 8.3 + 0.5 KB)

| Set | pk / sig (B) | keygen | sign | verify |
|---|---|---|---|---|
| SHA-256 H5 W8 | 56 / 1292 | 10.9 s → 473 ms | 161 → 7.19 ms | 209 → 9.21 ms |
| SHA-256 H5 W4 | 56 / 2348 | 1.37 s → 87.8 ms | 21.6 → 1.66 ms | 23.2 → 1.68 ms |
| SHA-256 H5 W2 * | 56 / 4460 | 737 → 95.7 ms | 13.0 → 1.96 ms | 11.3 → 1.77 ms |
| SHA-256 H5 W1 * | 56 / 8684 | 804 → 152 ms | 15.8 → 3.00 ms | 10.8 → 2.50 ms |
| SHA-256 H10 W4 * | 56 / 2508 | 44.0 → 3.38 s | 45.5 → 3.75 ms | 20.7 → 1.85 ms |
| SHA-256 H10 W8 * | 56 / 1452 | (347 s est.) → 20.2 s | – → 13.5 ms | – → 11.2 ms |
| SHA-256/192 H5 W8 | 48 / 780 | 8.23 s → 299 ms | 125 → 4.69 ms | 139 → 5.14 ms |
| SHA-256/192 H10 W4 * | 48 / 1500 | 33.0 → 2.19 s | 34.3 → 2.48 ms | 16.5 → 1.27 ms |
| SHAKE256/256 H5 W4 | 56 / 2348 | 5.82 → 2.87 s | 95.6 → 47.1 ms | 86.4 → 43.0 ms |
| SHAKE256/192 H5 W4 | 48 / 1380 | 4.40 → 2.17 s | 72.7 → 35.8 ms | 62.7 → 31.1 ms |
| SM3 H5 W8 (GmSSL numbering) | 56 / 1292 | 12.0 → 7.57 s | 182 → 115 ms | 198 → 125 ms |
| SM3 H10 W8 | 56 / 1452 | – → 242 s | – → 161 ms | – → 140 ms |
| HSS SHA-256 H5W8/H5W8 * | 60 / 2644 | 21.9 → 1.28 s | 441 → 25.9 ms (max 11.2 s → 650 ms) | 379 → 22.3 ms |

\* `opt` measured with the earlier loop version of the hardware chain; the final straight-line version is about 1.3× faster per step (only the unstarred SHA-256 rows were re-run with it).

Height estimates with the final code: H15 about 8 min (W8) or 1.5 min (W4); H20 about 4.3 h or 48 min.

**XMSS / XMSS^MT** (reference code with BDS traversal, no heap, stack ≤ 5 KB)

| Set | pk / sig / state (B) | keygen | sign | verify |
|---|---|---|---|---|
| XMSS-SHA2_10_256 | 64 / 2500 / 1373 (1569 opt) | 236 → 13.0 s | 511 → 25.7 ms | 130 → 7.10 ms |
| XMSS-SHA2_10_192 | 48 / 1492 / 1053 (1193) | 153 → 11.1 s | 328 → 21.3 ms | 78.3 → 5.56 ms |
| XMSSMT-SHA2_20/4_256 | 64 / 9251 / 10938 (11015) | 29.8 → 1.66 s | 918 → 39.9 ms | 476 → 26.3 ms |
| XMSSMT-SHA2_20/4_192 | 48 / 5403 / 7138 (7159) | 19.3 → 1.41 s | 592 → 33.0 ms | 336 → 24.0 ms |
| XMSSMT-SHAKE256_20/4_256 | 64 / 9251 / 10938 (11015) | 74.4 → 37.1 s | 2.27 s → 846 ms | 1.18 s → 591 ms |
| XMSSMT-SHAKE256_20/4_192 | 48 / 5403 / 7138 (7159) | 55.9 → 27.5 s | 1.70 s → 626 ms | 950 → 468 ms |
| XMSS-SHAKE256_10_256 | – | – → 294 s | – → 571 ms | – → 150 ms |

**GmSSL**

| Algorithm | keygen | sign / encaps | verify / decaps | stack / heap (B) |
|---|---|---|---|---|
| Kyber-768 | 23.7 → 9.70 ms | 25.7 → 11.5 ms | 29.3 → 15.1 ms | 16004 / 0 |
| LMS-SM3 H5 | 8.81 → 8.85 s | 136 → 137 ms | 146 → 146 ms | 3420 / 2048 |
| HSS-SM3 H5/H5 | 17.8 → 17.8 s | 135 → 136 ms | 283 → 285 ms | 12332 / 4096 |
| XMSS^MT-SM3 20/4 | 24.9 → 24.9 s | 89.2 → 89.4 ms | 387 → 388 ms | 29228 / 8192 |
| SPHINCS+-SM3 128s | – → 16.0 s | does not run | not measured | static 138 KB stack |

GmSSL implements LMS-SM3 H5–H25 (W8 only), XMSS-SM3 10/16/20, eight XMSS^MT-SM3 sets, one SPHINCS+ set (128s) and Kyber-768 only.

## What was optimized

- **LMS:**
  - Subtree caching, so a signature costs the one-time signature plus one leaf per layer.
  - Direct single-block hashing: 6250 → 4580 cycles per hash.
  - Winternitz chains on the SHA accelerator's registers: 641 cycles through the general hardware backend → 364 as a loop → 272 as straight-line code.
- **XMSS:**
  - Direct fixed-length hash layer plus a cached PRF state: 29.9 → 16.9 s.
  - BDS k = 3/4: signing 25 % faster.
  - PRF, F and H on the accelerator's registers: 2.93 s through the general backend → 1.66 s.
- **GmSSL:**
  - Kyber's SHA-3 as a direct sponge on the assembly Keccak: 2.4×.
  - For the SM3 algorithms, routing SM3 through `components/prims` was a regression of 6.6 %, and lean buffering around GmSSL's compression was still 2 % slower. Both are rejected; fast mode now keeps GmSSL's own SM3, so `opt` equals `ref`.

## Before/after WHOLE_ARCHIVE

- **After (optimized primitives really in the image):** every `opt` number in the tables above. LMS and XMSS `opt` were re-measured after the rebuild and were identical, because my suites had already pulled the backends in by name. All GmSSL runs are after.
- **Before (reference primitives):** only the intermediate steps in the journal (`lms-02`, `lms-04` before, `xmss-01` to `xmss-03`, `xmss-05` before).
- `ref` numbers are unaffected.

## Files (all untracked, mine)

- `suites/lms/`: `NOTES.md`, `extra/h15test.c`, `extra/hwmock/{sha256_hw.h,mock.c,run.sh}`, and in `pqc_suite/`: `CMakeLists.txt`, `suite.cmake`, `suite.c`, `lms.c`, `lms.h`, `lms_hw.c`, `lms_kat.h`, `lms_kat_sm3.h`, `gen_kat.py`.
- `suites/xmss/`: `NOTES.md`, and in `pqc_suite/`: `CMakeLists.txt`, `suite.cmake`, `suite.c`, `hash_ref.c`, `hash_opt.c`, `hash_hw.c`, `core_fast.c`, `core_slow.c`, `randombytes.h`, `xmss_port.h`, `shim/openssl/sha.h`.
- `suites/gmssl/`: `NOTES.md`, and in `common/`: `gmssl.cmake`, `suite.c`, `gmssl_port.c`, `gmssl_port.h`, `gmssl_rename.h`, `gmssl_sm3_ref.c`, `gmssl_sha3_ref.c`, `k_kyber.c`, `t_lms.c`, `t_xmss.c`, `t_sphincs.c`, `t_kyber.c`, `bigstack.c`.
- `suites/gmssl_{lms,xmss,sphincs,kyber}/`: `NOTES.md` and `pqc_suite/{CMakeLists.txt,suite.cmake}` each.
- `third_party/xmss-reference/` (CC0, commit 171ccbd) and `third_party/gmssl/` (Apache-2.0, commit 24ae482), each with `LICENSE` and `SOURCE.md`, files unchanged.
- `journal/experiments/lms.jsonl` (7 entries), `xmss.jsonl` (7), `gmssl.jsonl` (8, including 3 rejected and 1 partial).
- `results/{lms,xmss,gmssl_kyber,gmssl_lms,gmssl_xmss,gmssl_sphincs}.json` and their `.host.json`.

## For the integrator

- **SM3 backend:** the SM3 backend of `components/prims` is slower than GmSSL's own unrolled compression on this chip (6514 against 6191 µs per 256 blocks). Adopting GmSSL's function (Apache-2.0, 13 KB) or beating it would speed up `lms-sm3` and give GmSSL's SM3 algorithms a real `opt`.
- **Idle limits:** the following jobs run only when named and need `--idle 400` to `900`: `ref` of `lms-sha256-m32-h10-w8`, `xmss-sha2_10_*` and `xmssmt-shake256_20-4_192`, and both variants of `lms-sm3-m32-h10-w8` and `xmss-shake256_10_256`. Commands are in each `NOTES.md`.
- **Edited result files:** I removed stale failed entries from `results/gmssl_sphincs.json` and `results/gmssl_xmss.json` (the old SPHINCS+ `ref` run that could not get a stack, and the failed `gmssl-xmss-sm3-10` run). I also deleted the experiment result files `lms_exp_off` and `xmss_exp_off`; their numbers are in the journal and the build switches are documented.
- No shared file needs changing for my suites.
