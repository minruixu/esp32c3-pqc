# HQC on the ESP32-C3

HQC-1, HQC-3 and HQC-5 (the former HQC-128/192/256): key generation,
encapsulation, decapsulation. NIST selected HQC in March 2025; the standard
was still being drafted when this was written.

## Source

`third_party/hqc`: the HQC team's reference implementation, release v5.1.0,
commit `06567ccb` (2026-10-26), public domain. See
`third_party/hqc/SOURCE.md`.

Why this and not PQClean's `hqc-128/192/256`: PQClean carries the round-4
submission of 2023-04-30. After the selection the designers changed the
scheme (v5.0.0 of 2025-08-22, then v5.1.0: new KEM interface, a 32-byte seed
as decapsulation key, new weights, a new GF(2^8) polynomial, new sampling,
new domain separation). Keys, ciphertexts and test vectors of the two are
different, and the standard will follow the designers' current version, not
the 2023 one. Whether NIST's draft matches v5.1.0 exactly could not be
checked; the test vectors used here are those of v5.1.0.

| | Public key | Secret key | Ciphertext | Shared secret |
|---|---|---|---|---|
| HQC-1 | 2241 | 32 | 4433 | 32 |
| HQC-3 | 4514 | 32 | 8978 | 32 |
| HQC-5 | 7237 | 32 | 14421 | 32 |

## Suites

- `hqc`: reference code of HQC-1, and the ESP32-C3 build (`opt`) of all three
  parameter sets.
- `hqc3ref`: reference code of HQC-3 alone, on a static stack (same sources,
  built with `HQC_REF3_ONLY`).
- The reference code of HQC-5 runs on the host only (see "RAM").

The upstream sources have no name prefixes, so each parameter set and
variant is one compilation unit (`hqc<level>_<ref|opt>.c`) in which
`hqc_rename.h` gives every external symbol a prefix such as `hqc1r_`.

## Two problems of the reference code on this target

Neither needed a change in `third_party/hqc`; both are handled in
`pqc_suite/`.

1. `ref/hqc.h`, `ref/parsing.h` and `ref/vector.h` include `<immintrin.h>`,
   which only exists for x86. The reference code uses no intrinsics, so an
   empty `pqc_suite/immintrin.h` is enough.
2. `BITMASK(a, size)` is defined as `(1UL << (a % size)) - 1`. `1UL` has 32
   bits on RV32. HQC-5 uses it with a shift of 37 (57637 mod 64), which is
   undefined behaviour and in practice yields the wrong mask for the last
   word of every polynomial: wrong public keys and ciphertexts on any
   32-bit machine, in the reference code as well. HQC-1 and HQC-3 shift by 5
   and 11 and are not affected. `hqc_unit_*.inc` redefines the macro with
   `UINT64_C(1)`. This was found because the device's HQC-5 fingerprints
   did not match the host's.

## Tests

- Official known-answer vectors (`kats/ref/hqc-{1,3,5}/PQCkemKAT_32.rsp`).
  `gen_kat.py` replays NIST's AES-256 CTR_DRBG on the build machine and
  stores, per vector, the three values the KEM draws from it and
  fingerprints of the expected public key, ciphertext and shared secret
  (`kat_hqc.h`). The KEM gets the drawn values through `hqc_rng_script()`,
  so no AES is needed on the device. The host checks all 100 vectors per
  level and variant; the device checks one with the reference code and
  three with the optimized code, plus a corrupted ciphertext.
- `tools/pqc check hqc`: the host runs the reference code for both
  variants, so the device's optimized code is compared with the reference.
- `hqc/opt/selftest-gf`: all 65536 products in GF(2^8), optimized against
  reference. `hqc/opt/selftest-code`: Reed-Muller decoding of random words
  and Reed-Solomon encoding and decoding with 0 to n1 symbol errors,
  optimized against reference, three parameter sets (8 rounds on the
  device; 200000 rounds on the host with `-D HQC_HOST_OPT=1`).
- `hqc<level>/opt/ct-mul`: timing of the multiplication, see below.

`tools/pqc host hqc -D HQC_HOST_OPT=1` builds the host program with the
optimized C code in the `opt` variant, which is how that code was developed.
The option is remembered in `build/host/hqc`; pass `-D HQC_HOST_OPT=0` to go
back.

## RAM

Stack use measured on the device (bytes), nothing on the heap:

