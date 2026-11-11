# pqmagic_aigis_enc_plus: Aigis-Enc+ (NGCC round-1 candidate)

Reference implementation as shipped with PQMagic
(`third_party/pqmagic/kem/aigis-enc-plus/std`, Apache-2.0), the three
parameter sets with ring dimension 512, 1024 and 2048:
`aigis-enc-plus-512`, `-1024`, `-2048`.

Built the way PQMagic's CMakeLists.txt builds the library (`USE_SHA3`), one
compilation unit per set (`aep_unity.inc`), with the symbol prefix supplied
through PQMagic's own `namespace.h`. Random bytes come from the harness
instead of `randombytes.c`; Keccak-f[1600] comes from `pqc_prims.h`.

Checks: round trip, equal fingerprints on device and host (24), identical
outputs with the implementation's own Keccak. No known-answer comparison:
the candidate's KAT program uses a different hash configuration
(`USE_NICCS_API`) and was not built.

**Tamper test.** For the 512 set a one-bit change of the ciphertext is
accepted and gives the same shared secret. The cause is in `mkem_dec()`: see
finding 2 in `suites/pqmagic_common/NOTES.md`. The suite reports this as
information (`@INFO|...|modified ciphertext was ACCEPTED...`), because it is
the behaviour of the reference code, not of the port.

Stack: 9 to 58 KB.
