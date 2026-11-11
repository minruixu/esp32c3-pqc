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
