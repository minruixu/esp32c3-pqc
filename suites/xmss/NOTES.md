# XMSS and XMSS^MT (RFC 8391, NIST SP 800-208)

## Where the code comes from

- `third_party/xmss-reference/`: the XMSS reference implementation,
  https://github.com/XMSS/xmss-reference, commit `171ccbd` (2021-03-16),
  CC0 1.0. The files are unchanged copies; see `SOURCE.md` there.
- Test vectors: the output of the reference implementation's `test/vectors.c`
  as recorded in the tests of https://github.com/bwesterb/go-xmssmt (MIT),
  which checks itself against the same numbers. They are in `suite.c`.
- Everything in `pqc_suite/` was written for this project.

## How the reference code is built without changes

| Need of the reference code | How it is met |
|---|---|
| `<openssl/sha.h>` for SHA-256 and SHA-512 | `shim/openssl/sha.h` maps `SHA256()` and `SHA512()` to `sha2.h` of `components/prims` |
| its own `fips202.c` | not copied; `fips202.h` of `components/prims` has the same `shake128()`/`shake256()` calls |
| `randombytes()` from `/dev/urandom` | `randombytes.h` here maps it to `xmss_port_randombytes()`, which returns the harness generator or, for known-answer tests, the fixed seed |
| two cores with the same function names | `core_fast.c` includes `xmss_core_fast.c` (BDS traversal, the one that is timed); `core_slow.c` includes `xmss_core.c` under other names (host only, for the signature vectors) |
| `hash.c` | `hash_ref.c` includes it under `xmss_ref_*` names; `hash_opt.c` provides the real names and calls either the reference functions (`ref`) or its own (`opt`) |

## What `opt` adds

`ref` is the reference code as is, with the reference primitives. `opt` is the
same core with four changes. Keys and signatures are byte-identical in both.

1. **Hash layer written for fixed short inputs** (`hash_opt.c`). Every hash in
   XMSS has a short input of known length. The padded SHA-256 blocks, or the
   SHAKE state, are built in place and handed to the compression function or
   the permutation directly, without a hash context and without the copies of
   the generic one-shot API.
2. **Cached state after the key block of PRF** (SHA-256, n = 32). PRF hashes
   `toByte(3,32) || KEY || ADRS`; the first block depends only on the key. F
   calls PRF twice with the public seed and H three times, so keeping the
   state after that block makes F cost 4 compressions instead of 6 and H 6
   instead of 9. PRF_keygen with the secret seed is handled the same way (2
   compressions instead of 3). The cache holds three entries and compares the
   whole key, so a new key can never hit a stale state. With n = 24 the PRF
   input is 60 bytes, one data block plus one padding block, and nothing can
   be cached; SHAKE inputs are a single block anyway.
3. **BDS parameter k** (a parameter of the reference code, default 0): the
   optimized variant uses k = 3 for trees of height 5 and k = 4 for height 10.
   Signing then runs (h - k)/2 tree-hash steps per tree instead of h/2. The
   state grows by 2^k - k - 1 nodes per tree.

4. **PRF, F and H on the SHA accelerator** (`hash_hw.c`, device only, SHA-256
   sets with n = 32 and 24). The accelerator's registers hold hash values and
   message blocks as plain byte strings, so the output of one hash goes into
   the next block as words. For n = 32, F is: load the cached PRF state into
   the H registers, write the address block, run; load the state again,
   change one word of the block, run; write `0^32 || key` and start from the
   initial value; write `(in xor mask)` and the padding while that block is
   being compressed, run, read. Four compressions, about 95 register
   accesses, no byte-order conversion except for the seven address words.

Measured on the device, average time (the first three rows with the
reference primitives):

| Step | XMSSMT-SHA2_20/4_256 keygen | sign | verify |
|---|---:|---:|---:|
| reference code | 29.9 s | 921 ms | 477 ms |
| + hash layer and PRF state (1, 2) | 16.9 s | 521 ms | 269 ms |
| + BDS k = 3 (3) | 17.0 s | 391 ms | 270 ms |
| the same with the general hardware SHA-256 backend (`-D XMSS_HW_HASH=off`) | 2.93 s | 67.5 ms | 46.9 ms |
| + PRF, F and H on the accelerator's registers (4), final | 1.66 s | 39.9 ms | 26.3 ms |

For the other functions only step 1 applies to hashing: XMSSMT-SHA2_20/4_192
key generation went from 19.3 s to 16.2 s, XMSSMT-SHAKE256_20/4_256 from
74.5 s to 61.3 s. With BDS k = 3 their signing times went from 497 ms to
372 ms and from 1.87 s to 1.40 s.

`opt` also selects the optimized primitives of `components/prims`: the
assembly Keccak for the SHAKE sets, and hardware SHA-256 for the few hashes
that `hash_hw.c` does not cover.

## Results on the device

ESP32-C3 at 160 MHz, program in RAM, ESP-IDF v5.5.3, `-O2`. Key generation
once, signing 8 or 40 times, verification 5 times; `x` is ref time divided by
opt time. No heap is used. All 50 checks pass and all 36 fingerprints of the
device equal those of the host build (`tools/pqc check xmss`).

