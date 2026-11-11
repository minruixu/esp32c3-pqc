# pqmagic_nev and pqmagic_nev_sm3: NEV (NGCC round-1 candidate)

NEV is an NTRU-based KEM. Reference implementation as shipped with PQMagic
(`third_party/pqmagic/kem/nev/std`, Apache-2.0), all twelve parameter sets:
ring dimension 512/1024/2048 with q = 769 (compressed "c" and plain), 1409
and 3329.

- `pqmagic_nev`: hashing with SHA-3/SHAKE, as PQMagic's library build does
  (`USE_SHA3`). Names `nev-<n>-<q>[-c]`.
- `pqmagic_nev_sm3`: hashing with SM3 through the helper functions that the
  NGCC organisers provide (`USE_ICCS`), the configuration of the candidate's
  known-answer program. Names `nev-<n>-<q>[-c]-sm3`.

Both are the same sources (`nev_unity.inc`); `pqmagic_nev_sm3/suite.cmake`
only sets a switch. Keccak-f[1600] and the SM3 compression function come
from `pqc_prims.h` (guarded changes in the vendored `fips202.c` and
`auxfunc.c`).

Checks: round trip, tamper test, equal fingerprints on device and host (96
per suite), identical outputs with the implementation's own Keccak and SM3
on the host. The candidate's KAT files are not part of PQMagic, so there is
no known-answer comparison.

Stack: 7 to 47 KB. The image holds twelve parameter sets and is large
(243 to 254 KB), which leaves a largest heap block of 90 to 102 KB; the job
stack is 56 KB.
