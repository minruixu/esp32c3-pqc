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

**State on the device.** All sets that fit run on the device, in both
variants, and every fingerprint equals the host's: 36 of 36 for
`pqmagic_sphincsa` (128f and 128s in the three hash families) and 12 of 12
for `pqmagic_sphincsa_192f`. Milliseconds, reference to optimized:

| Set | Key generation | Signing | Verification | Table on the heap |
|---|---:|---:|---:|---:|
| SM3 128f | 193 to 140 | 4891 to 3448 | 259 to 186 | 66 KB |
| SHA-2 128f | 173 to 64 | 4138 to 1336 | 222 to 79 | 66 KB |
| SHAKE 128f | 643 to 416 | 16217 to 10379 | 832 to 537 | 66 KB |
| SM3 128s | 9092 to 6436 | 98067 to 69106 | 348 to 250 | 106 KB |
| SHA-2 128s | 8088 to 2606 | 83757 to 26591 | 304 to 107 | 106 KB |
| SHAKE 128s | 31909 to 20435 | 332210 to 212528 | 1157 to 746 | 106 KB |
| SHA-2 192f | 375 to 129 | 7263 to 3183 | 211 to 112 | 146 KB |
| SHAKE 192f | 1348 to 865 | 26002 to 16695 | 682 to 442 | 146 KB |

The heap column is the peak while the table is built (the table plus the two
full-width rows used to build it). The SHA-2 sets gain most because of the
hardware SHA-256; 192f uses SHA-512 for part of its hashing and gains less.

192f needed two changes to fit: its jobs get a 10 KB stack instead of the
default 20 KB (7.3 KB is used), and the image leaves out the fast SM3 backend
it never calls (`prims_off.txt`). With 172 KB of free heap, the 146 KB peak
then fits; with the 20 KB stack the table could not be allocated.
192s, 256f and 256s do not fit in RAM. In a flash-based firmware the table
would be a constant in flash and all sets would be possible; that was not
tested.

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
  the heap of the image to itself).

SHA-256 goes through `pqc_sha256_compress()` (one `#ifdef PQC_PRIMS_SHA256`
block in the vendored `sha2.c`), SM3 and Keccak through the PQMagic port
layer, so the hardware SHA-256 and the fast SM3 and Keccak apply.

The 128s signatures take minutes with the reference primitives; run one
variant at a time (`-f sphincs-a-sm3-128s/ref --idle 800`).
