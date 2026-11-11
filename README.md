# Post-quantum cryptography on an ESP32-C3

[中文说明](README.zh-CN.md)

The post-quantum algorithms that NIST has standardized or selected, and the
Chinese ones that have a public implementation, ported to an ESP32-C3
(RV32IMC, 160 MHz, about 385 KB of RAM, no FPU), checked against known
answers, measured, and then made faster for this particular chip. The ones
that do not fit the chip are listed with the reason.

- **NIST:** ML-KEM, ML-DSA, SLH-DSA (all 12 parameter sets), FN-DSA (Falcon),
  HQC, and the stateful hash-based signatures LMS/HSS and XMSS/XMSS^MT.
- **China:** Aigis-enc and Aigis-sig, the SM3 instantiations of ML-KEM,
  ML-DSA and SLH-DSA, SPHINCS-alpha, GmSSL's SM3-based hash signatures, LAC,
  and the candidates of the 2026 NGCC first round: SCloud+, NEV, Aigis-Enc+,
  Aigis-Sig+ with their own suites, and 36 more through a generic port.

What exactly is covered, and what did not fit: [docs/ALGORITHMS.md](docs/ALGORITHMS.md).

## Results

`reference → optimized (speedup)`, measured on the chip. One representative
parameter set per family; every set is in [docs/SUMMARY.md](docs/SUMMARY.md),
and every measured job in [docs/RESULTS.md](docs/RESULTS.md).

<!-- results:begin -->
| Algorithm | Key generation | Encapsulation / signing | Decapsulation / verification | Stack + heap |
|---|---:|---:|---:|---:|
| ML-KEM-768 | 8.21 ms → 4.48 ms (1.8×) | 9.03 ms → 4.85 ms (1.9×) | 11.0 ms → 5.90 ms (1.9×) | 15.7 KB |
| ML-DSA-65 | 33.1 ms → 18.6 ms (1.8×) | 94.4 ms → 45.2 ms (2.1×) | 33.4 ms → 18.2 ms (1.8×) | 68.5 KB |
| SLH-DSA-SHA2-128f | 130 ms → 5.64 ms (23.0×) | 3046 ms → 143 ms (21.3×) | 176 ms → 9.79 ms (18.0×) | 4.3 KB |
| SLH-DSA-SHA2-128s | 8298 ms → 360 ms (23.0×) | 63.5 s → 2890 ms (22.0×) | 60.6 ms → 3.57 ms (17.0×) | 4.3 KB |
| FN-DSA-512 (Falcon) | 522 ms | 289 ms → 219 ms (1.3×) | 4.10 ms → 3.21 ms (1.3×) | 2.2 KB |
| HQC-1 | 281 ms → 4.29 ms (65.4×) | 561 ms → 9.11 ms (61.5×) | 1132 ms → 22.3 ms (50.8×) | 19.8 KB |
| LMS-SHA256 H10/W4 | 44.0 s → 2807 ms (15.7×) | 45.5 ms → 3.18 ms (14.3×) | 20.6 ms → 1.59 ms (13.0×) | 2.4 KB |
| XMSS-SHA2_10_256 | 235.9 s → 13.0 s (18.1×) | 511 ms → 25.7 ms (19.9×) | 130 ms → 7.10 ms (18.2×) | 4.1 KB |
| Aigis-enc-2 (SM3) | 9.79 ms → 7.38 ms (1.3×) | 12.8 ms → 9.65 ms (1.3×) | 13.3 ms → 10.4 ms (1.3×) | 15.3 KB |
| Aigis-sig-2 (SM3) | 37.3 ms → 25.4 ms (1.5×) | 66.8 ms → 51.0 ms (1.3×) | 37.2 ms → 26.1 ms (1.4×) | 85.6 KB |
| ML-KEM-768 with SM3 | 10.3 ms → 7.42 ms (1.4×) | 11.2 ms → 8.26 ms (1.4×) | 12.3 ms → 9.39 ms (1.3×) | 15.9 KB |
| ML-DSA-65 with SM3 | 53.0 ms → 36.9 ms (1.4×) | 88.1 ms → 68.7 ms (1.3×) | 48.6 ms → 34.6 ms (1.4×) | 79.9 KB |
| LAC-128 | 8.78 ms → 4.40 ms (2.0×) | 13.7 ms → 7.64 ms (1.8×) | 17.0 ms → 10.9 ms (1.6×) | 13.5 KB |
| SCloud+-128 (AES) | 1380 ms → 170 ms (8.1×) | 1372 ms → 148 ms (9.3×) | 1364 ms → 145 ms (9.4×) | 32.2 KB |
| NEV-512 | 2.87 ms → 2.43 ms (1.2×) | 2.41 ms → 2.02 ms (1.2×) | 3.04 ms → 2.87 ms (1.1×) | 6.5 KB |
| Aigis-Enc+-512 | 4.68 ms → 3.45 ms (1.4×) | 6.45 ms → 4.94 ms (1.3×) | 7.72 ms → 6.11 ms (1.3×) | 12.7 KB |
<!-- results:end -->

