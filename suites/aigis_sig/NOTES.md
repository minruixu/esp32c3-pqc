# aigis_sig and aigis_sig_lowram: Aigis-sig, parameter sets 1 to 3, SM3 and SHAKE

Aigis-sig is the lattice signature scheme (asymmetric module-LWE and
module-SIS, Dilithium-like) that took a first prize in the 2019 Chinese
national cryptographic algorithm competition.

## Source and build

`third_party/pqmagic/sig/aigis-sig/std` (PQMagic, Apache-2.0), built like
`aigis_enc`: one compilation unit per parameter set and hash mode
(`aigis_sig_unity.inc`), names `aigis-sig-<set>-sm3` and
`aigis-sig-<set>-shake`.

## Checks

Round trip, tamper test (a changed signature and a changed message must both
be rejected), equal fingerprints on device and host, and identical outputs
with PQMagic's own hash code (`check_upstream_hash.sh aigis_sig`, 36
fingerprints). No known-answer files ship with PQMagic for this scheme.
Sanitizer run on the host: clean.

## Memory

The upstream code keeps the expanded public matrix and all working vectors
on the stack:

| Set | key generation | signing | verification |
|---|---:|---:|---:|
| 1 | 37 KB | 66 KB | 41 KB |
| 2 | 52 KB | 87 KB | 55 KB |
| 3 | 68 KB | 111 KB | 72 KB |

Signing with set 3 fits the largest heap block of the RAM image (112 KB)
with one kilobyte to spare, so its job stack is set to 114000 bytes.

## aigis_sig_lowram

`suites/aigis_sig_lowram` builds the same sources with `AIGIS_SIG_LOWRAM`;
its `opt` variant is `aigis_sig_lowram.inc`: key generation, signing and
verification that expand one row of the public matrix at a time instead of
holding all K rows (K-1 rows of L kilobytes saved: 9, 16 and 25 KB for sets
1, 2 and 3). Key generation and verification use each row once and lose
nothing. Signing expands the rows again in every round of its rejection
loop and is slower; that is the price of the smaller stack.

The file is generated from PQMagic's `sign.c` by these changes and nothing
else: `expand_mat()` restricted to one row (`lr_expand_row`, the copy of the
seed uses `memcpy` because the original loop variable is now the row
index); in `crypto_sign_keypair_internal`, `crypto_sign_signature_internal`
and `crypto_sign_verify_internal` the matrix variable becomes one row, the
call of `expand_mat()` is dropped and each use of row `i` is preceded by
`lr_expand_row()`; the two context wrappers call the new functions.

Keys and signatures are the same bytes as upstream's: on the host and on
the device all 18 reference/optimized fingerprint pairs are equal, and the
sanitizer run is clean.

Measured on the device (stack in KB, upstream to low-stack):

| Set | key generation | signing | verification |
|---|---:|---:|---:|
| 1 | 37.3 to 28.1 | 65.6 to 56.5 | 41.0 to 31.7 |
| 2 | 51.8 to 35.4 | 87.3 to 70.9 | 55.5 to 39.1 |
| 3 | 68.2 to 42.6 | 110.9 to 85.4 | 71.9 to 46.5 |

Key generation and verification take the same time as before. Signing
depends on how many rounds the signature at hand needs: for the test message
35.6 ms either way for set 2 with SHAKE (one round), 70.6 to 126 ms for set
3 and 81.7 to 170 ms for set 1 (several rounds).

## Optimization of the standard suite

As for Aigis-enc, the run time is dominated by hashing and follows the
shared primitives; the SM3 variants also use the lean SM3 paths.
