# FN-DSA (Falcon) on the ESP32-C3

FN-DSA-512 and FN-DSA-1024: key generation, signing, verification.
Status of the standard: FIPS 206 was still a draft when this was written.

## Source

`third_party/c-fn-dsa`: Thomas Pornin's c-fn-dsa, commit `1b858880`
(2026-09-21), public domain (Unlicense). See `third_party/c-fn-dsa/SOURCE.md`.

Why this implementation and not PQClean's `falcon-512` / `falcon-1024`
(also in reach, MIT license, same author's 2021 code):

- It follows the author's best estimate of the FIPS 206 draft (changed on
  2026-07-22: little-endian encodings, NTT-format public keys, the ML-DSA
  style message representative, 40-byte seeds). PQClean has the round-3
  Falcon submission, whose keys and signatures are not FN-DSA.
- It was written with small 32-bit targets in mind: every function has a
  `_temp` form that takes all large temporaries from one caller-provided
  area, so the RAM need is known exactly and the stack stays under 3 KB.
- Key generation uses the 2025 improvements (eprint 2025/1239): on this
  board about 0.5 s for FN-DSA-512.
- Without an FPU it emulates IEEE-754 doubles with integers, in constant
  time, and produces the same signatures as on hardware floating point.
  That is what makes the host comparison below meaningful.

Caveat from upstream: until FIPS 206 is final, keys and signatures of this
code may not interoperate with the final standard.

## How it is wired into the project

- `fndsa_glue.c`: the Keccak permutation goes through `pqc_prims.h`
  (`FNDSA_EXTERNAL_KECCAK=1`, the one local change in the library, see
  `SOURCE.md`); `fndsa_sysrng()` is the harness RNG.
- `suite.c`: for each parameter set and variant, the upstream known-answer
  test, then key generation, signing and verification with fingerprints of
  the secret key, the public key and the signature.
- Variants: `ref` is the library as it is, with the reference Keccak. `opt`
  uses the optimized Keccak and, on the device, the ESP32-C3 build of the
  signing code (below). Key generation and verification are the same code in
  both variants; they only gain from Keccak.
- The host build runs the unchanged library for both variants and uses the
  machine's floating-point unit (NEON or SSE2). The device emulates floating
  point. `tools/pqc check fndsa` therefore compares emulated signing on
  RV32, in both its forms, with hardware floating point.

## Tests

- Upstream known-answer test (`inner_test_kat` of `test_fndsa.c`): a key
  pair and a signature from a seed, SHA3-256 of `sk || vk || sig` compared
  with the upstream value. The host runs all ten vectors per parameter set,
  the device the first two (each is a key generation). Data in
  `kat_fndsa.h`, regenerate with `gen_kat.py`.
- Upstream verification vector, and the same signature with one bit
  flipped (must be rejected).
- `fndsa/opt/fpr-selftest` (device): 20000 operand pairs through the
  optimized and the upstream floating-point routines.
- `tests/run_fpr_host.sh`: the same comparison on the build machine, also
  against hardware doubles, with zeros, equal magnitudes, cancellations and
  rounding ties over-represented. One billion pairs: no mismatch.

## RAM

All temporaries are in the `tmp` area; n is 512 or 1024.

| Operation | tmp area | FN-DSA-512 | FN-DSA-1024 | Task stack used |
|---|---|---|---|---|
| Key generation | 22n + 31 | 11295 B | 22559 B | 1.0 KB |
| Signing | 59n + 31 | 30239 B | 60447 B | 2.3 to 2.4 KB |
| Verification | 4n + 31 | 2079 B | 4127 B | 1.1 KB |

Keys and signature: FN-DSA-512 sk 1345, vk 897, sig 666 bytes; FN-DSA-1024
sk 2369, vk 1793, sig 1280 bytes. Nothing is taken from the heap.

Code size (RV32IMC, `-O2`): shared 9.8 KB, key generation 51.8 KB, signing
29.9 KB (32.0 KB for the ESP32-C3 build), verification 0.8 KB.

## Where the time of a signature goes

Counted on the host with the emulated floating point, per FN-DSA-512
signature: 100544 additions, 86166 multiplications, 2304 divisions, 512
square roots, 9920 integer-to-float conversions, 161 Keccak permutations.
With the instruction counts of the compiled functions that is about 85 % of
the 46 million cycles of one signature; additions alone are about half.

## Optimizations

### Kept: 32-bit floating-point addition and multiplication (`fpr_c3.c`)

