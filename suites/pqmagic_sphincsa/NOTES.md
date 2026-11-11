# pqmagic_sphincsa and pqmagic_sphincsa_192f: SPHINCS-alpha (PQMagic)

SPHINCS-alpha (CRYPTO 2023) replaces the Winternitz checksum of SPHINCS+ by
a constant-sum encoding. PQMagic implements it for SM3 (128f, 128s), SHA-2
and SHAKE (128/192/256, f and s).

Source: `third_party/pqmagic/sig/sphincs-a/std`, one compilation unit per
parameter set (`sphincsa_unity.inc`), simple tweakable hash.

## The encoding table

The reference code builds, at first use, a static table
`dp[len+1][len*(w-1)/2+1]` of 256-bit integers: 278 KB for 128f, 584 KB for
128s, up to 2.2 MB for 256s. No RAM image can hold that. `sphincsa_dp.c`
stores the same table compactly: each row uses only the bytes its largest
entry needs, the symmetry `dp[i][j] = dp[i][i(w-1)-j]` halves the rows, and
each row is its own heap block (the heap of the image is split into
regions). It is built with a sliding sum, which also makes the build a
matter of milliseconds.

| Set | reference table | compact table |
|---|---:|---:|
| 128f | 278 KB | 50 KB |
| 128s | 584 KB | 102 KB |
| 192f | 497 KB | 128 KB |
| 192s | 1285 KB | 332 KB |
| 256f | 1063 KB | 364 KB |
| 256s | 2255 KB | 769 KB |

So 128f and 128s are within reach of the device, 192f is at the limit, and
192s, 256f and 256s do not fit in RAM in either form.

**State on the device.** `pqmagic_sphincsa_192f` does not run yet: its
first job stopped at the `assert` after `sphincsa_dp_select()` failed to
allocate. That image has 162 KB of free heap, and the 128 KB table, the two
build rows (15 KB) and the 20 KB job stack add up to 163 KB. A smaller job
stack (the scheme uses about 4 KB) or building the table with narrower
temporary rows should make it fit; neither was tried.
`pqmagic_sphincsa` runs on the device for 128f in all three hash families:
18 of 18 fingerprints equal the host's, the reference and the optimized
variant agree, and the table shows as 66 KB of heap in the first job. With
the shared optimized primitives (milliseconds, reference to optimized):
SM3 key generation 192 to 128, signing 4889 to 3455, verification 259 to
186; SHA-2 160 to 50, 4110 to 1307, 220 to 77 (hardware SHA-256); SHAKE
633 to 406, 16280 to 10441, 835 to 540. The 128s sets (102 KB table,
signatures that take minutes) were not run on the device; they are verified
on the host only. In a flash-based firmware the table would be a constant
in flash and all sets would be possible; that was not tested.

The compact table is used for both variants (`ref` and `opt`): there is no
other way to run the scheme here. The change is one `#ifdef PQC_COMPACT_DP`
block in the vendored `wots.c`. The host check
(`suites/pqmagic_common/check_upstream_hash.sh pqmagic_sphincsa`) builds the
unchanged upstream code, with its static table and its own hash functions,
and compares every key and signature: identical for 128f and 128s in all
three hash families and for 192f.

The table shows up in the results as heap use of the first job of a
parameter set.

## Suites

- `pqmagic_sphincsa`: 128f and 128s for SM3, SHA-2 and SHAKE.
- `pqmagic_sphincsa_192f`: 192f for SHA-2 and SHAKE (the 128 KB table needs
  the heap of the image to itself). Host only for now, see above.

SHA-256 goes through `pqc_sha256_compress()` (one `#ifdef PQC_PRIMS_SHA256`
block in the vendored `sha2.c`), SM3 and Keccak through the PQMagic port
layer, so the hardware SHA-256 and the fast SM3 and Keccak apply.

The 128s signatures take minutes with the reference primitives; run one
variant at a time (`-f sphincs-a-sm3-128s/ref --idle 800`).
