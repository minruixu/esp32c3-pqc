# Timeline

All dates 2026-11-11 unless noted.

1. **Feasibility.** Identified the board as an ESP32-C3 (rev v1.1, 8 MB
   flash). Built ML-KEM-768 and ML-DSA-65 from mlkem-native and mldsa-native
   and ran them from RAM with `esptool load_ram`, so the board's own firmware
   stayed untouched (checked by an MD5 of the whole flash before and after).
   First numbers: ML-KEM-768 keygen 8.3 ms, ML-DSA-65 sign 112 ms average.
2. **Scope.** Surveyed what exists: NIST (ML-KEM, ML-DSA, SLH-DSA final;
   FN-DSA and HQC drafts; LMS and XMSS from SP 800-208) and China (PQMagic
   with Aigis and SM3 variants, GmSSL's SM3-based hash signatures, SCloud+,
   LAC, and the 84 public-key candidates of the NGCC first round published in
   September 2026).
3. **Framework.** One RAM image per algorithm family, a harness that reports
   time, stack and heap per operation, shared primitives behind a ref/opt
   switch, the same suites built for the host, and a fingerprint comparison
   between device and host. Verified with ML-KEM: 24 fingerprints equal.
4. **Parallel lines of work**, each on its own suites: (A) Keccak, ML-KEM,
   ML-DSA; (B) SLH-DSA, hardware SHA-256, SM3; (C) FN-DSA and HQC; (D) LMS,
   XMSS, GmSSL; (E) PQMagic, SCloud+, LAC; (F) the NGCC first-round packages.
5. **Finding: optimized primitives were not being linked.** The optimized
   backends replace weak defaults that live in the same static library. A
   linker does not pull an object out of an archive only to override a weak
   symbol, so every suite that did not reference the object by another name
   kept the reference code and reported `opt` equal to `ref`. Fixed by linking
   the primitives with `WHOLE_ARCHIVE`; images grew by about 14 KB. FN-DSA
   verification went from 4.10 ms to 3.21 ms once the assembly Keccak was
   really in the image.
6. **Findings from the FN-DSA and HQC line.**
   - The HQC reference code computes a bit mask with `1UL`, which is 32 bits
     on this target; HQC-5 shifts by 37, so keys and ciphertexts are wrong on
     any 32-bit platform. Overridden locally without touching upstream files.
   - GCC 14.2 for RV32IMC turns masked selects written to be constant-time
     into conditional branches on secret operands, in upstream code too.
   - The heap of a RAM image is split into regions of at most 128 KB, which
     caps a single task stack at about 115 KB.
7. **FN-DSA and HQC accepted.** Both suites were rerun independently after
   the linking fix; known-answer tests pass and all output fingerprints equal
   the host build.
8. **The Keccak, ML-KEM and ML-DSA line stopped early.** The session running
   it ended before it delivered its report and its experiment log. Its suites
   (`keccak`, `mlkem`, `mldsa`, `mldsa87`, `mldsa_lowram`), their notes and
   the results of its own last runs were on disk and are committed as they
   were: every job passes, and the fingerprints of those runs equal the host
   build. They have not been rerun independently, and there is no
   `experiments/*.jsonl` for them; the prose record is in each suite's
   `NOTES.md`.
9. **The PQMagic, SCloud+ and LAC line delivered** (report in
   `reports/E-china-open-source.md`). Ported and matching the host on the
   device: Aigis-enc, Aigis-sig, the SM3 instantiations of ML-KEM, ML-DSA and
   SLH-DSA, SPHINCS-alpha 128f, Aigis-Enc+, Aigis-Sig+ set I, NEV, NEV-AKE,
   SCloud+ 128/192/256 and LAC. Not feasible in RAM: SCloud+ 384/512,
   SPHINCS-alpha above 128f, ReSolveD-alpha. Skipped: QIMEN-PIKE (needs GMP).
   Open: SCloud+-256 with SM3 fails on the device because the submission's
   SM3 helper cannot allocate its buffers. Problems found in upstream code:
   - Aigis-Sig+ signing writes 2 KB past a vector (a stray unpack call), and
     reads an `int` through an `int32_t *`, which this toolchain treats as a
     different type; with strict aliasing the compiler dropped a store and the
     code crashed. All PQMagic suites are built with `-fno-strict-aliasing`.
   - Aigis-Enc+ decapsulation outputs the shared secret before the
     re-encryption check and never replaces it.
   - The LAC BCH decoder touches one element past an array.
   - The SCloud+ SM3 helper returns wrong hashes when `malloc` fails.
10. **Correction to entry 7 (2026-11-12).** When the HQC suites were committed
    as "verified by an independent rerun", that rerun had not finished: it was
    still queued behind the device lock, and the results committed then were
    the HQC line's own. The rerun has now completed with the assembly Keccak
    in the image: official known-answer vectors pass, 20 fingerprints equal the
    host build (5 more for the HQC-3 reference image), and HQC-1
    decapsulation is 22.3 ms instead of 27.9 ms. Lesson: read a run's own
    summary line before recording it as done, not the files it is expected
    to have written.
11. **The SLH-DSA and hash-primitive line stopped early (2026-11-12).** Like
    the Keccak line, the session running it ended before its final report.
    On disk and committed as they were: the `slhdsa` suite (all 12 FIPS 205
    parameter sets from slhdsa-c) and the `hashes` micro-benchmarks with
    their notes and experiment logs, the hardware SHA-256 driver, the SHA-512
    assembly and the fast SM3. Its result files are from its own runs and are
    not one consistent data set: `results/slhdsa.json` holds the reference
    variant for 11 sets and the optimized one for 3, `results/slhdsa_dev.json`
    holds an optimized run of all 12 sets, and `slhdsa_v3`/`slhdsa_v4` are
    later partial runs of newer builds. Every job in them passes; they have
    not been rerun independently.
