# Algorithms covered

Status as of November 2026. "Verified" means: runs on the ESP32-C3 from RAM,
the known-answer or self-tests pass on the chip, and every key, ciphertext,
signature and shared secret it produces has the same fingerprint as an
independent build of the reference code on the build machine. Times and
memory are in [SUMMARY.md](SUMMARY.md) and [RESULTS.md](RESULTS.md).

## NIST standards and selections

| Algorithm | Status at NIST | Parameter sets | Code used (license) | Suite | On the ESP32-C3 |
|---|---|---|---|---|---|
| ML-KEM | FIPS 203, final (2024) | 512, 768, 1024 | mlkem-native (Apache-2.0 OR ISC OR MIT); RV32IM arithmetic from PQRV (MIT) | `mlkem` | Verified, all three |
| ML-DSA | FIPS 204, final (2024) | 44, 65, 87 | mldsa-native (Apache-2.0 OR ISC OR MIT); RV32IM arithmetic from PQRV (MIT) | `mldsa`, `mldsa87`, `mldsa_lowram` | Verified, all three, plus the low-RAM build |
| SLH-DSA | FIPS 205, final (2024) | all 12 (SHA2 and SHAKE; 128, 192, 256; s and f) | slhdsa-c (Apache-2.0 OR ISC OR MIT) | `slhdsa` | Verified, all 12 |
| FN-DSA (Falcon) | FIPS 206 not final yet | 512, 1024 | c-fn-dsa by Thomas Pornin (Unlicense) | `fndsa` | Verified, both; floating point is emulated |
| HQC | Selected in 2025; standard not final yet | HQC-1, HQC-3, HQC-5 | HQC reference v5.1.0 (public domain) | `hqc`, `hqc3ref` | Optimized variant verified for all three. Reference variant verified for HQC-1 and HQC-3; the HQC-5 reference needs about 211 KB of stack and is verified on the host only |
| LMS, HSS | SP 800-208 (RFC 8554) | SHA-256 and SHA-256/192 with heights 5 and 10, Winternitz 1 to 8; SHAKE256; HSS with two levels | Written for this project | `lms` | Verified; RFC 8554 and RFC 9858 test cases pass |
| XMSS, XMSS^MT | SP 800-208 (RFC 8391) | XMSS-SHA2_10 (256, 192), XMSS-SHAKE256_10_256, XMSS^MT 20/4 with SHA2 and SHAKE256 | xmss-reference (CC0) | `xmss` | Verified |

Not attempted: Classic McEliece, BIKE, FrodoKEM and NTRU (not NIST
standards), and the candidates of NIST's additional signature round.

## Chinese algorithms and SM3 instantiations

