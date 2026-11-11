<!-- Final report of this line of work, as delivered to the integrator. -->

**Fork E report: Chinese open-source PQC on the ESP32-C3 (PQMagic's Aigis and SM3 variants, PQMagic's NGCC references, SCloud+, LAC).**

Most of it is ported and matches the host on the device. Two suites are not finished: `scloudplus_256` fails in its SM3 family on the device, and `pqmagic_sphincsa_192f` does not run on the device at all. Nothing was committed and flash was never written.

## Read this first

- **Most `opt` numbers predate the prims link fix.** Until 22:35 none of my images contained the optimized Keccak, SHA-256 or SM3 (weak defaults won), so `opt` equals `ref` there except for my own changes.
- **Runs with the shared primitives linked:** `scloudplus`, `scloudplus_256`, `pqmagic_sphincsa` (128f), `pqmagic_aigis_sig_plus`. Everything else was last run before that.
- **Not re-run after later build changes:** `-fno-strict-aliasing` and the lean one-shot `sm3()` path were added after the device runs of `aigis_enc`, `aigis_sig`, `pqmagic_mlkem`, `pqmagic_mldsa`, `pqmagic_mldsa87`, `pqmagic_slhdsa_sm3`, `pqmagic_nev*`, `pqmagic_aigis_enc_plus` and `lac`. Those results are valid for the build they ran; the current build of them is host-verified only.
- **`results/aigis_sig.json` has a stale `fails=20`** from a first run that was later merged; all 48 jobs in it have rc 0 and 36/36 fingerprints match.

## Status

| Algorithm | Suite | Device status |
|---|---|---|
| Aigis-enc 1–4, SM3 and SHAKE | `aigis_enc` | verified, 64/64 fingerprints |
| Aigis-sig 1–3, SM3 and SHAKE | `aigis_sig` | verified, 36/36 |
| Aigis-sig low-stack | `aigis_sig_lowram` | verified, 36/36, ref and opt outputs equal |
| ML-KEM-512/768/1024 with SM3 (plus SHAKE as anchor) | `pqmagic_mlkem` | verified, 48/48; SHAKE build passes the FIPS 203 vectors |
| ML-DSA-44/65 with SM3 (plus SHAKE) | `pqmagic_mldsa` | verified, 24/24; SHAKE build passes the FIPS 204 vectors |
| ML-DSA-87 with SM3 (plus SHAKE) | `pqmagic_mldsa87` | verified, 12/12; needs an extra heap region for its 125 KB stack |
| SLH-DSA-SM3 128f/128s, simple and robust | `pqmagic_slhdsa_sm3` | verified, 24/24 |
| SPHINCS-α 128f (SM3, SHA-2, SHAKE) | `pqmagic_sphincsa` | verified, 18/18 |
| SPHINCS-α 128s | `pqmagic_sphincsa` | host only, not run on the device |
| SPHINCS-α 192f | `pqmagic_sphincsa_192f` | **fails**: table allocation runs out of heap (162 KB free vs 128 + 15 + 20 KB) |
| SPHINCS-α 192s, 256f, 256s | — | not feasible in RAM: table is 332–769 KB even compacted |
| Aigis-Enc+ 512/1024/2048 | `pqmagic_aigis_enc_plus` | verified, 24/24; upstream decapsulation accepts a modified ciphertext at 512 |
| Aigis-Sig+ set I | `pqmagic_aigis_sig_plus` | verified, 6/6 |
| Aigis-Sig+ sets II, III | `pqmagic_aigis_sig_plus` | host only; set II signing overflowed a 112 KB stack |
| NEV, 12 sets, SHAKE config | `pqmagic_nev` | verified, 96/96 |
| NEV, 12 sets, SM3 config | `pqmagic_nev_sm3` | verified, 96/96 |
| NEV-AKE, 9 sets | `pqmagic_nev_ake` | verified, 90/90 |
| SCloud+ 128/192, SM3, SHAKE, AES | `scloudplus` | verified, 48/48; submission KATs pass in ref and opt |
| SCloud+ 256, SHAKE and AES | `scloudplus_256` | verified, KATs pass |
| SCloud+ 256, SM3 | `scloudplus_256` | **fails**: upstream SM3 helper cannot allocate its buffers; passes on host |
| SCloud+ 384/512 | — | not feasible: about 150 KB stack plus 107 KB of key buffers; KATs pass on host |
| LAC-v3b Light/128/192/256 | `lac` | verified, 32/32; no upstream vectors exist, not compared with an OpenSSL build |
| QIMEN-PIKE | — | skipped: needs GMP and an LGPL library |
| ReSolveD-α | — | not feasible: about 0.4 MB heap (160f) to over 1 MB (160s), computed from the source |
| Other 2019 competition winners | — | not surveyed |

