# pqmagic_aigis_sig_plus: Aigis-Sig+ (NGCC round-1 candidate)

Reference implementation as shipped with PQMagic
(`third_party/pqmagic/sig/aigis-sig-plus/std`, Apache-2.0), parameter sets
I, II and III: `aigis-sig-plus-1`, `-2`, `-3`.

Built as PQMagic builds the library (`USE_SHA3`), one compilation unit per
set (`asp_unity.inc`). `pspm.c` and `fips202.c` each define static
`load64`/`store64`; the unit renames those of `pspm.c`.

**Upstream bug fixed in the vendored copy.** `polyvecl_uniform_gamma1()`
wrote one polynomial past the end of its output (finding 1 in
`suites/pqmagic_common/NOTES.md`). With the reference build the overflow is
silent; in this build every signature of set I failed to verify until the
stray call was disabled. Signatures are unchanged by the fix.

**Crash on the device, avoided by a compiler option.** The first device run
stopped with a store access fault in `reject_uniform()`: the counter that
`poly_uniform()` initializes as an `int` is read there through an
`int32_t *` (`long *` with this toolchain), and GCC, relying on strict
aliasing, had dropped the initialization. All suites of this family are now
compiled with `-fno-strict-aliasing` (finding 4 in
`suites/pqmagic_common/NOTES.md`).

Checks: round trip, tamper test, equal fingerprints on device and host,
identical outputs with the implementation's own Keccak (18 fingerprints),
400 signatures per set verified on the host, sanitizer run clean apart from
left shifts of negative values.

## What runs on the device

Only parameter set I. With `-fno-strict-aliasing` set I runs and matches
the host (key generation 11.3 ms, signing 46.9 ms, verification 12.4 ms,
75 KB of stack). Key generation of set II used 83 KB of stack, and its
signing overflowed a 112000-byte stack, the largest a job can get from the
heap of this image; set III has larger vectors still. `suite.c` therefore
registers sets II and III only in the host build, where all three pass.
Running set II on the device would need the extra heap region of
`kemsig.c` (`KEMSIG_EXTRA_HEAP`, as `suites/pqmagic_mldsa87` uses) in an
image of its own; that was not tried.

Signatures have variable length; the suite reports the length of the last
one and the maximum.
