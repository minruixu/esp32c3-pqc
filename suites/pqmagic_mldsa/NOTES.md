# pqmagic_mldsa and pqmagic_mldsa87: ML-DSA with SM3 (PQMagic)

PQMagic's ML-DSA-44/65/87 with SHAKE replaced by SM3:
`mldsa<level>-sm3-pqmagic`. Not FIPS 204; not interoperable with ML-DSA.

The same code with SHAKE (`mldsa<level>-shake-pqmagic`) is the reference
point: key generation from a seed, deterministic signing and verification
must reproduce the FIPS 204 vectors of mldsa-native (`fips204_vectors.h`).
That check passes on the device for all three levels.

Source: `third_party/pqmagic/sig/ml_dsa/std`, one compilation unit per level
and hash (`mldsa_unity.inc`). PQMagic signs deterministically by default
(`ML_DSA_RANDOMIZED_SIGNING` is not set); the suite keeps that, so all
signing runs take the same time.

## Memory

PQMagic's ML-DSA is the pq-crystals reference structure and keeps
everything on the stack:

| Level | key generation | signing | verification |
|---|---:|---:|---:|
| 44 | 39 KB | 54 KB | 37 KB |
| 65 | 61 KB | 82 KB | 59 KB |
| 87 | 98 KB | 125 KB | 94 KB |

Level 87 does not fit the largest heap block of the RAM image (112 KB).
`suites/pqmagic_mldsa87` builds only that level and defines
`KEMSIG_EXTRA_HEAP=143360`: `kemsig.c` then registers a static buffer of
that size as an extra heap region, from which the 136 KB job stack is taken.

Checks: FIPS 204 vectors (SHAKE variants), round trip, tamper test, equal
fingerprints on device and host, identical outputs with PQMagic's own hash
code. Sanitizer run: clean.

Optimization: shared primitives and the lean SM3 paths.