c-fn-dsa writes the emulation with 64-bit integers. On RV32IMC a 64-bit
shift by a variable count, a 64-bit comparison and a 64-bit leading-zero
count each become long branch-free sequences (there is no count-leading-
zeros instruction either). `fpr_c3.c` does the same arithmetic on 32-bit
halves: two 32-bit comparisons order the operands, the alignment shift moves
by a whole word first and then by 0..31 bits, leading zeros are counted on
the one word that holds the top bit, and the 53x53-bit product is built from
four 32x32-bit products.

`fpr_add_sub()` computes x + y and x - y together. Ordering, unpacking and
aligning the operands is shared; only the add/subtract, the normalization
and the rounding are done twice. c-fn-dsa already funnels every such pair of
the FFT code through the macro `FPR_ADD_SUB` (it has a Cortex-M4 assembly
routine behind it); `fndsa_sign_c3.c` redefines the macro.

| Function | Instructions, c-fn-dsa | Instructions, fpr_c3.c |
|---|---|---|
| `fpr_add` | 197 | 139 |
| x + y and x - y | 2 x 197 | 222 |
| `fpr_mul` | 103 | 80 |

Results are identical bit for bit (see Tests).

Measured on the device (FN-DSA-512 / FN-DSA-1024 signing):
288.6 ms -> 228.5 ms and 623.5 ms -> 490.2 ms, a factor 1.26 and 1.27.

### Constant time: what the compiler does to masks

GCC 14.2 for RV32IMC recognizes selections written as `(a ^ b) & mask` and
compiles them into a conditional branch around a move, because the
instruction set has no conditional move. The first version of `fpr_c3.c`
came out with four such branches on the magnitude comparison of the two
operands, which are secret during signing. The file now hides its masks
from the optimizer (`C3_OPAQUE`, an empty inline assembly statement), and
the compiled `fpr_add`, `fpr_add_sub` and `fpr_mul` contain no conditional
branch and no jump; this was checked in the disassembly and should be
checked again after a compiler change.

The same compiler puts conditional branches into c-fn-dsa's own `fpr_add`
(for example on the 64-bit shift helper). That is the unchanged upstream
code and was left alone, but it means the `ref` variant on this target is
not free of secret-dependent branches, while the `opt` signing arithmetic
is.

### Tried and dropped: `-O3`

- `-O3` for the whole library: the image no longer links. Code plus the
  60 KB signing work area exceed the 321 KB that a RAM-loaded image may
  occupy (2 KB over).
- `-O3` for the ESP32-C3 signing unit only (`-D FNDSA_C3_COPTS=-O3`): the
  unit grows from 32.0 KB to 59.6 KB because the floating-point routines are
  inlined into the FFT loops. The image links, with 9 KB to spare, but
  crashed during startup when loaded into RAM (instruction access fault
  before `app_main`); the cause was not investigated. No timing was
  obtained. Since the hot routines are straight-line code that `-O2`
  already compiles well, the possible gain is the call overhead, a few
  percent, for almost twice the code.

### Not done

- Division and square root are still c-fn-dsa's bit-serial loops (55 and 54
  iterations). They are about 9 % and 2 % of an optimized signature. A
  constant-time division built on the multiplier (reciprocal by Newton
  iteration, then exact correction) would remove most of that; it needs a
  careful error bound and was left out.
- Assembly versions of `fpr_add` and `fpr_mul`. The C versions compile to
  139 and 80 instructions; hand-written code could save perhaps another
  10 to 15 %.
- Key generation was not touched. It is integer and fixed-point arithmetic
  on big numbers plus SHAKE256 for sampling.
- No reduction of the temporary areas below upstream's 59n + 31 bytes for
  signing.

## Results

Device numbers are from `results/fndsa.json`; see the project README for
the full table. With the reference Keccak in both variants:

| FN-DSA-512 | ref | opt |
|---|---|---|
| Key generation (average of 4 seeds) | 533 ms | 533 ms |
| Signing | 288.6 ms | 228.5 ms |
| Verification | 4.10 ms | 4.10 ms |

| FN-DSA-1024 | ref | opt |
|---|---|---|
| Key generation (average of 2 seeds) | 1713 ms | 1713 ms |
| Signing | 623.5 ms | 490.2 ms |
| Verification | 8.22 ms | 8.22 ms |

Key generation time depends on the seed (430 to 666 ms over four seeds for
FN-DSA-512). When an optimized Keccak is linked in as `pqc_keccakf1600_opt`,
the `opt` column improves further without changes here: a signature runs
161 (FN-DSA-512) or 313 (FN-DSA-1024) permutations.