| Algorithm | What it is | Parameter sets | Code used (license) | Suite | On the ESP32-C3 |
|---|---|---|---|---|---|
| Aigis-enc | Lattice KEM (PKC 2020) | 1 to 4, with SM3 and with SHAKE | PQMagic (Apache-2.0) | `aigis_enc` | Verified, all |
| Aigis-sig | Lattice signature (PKC 2020) | 1 to 3, with SM3 and with SHAKE | PQMagic | `aigis_sig`, `aigis_sig_lowram` | Verified, all; the low-stack build gives the same outputs |
| ML-KEM with SM3 | FIPS 203 with SM3 in place of SHAKE/SHA-3; not a standard | 512, 768, 1024 | PQMagic | `pqmagic_mlkem` | Verified; the SHAKE build of the same code passes the FIPS 203 vectors |
| ML-DSA with SM3 | FIPS 204 with SM3; not a standard | 44, 65, 87 | PQMagic | `pqmagic_mldsa`, `pqmagic_mldsa87` | Verified; the SHAKE build passes the FIPS 204 vectors |
| SLH-DSA with SM3 | SPHINCS+ with SM3; not a standard | 128f, 128s, simple and robust | PQMagic | `pqmagic_slhdsa_sm3` | Verified |
| SPHINCS-alpha | Hash-based signature (CRYPTO 2023) | 128f and 128s with SM3, SHA-2, SHAKE; 192f with SHA-2, SHAKE | PQMagic | `pqmagic_sphincsa`, `pqmagic_sphincsa_192f` | Verified. The scheme's encoding table is stored in a compact form written for this project (66 KB to 146 KB of heap). 192s and 256 need 332 KB to 769 KB even so and do not fit |
| Kyber-768 | GmSSL's Kyber | 768 | GmSSL (Apache-2.0) | `gmssl_kyber` | Verified |
| LMS-SM3, HSS-SM3 | LMS with SM3 | height 5; HSS 5/5 | GmSSL | `gmssl_lms` | Verified. Height 10 needs one 65 KB allocation and was not run |
| XMSS^MT-SM3 | XMSS^MT with SM3 | 20/4 | GmSSL | `gmssl_xmss` | Verified. XMSS-SM3 with height 10 fails to allocate 65 KB |
| SPHINCS+-SM3 | SPHINCS+ with SM3 | 128s | GmSSL | `gmssl_sphincs` | Key generation only: signing keeps 131 KB of trees in local variables |
| LAC | Lattice KEM, NIST round-2 candidate (v3b) | Light, 128, 192, 256 | LAC reference, fetched (no license) | `lac` | Verified against the host; upstream ships no vectors |

## NGCC round 1 (China's call for next-generation commercial cryptography)

The Institute of Commercial Cryptography Standards published the first-round
candidates in September 2026: 84 public-key submissions (41 KEMs, 34
signatures, 9 key exchanges). They are candidates, not standards, and public
analysis has already reported breaks of several. The submission packages are
fetched from the official site and are not part of this repository.

From open implementations, with per-algorithm work:

| Candidate | Parameter sets | Code used | Suite | On the ESP32-C3 |
|---|---|---|---|---|
| Aigis-Enc+ | 512, 1024, 2048 | PQMagic (Apache-2.0) | `pqmagic_aigis_enc_plus` | Verified. The reference decapsulation outputs the shared secret before its re-encryption check |
| Aigis-Sig+ | I, II, III | PQMagic | `pqmagic_aigis_sig_plus` | Set I verified; II and III run on the host only (signing needs more than 112 KB of stack) |
| NEV | 12 sets, SHAKE and SM3 configurations | PQMagic | `pqmagic_nev`, `pqmagic_nev_sm3` | Verified, all |
| NEV-AKE | 9 sets | PQMagic | `pqmagic_nev_ake` | Verified, all |
| SCloud+ | 128, 192, 256 with SM3, SHAKE, AES | SCloud+ repository, fetched (no license) | `scloudplus`, `scloudplus_256`, `scloudplus_256_sm3` | Verified; the submission's own vectors pass. 384 and 512 need about 150 KB of stack plus 107 KB of key buffers and do not fit |
| ReSolveD-alpha | | PQMagic | none | Not feasible: 0.4 MB to over 1 MB of heap |
| QIMEN-PIKE | | PQMagic | none | Skipped: needs the GMP big-number library |

The other 77, from the official packages through one generic suite
(`suites/ngcc`), without per-candidate optimization:

| Outcome | KEM | Signature | Key exchange | Total |
|---|---:|---:|---:|---:|
| Runs on the ESP32-C3, fingerprints equal the host | 22 | 9 | 5 | 36 |
| Runs on the host only | 7 | 14 | 1 | 22 |
| Does not build on the host | 8 | 7 | 1 | 16 |
| Builds but fails its self-test | 0 | 2 | 1 | 3 |

Names, times, memory and the reason for each candidate that did not reach the
chip are in [ngcc/status.md](ngcc/status.md); the inventory of all packages is
in [ngcc/inventory.md](ngcc/inventory.md). Typical reasons: 128-bit integer
types that RV32 does not have, images or keys larger than the RAM, x86
intrinsics, GMP or OpenSSL dependencies, and errors that appear only on the
32-bit target.
