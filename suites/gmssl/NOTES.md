# GmSSL: LMS/HSS, XMSS/XMSS^MT and SPHINCS+ with SM3, and Kyber-768

## Where the code comes from

`third_party/gmssl/`: GmSSL, https://github.com/guanzhi/GmSSL, commit
`24ae482` (2026-06-30), Apache-2.0. Only the files these four algorithms need
are copied, all unchanged; see `SOURCE.md` there.

## What GmSSL implements

| Algorithm | Parameter sets in GmSSL | Timed here |
|---|---|---|
| LMS / HSS with SM3 (`lms.c`) | `LMS_SM3_M32_H5`, `H10`, `H15`, `H20`, `H25`, always with `LMOTS_SM3_N32_W8`; HSS with up to 5 levels | H5, HSS H5/H5; H10 on request |
| XMSS with SM3 (`xmss.c`) | `XMSS_SM3_10_256`, `16_256`, `20_256` | 10 on request |
| XMSS^MT with SM3 (`xmss.c`) | `XMSSMT_SM3_20_2`, `20_4`, `40_2`, `40_4`, `40_8`, `60_3`, `60_6`, `60_12` (all `_256`) | 20_4 |
| SPHINCS+ with SM3 (`sphincs.c`) | one set, chosen at compile time: 128s (n = 16, h = 63, d = 7, FORS 14 x 2^12); the 128f numbers are in the header behind `#if` | 128s key generation only, on request |
| Kyber (`kyber.c`) | Kyber-768 only (`KYBER_K` is a compile-time constant); it passes GmSSL's ML-KEM-768 vector test | 768 |

