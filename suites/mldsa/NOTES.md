# ML-DSA-44/65/87 (FIPS 204)

Three images are built from the sources in `pqc_suite/`:

| Suite | Contents | Why separate |
|---|---|---|
| `mldsa` | ML-DSA-44 and ML-DSA-65, variants ref and opt, and the transforms on their own | |
| `mldsa87` | ML-DSA-87, variants ref and opt | memory, see below |
| `mldsa_lowram` | all three levels, variant lowram | a third build of the library does not fit next to the other two |

## Sources

- `third_party/mldsa-native`: mldsa-native, commit `dd79120` (2026-11-05),
  Apache-2.0 OR ISC OR MIT, unchanged. See its `SOURCE.md`.
- Known-answer vectors: `pqc_suite/kat_vectors.h` is
  `examples/monolithic_build_multilevel/expected_test_vectors_multilevel.h`
  from the same commit.
- Everything else in `pqc_suite/` was written for this project.

## The variants

- **ref** (`mldsa_ref.c`): mldsa-native as shipped, portable C, its own
  Keccak, the project's default compiler options.
- **opt** (`mldsa_opt.c`, `mldsa_opt_config.h`): the same sources with
  ESP32-C3 code plugged into mldsa-native's configuration hooks. No upstream
  file is changed.
  - `MLD_CONFIG_FIPS202_CUSTOM_HEADER` → `c3_fips202_mldsa.h`: the
    word-oriented sponge of `suites/mlkem/pqc_suite/c3_sponge.c` on the
    Keccak-f[1600] of `components/prims` (assembly on the device).
  - `MLD_CONFIG_SERIAL_FIPS202_ONLY`: no four-way batching of Keccak.
  - `MLD_CONFIG_ARITH_BACKEND_FILE` → `c3_mldsa_backend.h` and
    `mldsa_c3_rv32imc.S`: NTT, inverse NTT, pointwise multiplication,
    accumulating pointwise multiplication, norm check and rejection sampling
    in assembly. Device only.
- **lowram**: opt plus `MLD_CONFIG_REDUCE_RAM`.

On the build machine opt and lowram use the C arithmetic and the reference
permutation; the device is compared against that run.

## Results

ESP32-C3 at 160 MHz, images loaded into RAM. Key generation and verification
are averages of 5 runs, signing of 20 signatures with fresh randomness. The
random stream is the same for every variant, so the same 20 signatures (with
the same numbers of rejected attempts) are timed in each. All variants pass
the known-answer tests of mldsa-native, reject a corrupted signature, and
produce the same keys and signatures as the host build
(`tools/pqc run <suite> --check`).

| Operation | ref ms | opt ms | Speedup | lowram ms | ref memory | opt memory | lowram stack |
|---|---:|---:|---:|---:|---:|---:|---:|
| ML-DSA-44 key generation | 19.24 | 10.54 | 1.83 | 10.84 | 27.6 KB stack | 24.4 KB stack | 9.2 KB |
| ML-DSA-44 signature (average of 20) | 66.97 | 32.16 | 2.08 | 66.62 | 48.8 KB stack | 45.5 KB stack | 11.5 KB |
| ML-DSA-44 verification | 20.00 | 10.73 | 1.86 | 11.02 | 29.5 KB stack | 26.5 KB stack | 11.3 KB |
| ML-DSA-65 key generation | 33.11 | 18.63 | 1.78 | 19.16 | 42.6 KB stack | 39.4 KB stack | 10.2 KB |
| ML-DSA-65 signature (average of 20) | 94.43 | 45.23 | 2.09 | 102.54 | 71.8 KB stack | 68.5 KB stack | 13.5 KB |
| ML-DSA-65 verification | 33.36 | 18.15 | 1.84 | 18.70 | 44.5 KB stack | 41.5 KB stack | 12.4 KB |
| ML-DSA-87 key generation | 56.00 | 31.57 | 1.77 | 32.52 | 5.3 KB stack + 67.8 KB heap | 2.1 KB stack + 67.8 KB heap | 12.2 KB |
| ML-DSA-87 signature (average of 20) | 128.31 | 62.29 | 2.06 | 143.42 | 5.3 KB stack + 109.7 KB heap | 2.1 KB stack + 109.7 KB heap | 15.8 KB |
| ML-DSA-87 verification | 56.33 | 31.32 | 1.80 | 32.27 | 5.7 KB stack + 69.8 KB heap | 2.7 KB stack + 69.8 KB heap | 14.6 KB |