| parameter set | keygen ref | opt | x | sign ref | opt | x | verify ref | opt | x | stack ref/opt (bytes) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| xmssmt-sha2_20-4_256 | 29.8 s | 1.66 s | 18.0 | 918 ms | 39.9 ms | 23.0 | 476 ms | 26.3 ms | 18.1 | 4924/4444 |
| xmssmt-sha2_20-4_192 | 19.3 s | 1.41 s | 13.7 | 592 ms | 33.0 ms | 18.0 | 336 ms | 24.0 ms | 14.0 | 3900/3516 |
| xmssmt-shake256_20-4_256 | 74.4 s | 37.1 s | 2.0 | 2.27 s | 846 ms | 2.7 | 1.18 s | 591 ms | 2.0 | 5068/4908 |
| xmss-sha2_10_256 | 236 s | 13.0 s | 18.1 | 511 ms | 25.7 ms | 19.9 | 130 ms | 7.10 ms | 18.2 | 4348/4188 |
| xmss-sha2_10_192 | 153 s | 11.1 s | 13.8 | 328 ms | 21.3 ms | 15.4 | 78.3 ms | 5.56 ms | 14.1 | 3260/3164 |
| xmssmt-shake256_20-4_192 | 55.9 s | 27.5 s | 2.0 | 1.70 s | 626 ms | 2.7 | 950 ms | 468 ms | 2.0 | 4044/3980 |

Sizes in bytes (public key / signature / secret key with state, ref and opt):
XMSS-SHA2_10_256 64 / 2500 / 1373 and 1569; XMSS-SHA2_10_192 48 / 1492 /
1053 and 1193; XMSSMT-SHA2_20/4_256 and XMSSMT-SHAKE256_20/4_256 64 / 9251 /
10938 and 11015; the two 20/4_192 sets 48 / 5403 / 7138 and 7159.

The SHA-256 sets gain from all four changes, the SHAKE256 sets from the
direct hash layer, the BDS parameter and the assembly Keccak of
`components/prims` (there is no hardware for SHAKE on this chip).

## Tests

- `kat-wots-oidNN`: WOTS+ public key, signature and leaf for OIDs 1, 4, 7,
  10, 13, 16, 19, that is SHA-256 (n = 32 and 24), SHA-512, SHAKE128 and
  SHAKE256 (n = 32, 24, 64), compared with the published vectors. Run in both
  variants, so they also cover the hash layer of `hash_opt.c`.
- `<set>/pk-vector`: the timed key generation uses the seed 0, 1, 2, ... of
  the vector generator, and the public key is compared with the published
  one.
- `hostkat-*` (host only): the plain reference core reproduces the published
  public key and signature of XMSS OIDs 1, 13, 16 and XMSS^MT OIDs 2, 34, 42.
  On the device this would take a full tree rebuild per signature.
- Every set: sign, verify, reject a signature with one changed bit, and
  fingerprints of public key, signature and secret state that must match the
  host build.

## State

The secret key of the reference code is the index, the seeds and the BDS
state: 1373 bytes for XMSS-SHA2_10_256, 10938 bytes for XMSSMT-SHA2_20/4_256
(1569 and 11015 with the larger k). `xmss_core_sign()` updates it in place
with every signature. An application has to store the new state durably
before it releases the signature, otherwise a one-time key can be used twice
after a reset; that write is not part of the times below. Nothing is kept on
the heap; the peak stack is about 5 KB (11 KB for the n = 64 sets).

## How to run

A plain `tools/pqc run xmss` runs everything except the sets whose key
generation takes minutes with the reference hash; those are named in the
output and run when asked for:

```
tools/pqc run xmss -f /ref/ --idle 400
tools/pqc run xmss -f /opt/ --idle 400
tools/pqc run xmss -f xmss-sha2_10_256/ref --idle 900
tools/pqc run xmss -f xmss-shake256_10_256/opt --idle 1800
```

The row for the general hardware backend is reproduced with:

```
tools/pqc run xmss -D XMSS_HW_HASH=off --build-name xmss_exp_off -f xmssmt-sha2_20-4_256/opt
```

## Not done

- XMSS-SHAKE256_10_256 was run in the `opt` variant only (key generation
  294 s, signing 571 ms, verification 150 ms). With the reference Keccak its
  key generation is about 10 minutes (8 times that of
  XMSSMT-SHAKE256_20/4_256).
- A WOTS+ chain that keeps its state in words across steps, as the LMS
  hardware chain does, would save the per-step address conversion and
  copies; it means replacing `wots.c` of the reference code and was not
  tried.
- Heights 16 and 20 of single-tree XMSS: key generation is 64 and 1024 times
  that of height 10.
- The n = 64 parameter sets (SHA-512, SHAKE256/512) are only covered by the
  WOTS+ vectors; `hash_opt.c` leaves them to the reference functions.