| | Key generation | Encapsulation | Decapsulation |
|---|---|---|---|
| HQC-1 ref | 52924 | 59100 | 66108 |
| HQC-1 opt | 10828 | 13308 | 20444 |
| HQC-3 ref | 105412 | 118436 | 132260 |
| HQC-3 opt | 20188 | 25436 | 39260 |
| HQC-5 ref (estimate from the array sizes) | about 166000 | about 188000 | about 211000 |
| HQC-5 opt | 31324 | 39644 | 61644 |

The reference code keeps six polynomials and an 18n-bit Karatsuba work area
on the stack. Two limits of the board matter:

- The harness creates task stacks on the heap, and the ESP32-C3 heap is made
  of regions of at most 128 KB (three 128 KB "levels" of internal RAM). A
  stack above about 116 KB cannot be allocated in practice. HQC-3 reference
  decapsulation needs 132 KB; `hqc_bigstack.c` runs it on a static array
  instead (suite `hqc3ref`).
- A RAM-loaded image may occupy 321 KB in total. HQC-5 reference
  decapsulation needs about 211 KB of stack (36 KB in the KEM, 43 KB in the
  encryption, 130 KB in the multiplication); with 22 KB of key and
  ciphertext buffers and about 150 KB of firmware that is over the limit.
  On a flashed firmware, where code is not in RAM, it would fit.

## The ESP32-C3 build (`opt`)

`hqc_unit_opt.inc` replaces four upstream files and keeps the rest (the
Reed-Solomon coder, sampling, parsing, the KEM layer).

### 1. Sparse polynomial products (`hqc_c3_pke.c`, `hqc_c3_mul.S`)

Every product in HQC has one operand of small fixed weight (the secret y,
weight 67/101/132, or the ephemeral r2, weight 76/114/150). The reference
code expands it to a dense vector and multiplies dense by dense: Karatsuba
on 64-bit words down to 16 words, then a bit-serial schoolbook. On RV32 one
product takes about 270 ms for HQC-1.

Here the sparse operand stays a list of positions and the product is the sum
of the dense operand shifted to each position. The inner loop
(`hqc_c3_shift_xor_asm`) is eight instructions per 32-bit word, unrolled
eight times. One product: 374795 cycles (2.3 ms) for HQC-1, 1126985 for
HQC-3, 2369093 for HQC-5, that is 8.9 cycles per word and position. The
work area is 2n bits instead of 18n. The sparse vectors x, e and r1 are not
expanded either; their bits are flipped in the result.

The same loop in C (`-D HQC_C3_NO_ASM=1`) is about 1.6 times slower per
product: with it HQC-1 took 7.07 / 15.82 / 42.71 ms for key generation,
encapsulation and decapsulation, against 5.83 / 12.99 / 37.40 ms with the
assembly loop (both before steps 2 to 4).

**Side channel.** The reference multiplication touches the same memory in
the same order for every secret. This one runs the same instructions the
same number of times for every secret (no secret-dependent branch or loop
bound), but the secret positions decide which words of the work area are
written and by how many bits words are shifted. On the ESP32-C3 internal RAM
is not cached and a shift takes one cycle for any count, so the running time
does not depend on the positions. This is measured, not assumed: the job
`ct-mul` times the product with interrupts off for 24 supports (all shift
counts 0, all 31, lowest positions, highest positions, a single repeated
position, random) and requires identical cycle counts; they are identical
for all three parameter sets. On a processor with a data cache, or against
power or electromagnetic measurements, the access pattern is a leak that
the reference code does not have. Do not move this file to another
processor without redoing that analysis.

### 2. GF(2^8) multiplication with the hardware multiplier (`hqc_c3_gf.c`)

The Reed-Solomon coder does thousands of field multiplications; the
reference `gf_mul` (constant-time 2-bit window, loop reduction) takes about
300 cycles. Spreading the bits of both factors to every fourth position
lets one 32x32 -> 64-bit multiplication compute the carry-less product (a
column of the integer product counts at most 8 ones, which fits in four
bits); the reduction is done on the spread form. No table, no branch.
Reed-Solomon decoding: 1039148 -> 205556 cycles (HQC-1), 3727184 -> 725912
(HQC-5). The coder itself is the upstream file.

### 3. Word-wise sponge (`hqc_c3_sym.c`)

The reference goes through the incremental FIPS 202 interface, which moves
every byte in and out of the Keccak state with 64-bit shifts. HQC absorbs
and squeezes kilobytes per operation, so the state is handled as memory
here, a word at a time. About 10 % off every hash and expansion. The
permutation is `pqc_keccakf1600()`.

