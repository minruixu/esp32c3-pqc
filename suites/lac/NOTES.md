# lac: LAC-v3b (Light, 128, 192, 256)

LAC is a KEM over a polynomial ring with a byte-sized modulus (256 in this
version of the code), with errors corrected by a BCH code. It was among the winners of the 2019
Chinese national cryptographic algorithm competition and a second-round
candidate of the NIST process.

## Source

https://github.com/pqc-lac/lac-intel32, commit `6cfd85a` (2020-04-15),
directory `LAC-v3b/opt32` (the 32-bit C version). The repository has no
license, so the code is fetched by `tools/fetch/lac.sh` into `downloads/`
and is not part of this project. (The 64-bit repository `lac-intel64` uses
AVX2 intrinsics.)

## What the port does

- `suites/lac/make_port.py` copies the files of the KEM to
  `downloads/lac_port/` with two changes: the security level hard-coded in
  `lac_param.h` only applies when the build has not chosen one, and the
  headers get include guards. Nothing else is touched.
- LAC's `rand.c` and `rng.c` need OpenSSL (AES-256-CTR as the seed
  expander, SHA-256 as the hash, the NIST DRBG as the random source). They
  are not built. `lac_unity.inc` defines the five functions of `rand.c` with
  the same behaviour: random bytes from the harness, SHA-256 from
  `components/prims`, and AES-256 in counter mode with an all-zero initial
  counter block from `lac_port.c`.
- `lac_port.c`: the reference variant uses the constant-time software AES of
  PQClean/BearSSL (`third_party/pqclean_aes`, MIT); the optimized variant
  computes the counter blocks on the ESP32-C3 AES accelerator
  (`suites/pqmagic_common/hwaes.c`).
- All four levels are in one image; LAC has no symbol prefix, so each level
  is one compilation unit with `lac_rename.h`.

## Checks and limits

- Round trip and tamper test for every level, equal fingerprints on device
  and host, and equal fingerprints of the reference and the optimized
  variant, which shows that the hardware AES and the software AES agree.
- **Not checked against LAC's own test vectors.** The repository ships
  none, and building the original with OpenSSL was not possible here, so
  there is no independent confirmation that this port produces the same keys
  as an OpenSSL build. The two primitives involved are standard (SHA-256,
  AES-256-CTR from a zero counter); the port reproduces `rand.c` call for
  call.
- The sanitizer run on the host reports a read one element past an array
  in the BCH decoder (`compute_error_locator_polynomial`, finding 3 in
  `suites/pqmagic_common/NOTES.md`) and unaligned 32-bit loads in
  `bin-lwe.c`. Both are in the upstream code; results are not affected and
  the code runs on the ESP32-C3.

## Optimization

The seed expander is AES-256 in counter mode and the hash is SHA-256, and
the ESP32-C3 has accelerators for both. The optimized variant computes the
AES counter blocks on the AES accelerator (`hwaes.c`) and takes SHA-256
from the shared primitives. Device run with the hardware AES alone (the
optimized SHA-256 was not linked into that image, see
`suites/pqmagic_common/NOTES.md`), reference to optimized: LAC-128 key
generation 8.8 to 4.4 ms, encapsulation 13.7 to 7.7 ms, decapsulation 17.0
to 11.0 ms; LAC-256 18.5 to 12.4, 31.4 to 23.1 and 43.1 to 34.8 ms. What
remains is LAC's own polynomial arithmetic and BCH decoding, which were not
touched. Current numbers: `results/lac.json`.

## Memory

Stack: 8 to 16 KB for key generation, 12 to 24 KB for encapsulation, 14 to
27 KB for decapsulation (the job stack is 64 KB). 960 bytes of heap per
operation for the software AES key schedule (PQClean allocates it).

## Other winners of the 2019 competition

Aigis-enc and Aigis-sig are in `suites/aigis_enc` and `suites/aigis_sig`.
SCloud, another entry of that competition, continues as SCloud+
(`suites/scloudplus`). The full list of second and third prizes was not
checked for public C code.
