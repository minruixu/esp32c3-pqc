# pqmagic_mlkem: ML-KEM with SM3 (PQMagic)

PQMagic's ML-KEM-512/768/1024 with every use of SHA-3/SHAKE replaced by SM3:
`mlkem<level>-sm3-pqmagic`. These are PQMagic's "national hash" variants, not
FIPS 203; keys and ciphertexts are not interoperable with ML-KEM.

The same code with SHAKE is built too (`mlkem<level>-shake-pqmagic`) as a
reference point: it must reproduce the FIPS 203 known-answer vectors
(`fips203_vectors.h`, from mlkem-native), which ties PQMagic's ML-KEM code
and this port to the standard. That check passes on the device for all three
levels.

Source: `third_party/pqmagic/kem/ml_kem/std`. Built as one compilation unit
per level and hash (`mlkem_unity.inc`); `sm3_256`, `sm3_512` and the two
`*_internal` functions, which PQMagic does not prefix, are renamed per unit.

Checks: FIPS 203 vectors (SHAKE variants), round trip, tamper test, equal
fingerprints on device and host, identical outputs with PQMagic's own hash
code (48 fingerprints). The sanitizer reports signed overflow in
`reduce.c` (the usual Montgomery idiom); no memory errors.

Optimization: shared primitives and, for SM3, the lean SM3 paths
(`suites/pqmagic_common/NOTES.md`).
