# ML-KEM-512/768/1024 (FIPS 203)

## Sources

- `third_party/mlkem-native`: mlkem-native, commit `a446d5a` (2026-11-09),
  Apache-2.0 OR ISC OR MIT, unchanged. See its `SOURCE.md`.
- Known-answer vectors: `pqc_suite/kat_vectors.h` is
  `examples/monolithic_build_multilevel/expected_test_vectors_multilevel.h`
  from the same commit.
- Everything else in `pqc_suite/` was written for this project.

## The two variants

Both are built from the same mlkem-native sources, all three parameter sets
in one compilation unit, under different symbol prefixes.

- **ref** (`mlkem_ref.c`): mlkem-native as shipped. Portable C, its own
  Keccak, the project's default compiler options.
- **opt** (`mlkem_opt.c`): the configuration hooks of mlkem-native are used
  to plug in code for the ESP32-C3. No upstream file is changed.
  - `MLK_CONFIG_FIPS202_CUSTOM_HEADER` → `c3_fips202_mlkem.h` and
    `c3_sponge.c`: a sponge that xors input into the state a word at a time
    and copies output with `memcpy`, on the Keccak-f[1600] of
    `components/prims` (assembly on the device).
  - `MLK_CONFIG_SERIAL_FIPS202_ONLY`: no four-way batching of Keccak.
  - `MLK_CONFIG_ARITH_BACKEND_FILE` → `c3_arith_backend.h` and
    `mlkem_arith_rv32imc.S`: NTT, inverse NTT, base multiplication,
    reduction, Montgomery conversion, multiplication cache, rejection
    sampling, and 12-bit packing and unpacking in assembly. Device only.
  - `mlkem_opt.c` and `c3_sponge.c` are compiled with `-O3`.

On the build machine the opt variant uses the C arithmetic and the reference
permutation, so the host run exercises the same configuration paths and the
device is compared against it.

## Results

ESP32-C3 at 160 MHz, image loaded into RAM, average of 10 runs. Both variants
pass the known-answer tests of mlkem-native, reject a corrupted ciphertext,
and produce the same keys, ciphertexts and shared secrets as the host build
(`tools/pqc run mlkem --check`: 38 fingerprints equal).

| Operation | ref ms | opt ms | Speedup | ref stack (bytes) | opt stack (bytes) |
|---|---:|---:|---:|---:|---:|
| ML-KEM-512 key generation | 4.91 | 2.79 | 1.76 | 9,776 | 7,792 |
| ML-KEM-512 encapsulation | 5.70 | 2.94 | 1.94 | 12,416 | 10,448 |
| ML-KEM-512 decapsulation | 7.13 | 3.69 | 1.94 | 13,152 | 11,184 |
| ML-KEM-768 key generation | 8.21 | 4.46 | 1.84 | 14,144 | 11,904 |
| ML-KEM-768 encapsulation | 9.03 | 4.83 | 1.87 | 17,264 | 14,928 |
| ML-KEM-768 decapsulation | 11.03 | 5.87 | 1.88 | 18,320 | 15,984 |
| ML-KEM-1024 key generation | 12.37 | 7.00 | 1.77 | 19,520 | 17,280 |
| ML-KEM-1024 encapsulation | 13.51 | 7.41 | 1.82 | 23,168 | 20,816 |
| ML-KEM-1024 decapsulation | 16.10 | 8.77 | 1.84 | 24,704 | 22,480 |

Internal routines, cycles per call (`mlkem-prim/*`, 100 calls, each including
a 512-byte copy of the input where the routine works in place):

| Routine | ref (C) | opt (assembly) | Speedup | Predicted by the generator |
|---|---:|---:|---:|---:|
| NTT | 20,650 | 8,350 | 2.47 | 7,824 |
| Inverse NTT | 32,900 | 8,690 | 3.78 | 8,181 |
| Base multiplication, k = 3 | 12,200 | 6,890 | 1.77 | 6,746 |
| Multiplication cache | 1,920 | 1,220 | 1.58 | 1,145 |
| Reduction to [0, q) | 6,900 | 3,200 | 2.15 | 2,711 |
| Conversion to Montgomery form | 4,080 | 2,440 | 1.67 | 1,943 |
| Public matrix, k = 3 (9 polynomials) | 646,000 | 382,000 | 1.69 | |

Code size as linked (all three parameter sets): ref 23.9 KB; opt 18.9 KB of C
plus 7.9 KB of assembly and tables, 4.0 KB of Keccak and 0.8 KB of sponge,
31.6 KB in total. With `-O2` the C part of opt is 14.0 KB.

## What was done, step by step

ML-KEM-768, milliseconds for key generation / encapsulation / decapsulation:

| Step | Keygen | Encaps | Decaps |
|---|---:|---:|---:|
| ref | 8.23 | 9.02 | 11.05 |
| Assembly Keccak-f[1600] through the FIPS 202 backend hook, and assembly NTT, inverse NTT, base multiplication, reduction, Montgomery conversion, multiplication cache | 4.96 | 5.38 | 6.51 |
| Own sponge (word-wise absorb, `memcpy` squeeze), serial FIPS 202, assembly rejection sampling | 4.59 | 5.10 | 6.18 |
| `-O3` for the C code of opt | 4.52 | 4.89 | 5.91 |
| Assembly 12-bit packing and unpacking | 4.46 | 4.83 | 5.87 |