## Results (ms, ref→opt where they differ; stack is the largest of the operations)

**SCloud+** (shared primitives linked; keygen / encaps / decaps)

| Set | keygen | encaps | decaps | stack KB | pk / sk / ct |
|---|---|---|---|---|---|
| 128 AES | 1380→169 | 1372→148 | 1364→145 | 32 | 6096 / 7440 / 6160 |
| 128 SHAKE | 712→438 | 704→418 | 697→415 | 33 | same |
| 128 SM3 | 901→565 | 890→542 | 886→541 | 34 (+19 KB heap) | same |
| 192 AES | 2752→406 | 2735→351 | 2730→355 | 60 | 11456 / 13872 / 12645 |
| 192 SHAKE | 1506→922 | 1529→869 | 1523→873 | 60 | same |
| 192 SM3 | 1977→1148 | 2007→1087 | 2009→1094 | 61 (+35 KB heap) | same |
| 256 AES | 5520→841 | 5573→714 | 5563→716 | 83 | 16296 / 19680 / 17925 |
| 256 SHAKE | 3292→1783 | 3257→1669 | 3247→1672 | 84 | same |
| 256 SM3 | 3917→2329 | 4057→2200 | 4079→2221 | 84 | same; **outputs wrong on the device** |

**LAC** (opt is hardware AES only)

| Set | keygen | encaps | decaps | stack KB | pk / sk / ct |
|---|---|---|---|---|---|
| Light | 4.9→2.3 | 7.0→3.5 | 8.1→4.7 | 13 | 544 / 800 / 648 |
| 128 | 8.8→4.4 | 13.7→7.7 | 17.0→11.0 | 14 | 544 / 1056 / 704 |
| 192 | 12.9→8.4 | 19.8→13.9 | 25.5→19.5 | 26 | 1056 / 1568 / 1352 |
| 256 | 18.5→12.4 | 31.4→23.1 | 43.1→34.8 | 27 | 1056 / 1824 / 1464 |

**Aigis-enc** (no primitives linked; opt equals ref)

| Set | SM3 keygen / encaps / decaps | SHAKE keygen / encaps / decaps | stack KB | pk / sk / ct |
|---|---|---|---|---|
| 1 | 6.5 / 10.5 / 10.9 | 6.0 / 10.2 / 10.2 | 11 | 672 / 1568 / 736 |
| 2 | 9.8 / 12.7 / 13.2 | 8.9 / 12.5 / 12.5 | 15 | 896 / 2208 / 992 |
| 3 | 10.1 / 13.2 / 13.6 | 9.1 / 12.7 / 12.5 | 15 | 992 / 2304 / 1056 |
| 4 | 18.1 / 23.1 / 23.6 | 16.1 / 21.6 / 21.3 | 21 | 1440 / 3168 / 1568 |

**Aigis-sig** (no primitives linked; low-stack columns are from `aigis_sig_lowram`)

