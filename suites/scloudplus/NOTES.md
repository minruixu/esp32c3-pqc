# scloudplus and scloudplus_256: SCloud+

SCloud+ is a KEM on plain (unstructured) LWE with q = 2^10, a candidate of
the NGCC round 1. Its public matrix A is n x n (n = 608, 832, 1184 for the
128, 192 and 256 levels) and is expanded row by row from a 16-byte seed, in
one of three "families": AES-128 in counter mode (the default), SHAKE128 or
SM3.

## Source

https://github.com/scloudplus/scloudplus, commit `088920a` (2026-06-30),
`Implementations/Reference_Implementation` with the shared core. The
repository has no license file, so the code is fetched by
`tools/fetch/scloudplus.sh` into `downloads/` and is not part of this
project. The fetch script also runs `suites/scloudplus/make_port.py`, which
writes two derived files to `downloads/scloudplus_port/` (the hash sources
with Keccak-f[1600] and the SM3 compression function taken from
`pqc_prims.h`) and regenerates `pqc_suite/kat_expected.h`.

## Build

One compilation unit per level and family (`scloudplus_unity.inc`).
SCloud+ has no symbol prefix; `sc_rename.h` gives every global name the
prefix of the unit. `parameters.h` in this directory selects the level
header that the core expects on its include path.

- `scloudplus`: levels 128 and 192, families SM3, SHAKE and AES.
- `scloudplus_256`: level 256, the same three families.

## Known-answer test

The submission ships test vectors. `kat_expected.h` holds, for the first
vector of each file, the seed of the deterministic generator and the SHA-256
of the expected public key, secret key, ciphertext and shared secret. The
suite seeds the submission's generator (`api_pkc/drng.c`), runs key
generation, encapsulation and decapsulation, and compares. On the host all
fifteen level/family combinations pass, for the reference and the optimized
variant; on the device the combinations of the two suites are run.

`suites/pqmagic_common/check_upstream_hash.sh scloudplus` confirms on the
host that the unmodified hash sources give the same outputs.

## Optimization

Time goes to two things: expanding A (608 rows of 760 bytes at level 128)
and the two big products A*S + E (key generation) and S'*A + E'
(encapsulation, and again in decapsulation for the re-encryption check),
about three million 16-bit multiply-adds each at level 128. `sc_opt.inc`
addresses both; the upstream functions are kept under `*_upstream` names
and serve the `ref` variant.

1. **Paired-coefficient products.** Everything is modulo 2^10, A has 10-bit
   entries and the secrets are small. Two coefficients are packed into the
   16-bit halves of a 32-bit word, the secret is biased to be non-negative
   (`t = s + 15`), and `(c + t*a) & 0x03ff03ff` updates two coefficients
   with one load, one multiply, one add and one store. The bias is removed
   at the end with the column (or row) sums of A. The code checks that all
   secret entries are in [-15, 15] and otherwise falls back to the upstream
   function. Cost: one extra row buffer, and in key generation a packed
   copy of the secret (about 10 KB at level 128, 20 KB at level 192).
2. **AES family: hardware AES.** The rows of A are AES-128 counter blocks
   with a little-endian counter in the first four bytes, which the
   accelerator's own counter mode cannot produce, so
   `suites/pqmagic_common/hwaes.c` drives it block by block: the three zero
   input words are written once, then one register write, the trigger, the
   wait and four reads per block. The reference AES is byte-oriented C.
3. **SM3 family: lean row expansion.** A row is 24 one-block SM3 calls of
   `seedA || row || counter`. The upstream helper allocates, copies and pads
   for each; the optimized function lays the padded block out once per row.
4. SHAKE and SM3 themselves come from the shared primitives.

Measured on the device with all of the above and the shared optimized
primitives linked in (milliseconds, reference to optimized; all known-answer
tests pass in both variants and all 48 fingerprints equal the host's):

| Level, family | key generation | encapsulation | decapsulation |
|---|---:|---:|---:|
| 128 AES | 1380 to 169 | 1372 to 148 (9.3x) | 1364 to 145 |
| 128 SHAKE | 712 to 438 | 704 to 418 (1.7x) | 697 to 415 |
| 128 SM3 | 901 to 565 | 890 to 542 (1.6x) | 886 to 541 |
| 192 AES | 2752 to 406 | 2735 to 351 (7.8x) | 2730 to 355 |
| 192 SHAKE | 1506 to 922 | 1529 to 869 (1.8x) | 1523 to 873 |
| 192 SM3 | 1977 to 1148 | 2007 to 1087 (1.8x) | 2009 to 1094 |

An earlier run without the shared primitives and without item 3 shows what
the matrix products and the hardware AES give on their own: level 128
encapsulation 1351 to 157 ms (AES), 724 to 602 ms (SHAKE), 907 to 781 ms
(SM3). Current numbers are in `results/scloudplus.json`.

With the hardware AES the remaining time of the AES family is roughly 40%
the matrix product, 15% the AES blocks, 15% unpacking the 10-bit rows, and
sampling, packing and hashing for the rest (estimated from cycle counts of
the loops, not profiled).

## Memory

| Level | public key | secret key | ciphertext | stack ref | stack opt |
|---|---:|---:|---:|---:|---:|
| 128 | 6096 | 7440 | 6160 | 32 KB | 35 KB |
| 192 | 11456 | 13872 | 12645 | 60 KB | 63 KB |

- The SM3 family allocates inside its hash helper (up to twice the public
  key size, 19 to 24 KB) and prints an error and returns wrong hashes when
  the allocation fails. That happened on the device when the job stack was
  larger than needed and took the whole large heap block; the job stacks
  are now sized to what the operations use.
- **Level 256, SM3 family, fails on the device.** `scloudplus_256` passes
  its known-answer tests for SHAKE and AES (encapsulation 3257 to 1669 ms
  and 5573 to 714 ms, 84 KB of stack), but in the SM3 family the hash helper
  cannot get its three buffers of 16 to 18 KB next to the job stack, with a
  110000-byte stack and with a 90 KB one, and the known-answer and tamper
  tests fail (they pass on the host). It needs either a smaller image or
  the hashing of the public key done without the helper's copies; not done.
- **Levels 384 and 512 do not fit.** The reference code works on full
  16-bit matrices on the stack: S' and E1 are mbar x n = 16 x 1664 words
  (53 KB each) at level 384 and 77 KB each at level 512, next to keys and
  ciphertexts of 33 to 40 KB each (level 384). Decapsulation alone needs
  about 150 KB of stack at level 384, plus 107 KB for the key and
  ciphertext buffers, plus the image. Making them fit would mean storing
  the secrets as bytes, sampling and packing row by row, and re-using the
  ciphertext buffer during re-encryption: a rewrite of `pke.c`, not done.
  Both levels pass their known-answer tests on the host with the reference
  and the optimized code.

## Side channels

The upstream decapsulation consumes a message-dependent number of XOF
blocks in its rejection sampler (publicly reported); this port does not
change that. The optimized matrix products run in time independent of the
secret values (the range check is a single branch on a property that always
holds for honestly generated secrets).