Signing times vary with the number of rejected attempts. Shortest and longest of the 20 signatures:

| | ref ms | opt ms | lowram ms |
|---|---:|---:|---:|
| ML-DSA-44 | 29.2 to 164.2 | 14.3 to 77.7 | 16.1 to 195.1 |
| ML-DSA-65 | 45.8 to 226.8 | 22.8 to 106.1 | 25.2 to 310.0 |
| ML-DSA-87 | 74.2 to 274.8 | 37.7 to 128.2 | 41.3 to 412.2 |

Memory: ref and opt keep their large temporaries on the stack (ML-DSA-87: on
the heap, see below). The lowram variant needs 9.2 to 15.8 KB of stack and no
heap. Its key generation and verification are about as fast as opt. Its
signing is slower, because the public matrix is regenerated for every attempt
(ML-DSA-65: 927 Keccak permutations per average signature instead of 302),
but stays within 12 % of the reference code, which needs about five times
the memory.

The transforms on their own, cycles per call (`mldsa-prim/*`, 100 calls, each
including a 1 KB copy of the input):

| Routine | C (`ref`) | mldsa-native RV32IM assembly (`up`) | This project (`opt`) | Predicted |
|---|---:|---:|---:|---:|
| NTT | 46,420 | 14,910 | 13,320 | 12,400 |
| Inverse NTT | 54,340 | 19,280 | 15,140 | 14,223 |
| Pointwise multiplication | 10,140 | 6,560 | 5,410 | 4,427 |

Code size as linked, ML-DSA-44 and ML-DSA-65 together: ref 32.5 KB; opt
19.2 KB of C plus 9.8 KB of assembly and tables, 4.0 KB of Keccak and 0.8 KB
of sponge, 33.8 KB in total.

## What was done, step by step

ML-DSA-65, milliseconds for key generation / average signature / verification:

| Step | Keygen | Sign | Verify |
|---|---:|---:|---:|
| ref | 33.17 | 95.32 | 33.41 |
| Assembly Keccak-f[1600] through the FIPS 202 backend hook, and mldsa-native's own RV32IM backend (NTT, inverse NTT, pointwise multiplication) | 20.19 | 52.99 | 19.80 |
| Own sponge, serial FIPS 202, assembly rejection sampling, own NTT and inverse NTT | 19.14 | 49.86 | 18.85 |
| Own pointwise and accumulating pointwise multiplication, norm check | 18.63 | 45.23 | 18.15 |

An ML-DSA-65 key generation runs 188 Keccak permutations, an average
signature 302, a verification 174; the permutation falls from 21,540 to
12,631 cycles (`suites/keccak/NOTES.md`).

### Arithmetic

`gen/mldsa_arith_gen.py` generates `mldsa_c3_rv32imc.S`. Every routine is
executed in the interpreter of `suites/mlkem/pqc_suite/gen/rvgen.py` and
compared with a Python model before it is printed. `gen/sim_upstream.py`
runs mldsa-native's own RV32IM assembly in the same interpreter; the
semantics of the transforms (ordering, the factor 2^32/256 of the inverse)
were taken from there.

- On this core `mul` takes 1 cycle and `mulh` takes 5. The modulus has 23
  bits, so a product does not fit in 32 bits and one `mulh` per modular
  multiplication cannot be avoided. Multiplication by a constant is a Barrett
  multiplication with a precomputed quotient (`mulh, mul, mul, sub`: 8
  cycles), the same arithmetic as mldsa-native's RV32IM backend, instead of a
  Montgomery multiplication with two `mulh` (12 or more).
  A variant without any `mulh` (approximate quotient from the top bits, then
  a second correction step) was worked out and comes to 9 single-cycle
  instructions, so it was not used.