The reference column is the upstream portable C code. The optimized column
adds what this project wrote for the ESP32-C3:

- hash chains and tree hashing on the **SHA-256 hardware accelerator**, with
  the state kept in the peripheral (one hash in about 200 to 280 cycles
  instead of 4400);
- **Keccak-f[1600] in RV32IMC assembly** (79 µs instead of 135 µs per
  permutation) and assembly polynomial arithmetic for ML-KEM and ML-DSA;
- a **sparse polynomial product for HQC** in place of dense Karatsuba;
- **32-bit emulated floating point** for FN-DSA signing;
- the **AES accelerator** for SCloud+ and LAC, assembly SHA-512, a faster
  SM3, and low-memory builds of ML-DSA and Aigis-sig.

How each of these was found, what was tried and rejected, and what the chip
cannot do: [docs/FINDINGS.md](docs/FINDINGS.md).

## How it is checked

Every suite runs the known-answer tests its upstream ships, on the chip. On
top of that, every key, ciphertext, signature and shared secret is
fingerprinted on the chip and compared with a second build of the reference
code on the build machine (`tools/pqc check`). The optimized variant must
produce the same bytes as the reference; a wrong optimization shows up as a
mismatch, not as a faster number.

## Running it

You need ESP-IDF v5.5 and an ESP32-C3 on USB. Nothing here writes to the
board's flash: each suite is built as a RAM-only image and loaded with
`esptool load_ram`. After a reset the board runs its own firmware again.

```
export IDF_PATH=~/esp/esp-idf            # wherever ESP-IDF lives
tools/pqc run mlkem --check              # build, load into RAM, run, compare with the host
tools/pqc run slhdsa -f slhdsa-sha2-128f # only the jobs whose name contains the text
python3 tools/report.py                  # regenerate docs/RESULTS.md and docs/SUMMARY.md
```

Suites are the directories under `suites/`. SCloud+, LAC and the NGCC
packages have no redistribution license, so their code is fetched first:
`tools/fetch/scloudplus.sh`, `tools/fetch/lac.sh`, and `docs/ngcc/README.md`
for the NGCC candidates. To add an algorithm, see
[docs/PORTING.md](docs/PORTING.md).

## Before you use any of it

This is a measurement project, not a hardened library. In particular:
constant-time behaviour is not guaranteed by the C source (the compiler was
seen turning masked selects into branches), nothing is protected against
power analysis, the fast HQC product is only safe on a core without a data
cache, the benchmarks use a seeded random generator, and everything was
measured running from RAM. The SM3 instantiations and the NGCC candidates are
not standards, and several NGCC candidates have published attacks. The full
list is at the end of [docs/FINDINGS.md](docs/FINDINGS.md).

## Layout

```
suites/<name>/        one algorithm family = one RAM image; NOTES.md explains each
components/prims/     Keccak, SHA-2, SM3 with a reference/optimized switch
components/bench/     harness: time, stack and heap per operation, fingerprints
third_party/          upstream code whose license allows redistribution
tools/                pqc (build, load, run, compare), report.py, fetch scripts
results/              the measurements, one JSON file per suite
docs/                 SUMMARY, RESULTS, ALGORITHMS, FINDINGS, PORTING, ngcc/
```

The branch [`optimization-journal`](../../tree/optimization-journal) is the
working record: every optimization attempt with its numbers and verdict, the
reports of each line of work, a timeline, and the intermediate commits.

## Licenses

Code under `third_party/` keeps its own license; each directory has the
license text and a `SOURCE.md` with the origin and local changes. The NIST
ACVP sample vectors used by the SLH-DSA suite carry NIST's notice next to
them. No license has been chosen yet for the code written in this project.