Where the first step comes from: the Keccak permutation falls from 21,540 to
12,631 cycles (`suites/keccak/NOTES.md`), and an ML-KEM-768 key generation
runs 45 of them, an encapsulation 45, a decapsulation 54. That is two thirds
to three quarters of the reference running time.

### Arithmetic

`gen/mlkem_arith_gen.py` generates `mlkem_arith_rv32imc.S`. Every routine is
executed in the interpreter of `gen/rvgen.py` and compared with a Python
model before it is printed; the interpreter also predicts the cycle count
from the measured instruction costs of the core.

- On this core `mul` takes 1 cycle and `mulh` takes 5. All modular
  multiplications by constants are Plantard multiplications written with
  `mul` and `srai` only (`mul, srai, addi, mul, srai`: 5 cycles). The usual
  RV32IM form with `mulh` would take 8, a Montgomery multiplication 8 or more.
  A butterfly is 7 instructions.
- Plantard results are below q/2 in absolute value, so the forward NTT needs
  no reduction between layers. Layers 1 to 4 are merged with 16 coefficients
  in registers, then layers 5 to 7; every coefficient is loaded and stored
  twice.
- The inverse NTT merges layers 7 to 5 and 4 to 1. The final scaling by
  2^16/128 is folded into the twiddle factors: each value is either "scaled"
  or "not yet scaled", a butterfly on two unscaled values uses the scaled
  twiddle, and only the one path that never meets a twiddle needs an extra
  multiplication. That is 48 extra multiplications instead of 128.
- The base multiplication accumulates the 32-bit products of all k vector
  entries and reduces once per coefficient with a Montgomery reduction that
  needs no `mulh` (`mul` by q^-1·2^16, `srai`, `mul`, `sub`, `srai`).
- Rejection sampling for the public matrix: 12 bytes are read as three words
  and give 8 candidates; each is stored unconditionally and the output
  pointer advances only if it is below q. The data is public.
- Packing and unpacking move three 32-bit words per 8 coefficients.
- `gp` and `tp` are not used, so the routines are safe with interrupts
  enabled. The routines that handle secrets have no data-dependent branches
  or addresses, and `mul` takes one cycle for all operands tested
  (`suites/keccak`, `cpu/model`).

### FIPS 202 glue

mlkem-native moves every byte into and out of the Keccak state in a
byte-wise loop, which the compiler leaves as 7 to 9 cycles per byte.
`c3_sponge.c` takes about one cycle per byte. With four-way batching on a
single-issue core, a batch that is not full runs unused permutations
(ML-KEM-768 key generation: 45 permutations batched, 43 serial).

### Compiler options

Measured on the opt variant, same source state (`-D PQC_COPT=...`):

| Option for the suite | ML-KEM-768 keygen / encaps / decaps | Image |
|---|---|---:|
| project default (`-O2`) | 4.59 / 5.10 / 6.18 ms | 173.5 KB |
| `-O3` | 4.52 / 4.89 / 5.91 ms | 191.5 KB |
| `-Os` | 4.60 / 5.12 / 6.20 ms | 165.4 KB |

`-O3` is applied to the C files of opt only.

## Where the time goes now

Sampling profile of the opt variant on the device (`-D PQC_PROFILE=1`, tick
interrupt, about 1,200 samples per operation, taken before the packing
routines moved to assembly), ML-KEM-768:

| | Keygen | Encaps | Decaps |
|---|---:|---:|---:|
| Keccak-f[1600] | 74.5 % | 67.2 % | 67.3 % |
| NTT and inverse NTT | 6.5 % | 7.5 % | 9.1 % |
| Base multiplication and cache | 3.4 % | 3.7 % | 4.1 % |
| Rejection sampling | 3.5 % | 3.0 % | 2.9 % |
| Reduction | 2.1 % | 2.6 % | 1.4 % |
| Noise sampling (C) | 1.7 % | 2.8 % | 2.5 % |
| Compression and decompression (C) | | 2.6 % | 3.8 % |
| Packing and unpacking (C at that point) | 2.6 % | 1.1 % | below 0.5 % |
| `memset` (wiping) | 1.4 % | 2.1 % | 1.6 % |
| Sponge absorb | 1.0 % | 1.9 % | 1.8 % |

## Not done

- **Stack reduction.** mlkem-native has no reduced-memory configuration; the
  matrix and the vectors are kept in full. Streaming the matrix would need
  changes to `indcpa.c`, which is outside "upstream code plus backends". The
  opt variant does use about 2 KB less stack, because serial FIPS 202 needs
  one Keccak state instead of four.
- **Noise sampling and compression in assembly.** Together about 5 % of an
  encapsulation. mlkem-native has hooks for compression but none for the
  centered binomial sampler.
- **A flash-resident measurement.** All numbers are from RAM-loaded images.