- NTT and inverse NTT merge four layers instead of two: 16 coefficients in
  registers, every coefficient loaded and stored twice per transform instead
  of four times.
- The inverse NTT has no separate scaling pass. In the last layer every
  output is one multiplication (by 2^32/256 or by a twiddle factor times
  that), with a rounded quotient so that the result is below q in absolute
  value as the interface requires. Sums are left to grow in between: at most
  256·(q−1), which is below 2^31.
- Pointwise multiplication is the Montgomery multiplication of
  mldsa-native's routine, four coefficients per loop iteration with the
  loads scheduled.
- Accumulating pointwise multiplication (the matrix-vector product): the C
  code accumulates 64-bit products and reduces once per coefficient, and the
  compiler turns the reduction into a 64-bit shift-and-add chain. The
  assembly does the same accumulation and reduces with `mul, mulh, sub`.
  About 70 instead of about 125 cycles per coefficient for ML-DSA-65.
- Norm check: 6 instructions per coefficient, constant time.
- Rejection sampling for the public matrix: 12 bytes are read as three words
  and give 4 candidates, stored unconditionally; the output pointer advances
  only for values below q. The data is public.
- `gp` and `tp` are not used. The routines that handle secrets have no
  data-dependent branches or addresses; `mul` and `mulh` take the same time
  for all operands tested (`suites/keccak`, `cpu/model`).

`-D PQC_MLDSA_UPSTREAM_NTT=1` builds opt with mldsa-native's own RV32IM
routines instead, for comparison.

### Compiler options

`-O3` instead of the project default `-O2` for the whole suite
(`-D PQC_COPT=-O3`) changes the six opt timings of ML-DSA-44 and ML-DSA-65 by
between -0.2 % and +0.5 % and adds 14 KB to the image. It is not used.

### Memory of ML-DSA-87

A reference ML-DSA-87 signature keeps about 110 KB of temporaries on the
stack, plus the stack frames themselves. The heap of the ESP32-C3 consists of three separate blocks of at most
128 KB, and a RAM-loaded image occupies the first and part of the second, so
the largest piece that can be allocated is the third block, about 116 KB
before the heap's own bookkeeping: a task with 124 KB or 118 KB of stack
could not be created. The `mldsa87`
image therefore sets `MLD_CONFIG_CUSTOM_ALLOC_FREE` for both variants
(`mldsa_heap_alloc.h`), which moves the large temporaries to the heap in
several pieces. The table reports stack and heap; their sum is what the
operation needs.

## Where the time goes now

Sampling profile of opt on the device (`-D PQC_PROFILE=1`), ML-DSA-65:

| | Keygen | Sign | Verify |
|---|---:|---:|---:|
| Keccak-f[1600] | 79.6 % | 52.2 % | 75.8 % |
| Inverse NTT | 3.0 % | 13.7 % | 2.8 % |
| NTT | 2.2 % | 7.6 % | 5.1 % |
| Accumulating pointwise multiplication | 3.4 % | 6.7 % | 3.8 % |
| Pointwise multiplication | | 2.5 % | 0.9 % |
| Rest of the signing or verification function (C: additions, reductions, copies) | 2.3 % | 5.8 % | 2.8 % |
| Rejection sampling | 2.4 % | 0.8 % | 2.6 % |
| `memset` (wiping) | 1.9 % | 2.0 % | 1.6 % |
| Decomposition, hints, unpacking of z (C) | | 3.2 % | 2.7 % |
| Norm check | | 1.1 % | |

Before the accumulating multiplication and the norm check moved to assembly,
the C part of the signing function took 20.4 % of a signature and Keccak
48.8 %.

## Not done

- Decomposition, hint handling and the unpacking of z have native hooks in
  mldsa-native and are still C: about 3 % of a signature.
- The plain polynomial additions, subtractions and reductions inside the
  signing function are C loops of one coefficient per iteration; mldsa-native
  has no hooks for them.
- A flash-resident measurement. All numbers are from RAM-loaded images.
