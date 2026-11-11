# aigis_enc: Aigis-enc, parameter sets 1 to 4, SM3 and SHAKE

Aigis-enc is the lattice KEM (asymmetric module-LWE) that took a first prize
in the 2019 Chinese national cryptographic algorithm competition.

## Source

`third_party/pqmagic/kem/aigis-enc/std` (PQMagic, Apache-2.0; see
`third_party/pqmagic/SOURCE.md`). PQMagic builds each parameter set in one
of two hash modes; this suite builds all eight combinations into one image
and names them `aigis-enc-<set>-sm3` and `aigis-enc-<set>-shake`.

## How it is built

`aigis_enc_<set>_<hash>.c` sets `AIGIS_ENC_MODE` (and `USE_SHAKE`) and
includes `aigis_enc_unity.inc`, which includes the upstream sources as one
compilation unit and replaces PQMagic's symbol prefix, because that prefix
does not tell the two hash modes apart. Hashing goes through
`suites/pqmagic_common/port` to `pqc_sm3_compress()` and `pqc_keccakf1600()`.

## Checks

- PQMagic ships no known-answer files for Aigis-enc (only a generator), so
  correctness rests on: round trip of every operation, a tamper test (a
  changed ciphertext must give a different key), and equal fingerprints of
  all keys, ciphertexts and shared secrets on the device and on the host.
- `suites/pqmagic_common/check_upstream_hash.sh aigis_enc` builds the suite
  on the host with PQMagic's own SM3 and Keccak instead of the project's
  primitives: all 64 fingerprints are identical.
- Sanitizer run on the host: no memory errors; one unaligned 32-bit load in
  `cbd.c` (works on the ESP32-C3).

## Optimization

Almost all of the time is hashing: matrix expansion and noise sampling. For
parameter set 2 that is about 60 Keccak permutations per key generation in
SHAKE mode, or about 250 SM3 compressions in SM3 mode. The gains therefore
come from the shared primitives (`components/prims/opt`), which this suite
gets through the backend switch.

On top of that the SM3 variants use the lean SM3 paths of
`suites/pqmagic_common/port/pqm_sm3.c` in the optimized variant. Their
separate effect can be measured with
`tools/pqc run aigis_enc -D PQM_NO_LEAN_SM3=ON --build-name aigis_enc_nolean`,
which builds the optimized variant without them.

Not done: assembly for the NTT (q = 7681). It is a few percent of the run
time next to the hashing.

## Memory

Stack per operation: 7 to 22 KB, no heap apart from 32 bytes in key
generation (SM3 mode, inside PQMagic's `sm3_extended`).