### 4. Reed-Muller decoder (`hqc_c3_rm.c`)

Same algorithm as the reference. The per-bit sums over the copies of a
codeword are formed by adding bytes spread to one bit per 4-bit field
instead of extracting every bit with a variable shift; the Hadamard
transform is in place, two levels per pass; the peak search is
branch-free. Decoding takes about half the time of the reference decoder
(table below).

### Where the time goes

Cycle counts on the device with all four steps, reference Keccak (21540
cycles per permutation), from the `profile` lines of the log. The two
columns for the decoders show what steps 2 and 4 changed.

| | HQC-1 | HQC-3 | HQC-5 |
|---|---|---|---|
| One product (weight of r2) | 375346 | 1126986 | 2370894 |
| Expanding h | 369113 | 716217 | 1150076 |
| Hash of the public key | 372072 | 744480 | 1181844 |
| Hash of the ciphertext | 737089 | 1496310 | 2389175 |
| Reed-Muller decoding, reference | 647812 | 931662 | 1549170 |
| Reed-Muller decoding, step 4 | 340250 | 443106 | 706642 |
| Reed-Solomon decoding, reference gf_mul | 1039148 | 1389299 | 3727184 |
| Reed-Solomon decoding, step 2 | 205556 | 268291 | 725912 |
| Encoding | 33140 | 57279 | 126141 |
| PKE key generation | 857318 | 1989302 | 3625917 |
| PKE encryption | 1418656 | 3512564 | 6740895 |
| PKE decryption | 960126 | 1857674 | 3720461 |

Decapsulation regenerates the key pair from the seed (one product,
expanding h), decrypts (one product, decoding), re-encrypts (two products,
expanding h) and hashes the public key and the ciphertext. Keccak is now the
largest single item: about 100 permutations per HQC-1 decapsulation, close
to half of its 4.46 million cycles. With an optimized permutation behind
`pqc_keccakf1600_opt` the `opt` numbers drop further without changes here.

Decapsulating a corrupted ciphertext (job `reject`) takes the same time as
decapsulating a valid one, to the microsecond, in both variants, and returns
the same rejection key as the reference code (fingerprint `ss_reject`).

## Results

See `results/hqc.json`, `results/hqc3ref.json` and the project README.
Reference Keccak in both variants:

| | ref | opt | Speedup | Stack ref (B) | Stack opt (B) |
|---|---|---|---|---|---|
| HQC-1 key generation | 276.9 ms | 5.52 ms | 50.2x | 52924 | 10828 |
| HQC-1 encapsulation | 553.3 ms | 11.46 ms | 48.3x | 59100 | 13308 |
| HQC-1 decapsulation | 1117.7 ms | 27.85 ms | 40.1x | 66108 | 20444 |
| HQC-3 key generation | 837.4 ms | 12.61 ms | 66.4x | 105412 | 20188 |
| HQC-3 encapsulation | 1672.5 ms | 27.13 ms | 61.6x | 118436 | 25436 |
| HQC-3 decapsulation | 3360.2 ms | 61.27 ms | 54.8x | 132260 | 39260 |
| HQC-5 key generation | not run on the device | 22.85 ms | n/a | about 166000 (estimate) | 31324 |
| HQC-5 encapsulation | not run on the device | 50.19 ms | n/a | about 188000 (estimate) | 39644 |
| HQC-5 decapsulation | not run on the device | 112.1 ms | n/a | about 211000 (estimate) | 61644 |

All rows with a time are verified on the device: the known-answer vectors
pass, and `tools/pqc check` finds the device's fingerprints (public key,
secret key, ciphertext, shared secret, rejection key) equal to those of the
reference code on the host. The reference code of HQC-5 was run on the host
only (all 100 official vectors pass there).

## Not done

- A decapsulation that keeps the expanded key (public key, its hash, the
  support of y) between calls would save the key regeneration and one hash,
  about a quarter of the decapsulation time. It needs an interface the
  reference does not have.
- The reference code of HQC-5 could be measured in a flashed firmware.
- h is expanded twice per decapsulation (once to regenerate the public key,
  once to re-encrypt), and y is sampled twice. Keeping them would save about
  19 of the 100 Keccak permutations but means replacing the upstream KEM
  layer, which was left as it is.
- The Hadamard transform and the peak search are plain C.