| Set | SM3 keygen / sign / verify | SHAKE keygen / sign / verify | stack KB keygen / sign / verify | low-stack KB | low-stack SHAKE sign | pk / sk / sig |
|---|---|---|---|---|---|---|
| 1 | 23.7 / 66.0 / 23.9 | 17.4 / 82.4 / 17.4 | 37 / 66 / 41 | 28 / 57 / 32 | 81.7→170 | 1056 / 2448 / 1852 |
| 2 | 37.2 / 66.6 / 37.1 | 27.0 / 35.8 / 26.7 | 52 / 87 / 55 | 35 / 71 / 39 | 35.6→35.6 | 1312 / 3376 / 2445 |
| 3 | 53.7 / 70.3 / 53.8 | 38.5 / 71.3 / 37.9 | 68 / 111 / 72 | 43 / 85 / 46 | 70.6→126 | 1568 / 3888 / 3046 |

Low-stack keygen and verify take the same time as upstream.

**PQMagic ML-KEM and ML-DSA** (no primitives linked; opt equals ref)

| Set | SM3 | SHAKE | stack KB |
|---|---|---|---|
| ML-KEM-512 | 5.7 / 6.3 / 7.2 | 5.4 / 5.9 / 6.8 | 11 |
| ML-KEM-768 | 10.3 / 11.3 / 12.4 | 8.9 / 9.8 / 11.0 | 16 |
| ML-KEM-1024 | 16.9 / 17.9 / 19.5 | 14.0 / 15.0 / 16.6 | 22 |
| ML-DSA-44 | 30.3 / 85.9 / 27.7 | 20.2 / 30.6 / 21.3 | 53 |
| ML-DSA-65 | 53.2 / 88.7 / 48.8 | 35.6 / 98.2 / 36.0 | 80 |
| ML-DSA-87 | 89.2 / 109 / 85.0 | 59.8 / 78.0 / 61.0 | 122 |

Sizes are the standard ML-KEM and ML-DSA sizes. Signing is deterministic in PQMagic's default.

**Hash-based signatures** (keygen / sign / verify)

| Set | Time | stack KB | sig bytes |
|---|---|---|---|
| SLH-DSA-SM3-128f | 199 / 4735 / 292 | 3 | 17088 |
| SLH-DSA-SM3-128f robust | 381 / 8931 / 569 | 4 | 17088 |
| SLH-DSA-SM3-128s | 12728 / 98925 / 106 | 3 | 7856 |
| SLH-DSA-SM3-128s robust | 24448 / 185793 / 209 | 4 | 7856 |
| SPHINCS-α-SM3-128f | 192→128 / 4889→3455 / 259→186 | 3.5 (+66 KB heap table) | 16720 |
| SPHINCS-α-SHA2-128f | 160→50 / 4110→1307 / 220→77 | 3.5 | 16720 |
| SPHINCS-α-SHAKE-128f | 633→406 / 16280→10441 / 835→540 | 3.5 | 16720 |

SLH-DSA-SM3 rows have no primitives linked; SPHINCS-α rows do.

**NGCC references from PQMagic**

| Set | keygen / encaps / decaps (or sign / verify) | stack KB | pk / sk / ct or sig |
|---|---|---|---|
| Aigis-Enc+ 512 | 4.6 / 6.4 / 7.7 | 13 | 656 / 1456 / 896 |
| Aigis-Enc+ 1024 | 8.9 / 13.3 / 16.3 | 27 | 1312 / 2912 / 1664 |
| Aigis-Enc+ 2048 | 21.3 / 30.7 / 38.9 | 57 | 2624 / 5824 / 3584 |
| Aigis-Sig+ I | 11.3→8.3 / 46.9→39.0 / 12.4→9.6 | 75 | 928 / 2800 / ~2001 |

Aigis-Enc+ has no primitives linked; Aigis-Sig+ I does.

**NEV** (keygen / encaps / decaps; no primitives linked; opt equals ref)