12. **Correction to entry 11: the SLH-DSA and hash-primitive line finished.**
    It resumed and delivered its report (`reports/B-slhdsa-hashes.md`). All 12
    SLH-DSA parameter sets run on the board in both variants: 88 checks pass
    (NIST ACVP keyGen and sigGen vectors, two sigVer vectors, tamper
    rejection) and 72 fingerprints equal the host build. The SHA-2 sets gain
    3.4x to 23x from the SHA-256 accelerator; at the 192 and 256 levels the
    standard uses SHA-512 for part of the hashing, which the chip has no
    hardware for, so those gain less. The SHAKE sets gain 1.5x. The rows were
    collected from several runs, because one run of everything holds the board
    for about an hour. The work-in-progress result files of entry 11 are
    removed; `results/slhdsa.json` and `results/hashes.json` are the record.
    Two things it found that concern every suite:
    - An image that leaves only about 10 KB of the main RAM segment free
      crashes at boot without any output; 20 KB is enough.
    - A run started with `--no-build` loaded whatever binary was in the build
      directory when it got the board, so a rebuild during the wait could hand
      it a half-written image. `tools/pqc` now loads a private copy.
13. **The LMS, XMSS and GmSSL line delivered** (`reports/D-lms-xmss-gmssl.md`).
    LMS/HSS is an implementation written for this project (no heap, 10.7 KB
    per key); XMSS and XMSS^MT are the reference code with its OpenSSL hashing
    replaced; GmSSL contributes Kyber-768 and the SM3-based LMS, HSS and
    XMSS^MT. With the hash chains running on the SHA-256 accelerator's
    registers, LMS-SHA256 H5/W8 key generation went from 10.9 s to 0.47 s and
    XMSS-SHA2_10_256 key generation from 236 s to 13 s. Not feasible as GmSSL
    implements them: SPHINCS+-SM3 signing (131 KB of trees in local
    variables) and XMSS-SM3 with height 10 (one 65 KB allocation). One
    negative result worth keeping: routing GmSSL's SM3 through the shared
    primitive made it 6.6% slower, because GmSSL's own unrolled compression is
    faster on this chip than the project's "fast" SM3.
14. **The NGCC round-1 line delivered** (`reports/F-ngcc-round1.md`). All 84
    public-key submission packages were fetched from the official site
    (4.7 GB, kept out of the repository) and driven through one generic suite
    that selects the C files itself and never runs a package's build scripts.
    Of the 77 candidates it handled, 36 run on the ESP32-C3 with device
    fingerprints equal to the host (22 KEMs, 9 signatures, 5 key exchanges),
    and 33 of those also reproduce the first vector of their own KAT file on
    the chip. 22 more work on the host only (128-bit integers, images or keys
    larger than RAM, stack overflows, or errors that appear only on the
    32-bit target), 16 do not build on the host (x86 intrinsics, GMP,
    OpenSSL), and 3 fail their self-test. Routing SM3 through the shared
    primitive speeds up the candidates that hash with the organisers' helper
    by 1.0x to 1.6x.
15. **Rerun of the PQMagic, SCloud+ and LAC suites (2026-11-12).** Their
    optimized numbers predated the linking fix. Rerun from the current code,
    13 of 16 passed at once with all fingerprints equal to the host; the SM3
    instantiations now gain 1.2x to 1.5x from the fast SM3. Three images had
    run out of RAM because of the 14 KB the optimized primitives add: NEV-AKE
    could not get its job stack, PQMagic's ML-DSA-87 no longer linked, and
    SCloud+-256 kept failing in its SM3 family. Fixed with a per-suite switch
    that leaves unused backends out (`prims_off.txt`), a smaller stack request,
    a smaller reserved region, and an image of its own for SCloud+-256 with
    SM3, which then passed the submission's vectors on the chip for the first
    time. All 17 suites of this line now pass.
16. **Final rerun and cross-check of the documents (2026-11-12).** The hash
    primitives, the four GmSSL suites, LMS, XMSS and all 12 SLH-DSA sets were
    rerun from the final code in one batch, so that every result file in the
    repository comes from the same state of the shared primitives. All pass;
    fingerprints equal the host build everywhere (SLH-DSA 72, LMS 53, XMSS
    42). The jobs that take minutes are skipped in a plain run, so the batch
    had dropped them from the result files; they were run again with their
    filters and merged (XMSS reference key generation up to 589 s, LMS H10/W8
    up to 383 s). LMS came out faster than in the line's own runs
    (LMS-SHA256 H10/W4 key generation 3.38 s to 2.81 s).
    Checking every claim in the documents against the final result files
    turned up statements that were no longer true:
    - HQC was quoted as 40 to 66 times faster; with the assembly Keccak
      linked it is 51 to 82 times.
    - SPHINCS-alpha 128s was described as host-only, but the rerun of entry
      15 had run it on the board with all fingerprints equal.
    - `pqmagic_sphincsa_192f` was the one suite that did not run. Its table,
      the two rows used to build it and a 20 KB job stack did not fit the
      heap together. With a 10 KB stack (7.3 KB used) and the unused SM3
      backend left out it runs: 16 jobs pass, 12 fingerprints equal the host.
    - GmSSL's SPHINCS+ key generation had no reference-variant measurement;
      it has one now, with the same public-key fingerprint as the host.
    State after this entry: 34 suites, 1306 jobs, 447 known-answer and
    self-test checks, 1004 fingerprints; no failed job, no failed check, and
    every fingerprint equals the host build. The NGCC generic port is counted
    separately: its per-candidate outcomes are in `docs/ngcc/status.md`.
