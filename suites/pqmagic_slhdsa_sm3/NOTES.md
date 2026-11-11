# pqmagic_slhdsa_sm3: SLH-DSA (SPHINCS+ structure) with SM3 (PQMagic)

PQMagic's `slh-dsa-sm3-128f` and `slh-dsa-sm3-128s`, with the simple and the
robust tweakable hash: `slhdsa-sm3-128f-pqmagic`, `slhdsa-sm3-128s-pqmagic`,
and the same with `-robust`. These are SPHINCS+ parameter sets instantiated
with SM3; FIPS 205 has no SM3 instance.

Source: `third_party/pqmagic/sig/slh_dsa/std`, one compilation unit per
parameter set and tweakable hash (`slhdsa_unity.inc`). The macros
`SLH_DSA_MODE`, `SLH_DSA_HASH_MODE_NAMESPACE` and `THASH` are set exactly as
PQMagic's CMakeLists.txt sets them.

Checks: round trip, tamper test, equal fingerprints on device and host (all
24), identical outputs with PQMagic's own SM3 on the host. PQMagic ships no
known-answer files for the SM3 instances.

Run time is SM3 compressions and nothing else (about 107 000 for one 128f
signature), so it follows `pqc_sm3_compress()`. The optimized variant also
takes the lean one-shot `sm3()` of `suites/pqmagic_common/port/pqm_sm3.c`,
which matters here because every hash call is one or two blocks long.

The 128s signatures take over a minute and a half each with the reference
SM3; run them one variant at a time, for example
`tools/pqc run pqmagic_slhdsa_sm3 -f slhdsa-sm3-128s-pqmagic/ref --idle 800`.

Stack: 2 to 4 KB. No heap.