| Set | SHAKE config | SM3 config | stack KB |
|---|---|---|---|
| 512-769-c | 2.9 / 2.4 / 3.1 | 2.6 / 2.1 / 3.0 | 6.5 |
| 1024-769-c | 6.7 / 5.2 / 6.6 | 6.0 / 4.8 / 6.9 | 15 |
| 2048-769-c | 20.8 / 13.2 / 14.9 | 20.3 / 12.5 / 15.9 | 38 |
| 512-1409 | 4.0 / 3.2 / 3.4 | 3.7 / 2.9 / 3.5 | 8 |
| 1024-1409 | 8.8 / 5.9 / 6.8 | 8.2 / 5.6 / 7.2 | 20 |
| 2048-1409 | 24.3 / 16.1 / 16.7 | 23.1 / 15.8 / 18.1 | 46 |
| 512-3329 | 4.5 / 4.2 / 4.2 | 4.8 / 4.5 / 4.7 | 9 |
| 1024-3329 | 8.5 / 7.1 / 7.3 | 7.9 / 6.8 / 7.9 | 16 |
| 2048-3329 | 23.7 / 17.9 / 16.7 | 23.4 / 18.1 / 18.8 | 40 |
| 512-769 | 2.9 / 2.7 / 3.0 | 2.6 / 2.3 / 2.8 | 7 |
| 1024-769 | 6.8 / 5.6 / 6.4 | 6.0 / 5.1 / 6.5 | 16 |
| 2048-769 | 20.8 / 14.0 / 14.5 | 20.3 / 13.5 / 15.6 | 38 |

NEV keys are 615–3072 bytes, secret keys 1246–6208, ciphertexts 512–3072. The SM3 config uses 0.5–6.7 KB of heap inside the organisers' helper.

**NEV-AKE** (keygen / init / response / finish; no primitives linked)

| Set | Time | stack KB |
|---|---|---|
| 512-769-c | 2.3 / 4.1 / 11.8 / 9.0 | 7 |
| 1024-769-c | 5.2 / 9.0 / 26.3 / 20.9 | 16 |
| 2048-769-c | 15.7 / 23.8 / 73.3 / 62.2 | 38 |
| 512-1409 | 3.2 / 5.6 / 14.3 / 10.5 | 9 |
| 1024-1409 | 7.2 / 11.5 / 29.7 / 23.5 | 20 |
| 2048-1409 | 18.9 / 29.4 / 88.1 / 73.0 | 46 |
| 512-769 | 2.2 / 4.3 / 12.6 / 9.5 | 8 |
| 1024-769 | 5.2 / 9.3 / 27.5 / 21.8 | 18 |
| 2048-769 | 15.7 / 24.6 / 78.3 / 66.1 | 40 |

Full per-job numbers are in `results/<suite>.json`; `suites/pqmagic_common/results_table.py <suite>` prints them.

## What I optimized

- **SCloud+ matrix products:** two 10-bit coefficients per 32-bit word with a biased secret, so one multiply updates both. Alone it took 128-SHAKE encapsulation from 724 to 602 ms.
- **SCloud+ AES family on the hardware AES accelerator:** together with the matrix products, 1351 to 157 ms at level 128, before any shared primitive.
- **SCloud+ SM3 row expansion:** one padded block per row instead of allocate, copy and pad per call. Not measured in isolation.
- **LAC seed expander on the hardware AES:** 1.2–2.2× (table above).
- **Lean SM3 paths for all PQMagic SM3 variants:** about 5% on Aigis-sig-3 key generation with the reference compression.
- **Aigis-sig low stack:** public matrix expanded one row at a time; 25 KB less at set 3, signing slower when it needs several rounds.
- **SPHINCS-α compact encoding table:** 278 KB to 50 KB (128f), 584 to 102 KB (128s), 497 to 128 KB (192f), identical signatures to upstream on the host.
- **Extra heap region** (`KEMSIG_EXTRA_HEAP`) so ML-DSA-87's 125 KB stack can exist.

