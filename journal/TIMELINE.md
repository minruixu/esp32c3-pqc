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