The type numbers GmSSL gives the SM3 sets are its own (the header says "TODO:
submit to IETF"); `LMOTS_SM3_N32_W8 = 14` is the number RFC 9858 later gave to
`LMOTS_SHAKE_N24_W2`.

## Layout: four images

All four algorithms with their tests are 160 KB of code and 99 KB of static
data, which does not fit next to ESP-IDF in the RAM of the chip. Each
algorithm therefore has its own suite: `gmssl_lms`, `gmssl_xmss`,
`gmssl_sphincs`, `gmssl_kyber`. They share the sources in `common/`;
`common/gmssl.cmake` picks the files for `GMSSL_ALG`.

## How the GmSSL files are built without changes

- `gmssl_port.c` supplies what the files expect from the rest of GmSSL:
  `rand_bytes()` (the harness generator), `format_print()` and
  `format_bytes()` (do nothing), and the SM3 and SHA-3 entry points.
- `gmssl_sm3_ref.c` and `gmssl_sha3_ref.c` include GmSSL's `sm3.c` and
  `sha3.c` under `gmssl_ref_*` names.
- `k_kyber.c` includes `kyber.c` with GmSSL's one-shot `sha3_256`,
  `sha3_512`, `shake128`, `shake256` renamed, because `components/prims` has
  functions of the same names with other argument orders.
- `t_*.c` include GmSSL's test programs with `main` renamed.

## `ref` and `opt`

- `ref`: GmSSL's code with GmSSL's own SM3 and SHA-3.
- `opt` (`gmssl_fast = 1`): the same GmSSL code with other hash entry points.
  `sm3_init/update/finish` buffer with less overhead, and the SHA-3 functions
  absorb and squeeze directly on the state and call `pqc_keccakf1600()`, so
  the assembly Keccak of `components/prims` applies to Kyber. For the SM3
  compression function there are two candidates, the SM3 backend of
  `components/prims` and GmSSL's own; `gmssl_port_calibrate()` times both on
  the device when the variant starts and fast mode uses the quicker one. The
  result is printed as `gmssl-sm3-compress/opt`.

On the ESP32-C3 GmSSL's own SM3 compression (fully unrolled, 13 KB of code)
is the quicker of the two. Before the calibration was added, `opt` used the
`components/prims` backend unconditionally and was slower than `ref`: LMS H5
key generation 9.23 s against 8.66 s, XMSS^MT 20/4 key generation 26.3 s
against 24.8 s. So for the SM3-based algorithms `opt` can gain only what the
leaner buffering saves; the large gains are in Kyber (Keccak).

No algorithm-level change was made to GmSSL. Two things in its code set the
resource use on this chip and are worth knowing:

- LMS and XMSS keys keep the whole Merkle tree in heap memory (`malloc`):
  2 KB for LMS H5, 65 KB for LMS H10 and XMSS height 10. Signing is then only
  the one-time signature, but H15 (2 MB) cannot fit. The LMS code in
  `suites/lms` supports the same SM3 parameter sets with 10.7 KB per key.
- SPHINCS+ keeps a whole FORS tree (8191 nodes, 131 KB) in a local variable
  while signing, and an XMSS tree (16 KB) while generating a key. The harness
  takes job stacks from the heap, and a 160 KB stack could not be allocated
  (`rc = -100`). The SPHINCS+ jobs therefore run in a task with a 138 KB
  static stack (`common/bigstack.c`, device only); the image then uses 313 KB
  of the 321 KB RAM segment.
- GmSSL keeps keys, signatures and contexts in local variables as well: its
  HSS test needs 40.8 KB of stack, XMSS^MT signing 29.2 KB. With 24 KB the
  first device run ended in a stack protection fault.

## Results on the device

ESP32-C3 at 160 MHz, program in RAM, ESP-IDF v5.5.3, `-O2`, optimized
primitives of `components/prims` in the image. `x` is ref time divided by opt
time. Heap is what GmSSL allocates for the cached trees.

| algorithm | keygen ref | opt | x | sign ref | opt | x | verify ref | opt | x | stack (bytes) | heap (bytes) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| LMS-SM3 H5 (W8) | 8.81 s | 8.85 s | 1.0 | 136 ms | 137 ms | 1.0 | 146 ms | 146 ms | 1.0 | 3420 | 2048 |
| HSS-SM3 H5/H5 | 17.8 s | 17.8 s | 1.0 | 135 ms | 136 ms | 1.0 | 283 ms | 285 ms | 1.0 | 12332 | 4096 |
| XMSS^MT-SM3 20/4 | 24.9 s | 24.9 s | 1.0 | 89.2 ms | 89.4 ms | 1.0 | 387 ms | 388 ms | 1.0 | 29228 | 8192 |

| algorithm | keygen ref | opt | x | encaps ref | opt | x | decaps ref | opt | x | stack (bytes) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Kyber-768 | 23.7 ms | 9.70 ms | 2.4 | 25.7 ms | 11.5 ms | 2.2 | 29.3 ms | 15.1 ms | 1.9 | 16004 |

Sizes in bytes (public key / signature): LMS-SM3 H5 56 / 1292, HSS-SM3 H5/H5
60 / 2644, XMSS^MT-SM3 20/4 68 / 9251. GmSSL's own tests on the device: 0.84 s
and 40.8 KB of stack (LMS subset), 3.05 s (XMSS subset), 0.12 s (SPHINCS+
subset), 0.31 s and 43.5 KB of stack (all of `kybertest`).

State of the measurements:

- Kyber, LMS/HSS and XMSS^MT are from the final build; device and host
  fingerprints are equal (`tools/pqc check gmssl_kyber`, `gmssl_lms`,
  `gmssl_xmss`). For the SM3 algorithms `opt` equals `ref` because fast mode
  keeps GmSSL's own SM3 (see above).
- SPHINCS+-SM3 128s: key generation runs on the device on the static stack,
  16.0 s in the `opt` variant, and its public key has the fingerprint of the
  host build. **Signing does not work on this chip.** With the 138 KB static
  stack the board gave no output for 20 minutes where about 2 minutes were
  expected; the most likely cause is a stack overflow, since GmSSL holds FORS
  trees of 131 KB in local variables and the RAM segment is 321 KB in all.
  The signing job is therefore not started on the device any more, and
  verification, which needs a signature, was not measured. Everything passes
  on the host in both variants. `ref` key generation was not run.
- `XMSS_SM3_10_256` does not run on the device as built: key generation
  returns an error at once, because GmSSL allocates the whole tree (65.6 KB)
  with one `malloc` and next to the 56 KB job stack no block of that size is
  left in the heap (154 KB free at start, in several regions). It passes on
  the host.
- `LMS_SM3_M32_H10` was not run on the device. Its key generation is 32 times
  that of LMS H5 (about 4.7 minutes) and it needs the same 65 KB allocation,
  so it may fail the same way.

For comparison, the LMS code of `suites/lms` on the same SM3 parameter set
(H5, W8): key generation 7.57 s, signing 115 ms, verification 125 ms, 2.1 KB
of stack and no heap.

## Tests

- GmSSL's own test programs. On the host all four run completely, in both
  variants (`hostfull`). On the device a subset runs (`quick`): the SM3 HSS
  known-answer test, LM-OTS, size tables, XMSS address and root tests, the
  SPHINCS+ WOTS tests, and all of `kybertest` (which includes the ML-KEM-768
  vector).
- Every timed algorithm: sign, verify, reject a signature with one changed
  bit, and fingerprints that must match the host build.

## How to run

```
tools/pqc run gmssl_kyber
tools/pqc run gmssl_lms --idle 400
tools/pqc run gmssl_xmss --idle 400
tools/pqc run gmssl_sphincs -f gmssl-sphincs-sm3-128s/ref
tools/pqc run gmssl_sphincs -f gmssl-sphincs-sm3-128s/opt
tools/pqc run gmssl_lms -f gmssl-lms-sm3-h10/opt --idle 600
```