One rejected attempt: shrinking the SCloud+-256 job stack to 90 KB did not fix its SM3 family. All attempts are in `journal/experiments/{scloudplus,lac,aigis,pqmagic}.jsonl`.

## Upstream bugs found

- **Aigis-Sig+ stack overflow:** a stray `polyz_unpack()` writes 2 KB past a vector in signing. Disabled in the vendored copy; signatures unchanged.
- **Aigis-Sig+ crash on this toolchain:** an `int` counter is read through `int32_t *` (which is `long *` here), and GCC dropped its initialization. All my suites now build with `-fno-strict-aliasing`.
- **Aigis-Enc+ decapsulation:** the shared secret is output before the re-encryption check and never replaced; it also reads before the start of the secret key. Left as is, reported as information.
- **LAC BCH decoder:** reads and writes back one element past an array. Harmless, left as is.
- **SCloud+ SM3 helper:** returns wrong hashes when `malloc` fails, with only a message on stderr.

Details are in `suites/pqmagic_common/NOTES.md`.

## Files added

- **Suites (18 directories, each with `NOTES.md`):** `aigis_enc`, `aigis_sig`, `aigis_sig_lowram`, `pqmagic_common` (shared code, not a suite), `pqmagic_mlkem`, `pqmagic_mldsa`, `pqmagic_mldsa87`, `pqmagic_slhdsa_sm3`, `pqmagic_sphincsa`, `pqmagic_sphincsa_192f`, `pqmagic_aigis_enc_plus`, `pqmagic_aigis_sig_plus`, `pqmagic_nev`, `pqmagic_nev_sm3`, `pqmagic_nev_ake`, `scloudplus`, `scloudplus_256`, `lac`.
- **Vendored code:** `third_party/pqmagic/` (Apache-2.0, subset, with `LICENSE` and `SOURCE.md` listing six local changes) and `third_party/pqclean_aes/` (MIT, unchanged, with `SOURCE.md`).
- **Fetch scripts:** `tools/fetch/pqmagic.sh`, `tools/fetch/scloudplus.sh`, `tools/fetch/lac.sh`. SCloud+ and LAC have no license and stay in `downloads/`.
- **Journal:** four files under `journal/experiments/`.
- **Results:** `results/<suite>.json` and `.host.json` for the suites above.

I edited no shared file.

## For you at integration

1. **Remove my `-u` line.** `suites/pqmagic_common/component.cmake` still passes `-u pqc_keccakf1600_rv32imc_present -u pqc_sha256_hw_init -u pqc_sm3_compress_rv32`; it is redundant with WHOLE_ARCHIVE and will break if those symbols are renamed.
2. **Skip `suites/pqmagic_common`** in anything that iterates over `suites/*`; it has no `pqc_suite` directory.
3. **Run the fetch scripts first** for `scloudplus`, `scloudplus_256` and `lac`; the build stops with a clear message otherwise.
4. **Decide on `suites/scloudplus/pqc_suite/kat_expected.h`.** It holds 64-byte seeds and SHA-256 digests taken from the unlicensed SCloud+ test vectors. It can be regenerated by the fetch script instead of being committed.
5. **`tools/pqc` merge keeps stale `fails` and `complete`** when a filtered run is merged into an existing result file.
6. **Consider `-fno-strict-aliasing` project-wide** for upstream code, and moving the extra-heap-region trick into the harness; the largest heap block is 112 KB and shrinks once an image passes about 240 KB.

## Publish or hold

- **Hold:** `scloudplus_256` (SM3 family fails on the device; SHAKE and AES are fine) and `pqmagic_sphincsa_192f` (does not run on the device).
- **Publish with the stated limits:** `pqmagic_sphincsa` (128s is host-only) and `pqmagic_aigis_sig_plus` (sets II and III are host-only).
- **Publish:** the other thirteen suites; `suites/pqmagic_common` is their shared code, not a suite.
