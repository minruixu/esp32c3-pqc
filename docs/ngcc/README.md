# NGCC round-1 candidates on the ESP32-C3

In September 2026 the Institute of Commercial Cryptography Standards (ICCS)
published the first-round candidates of China's Next-generation Commercial
Cryptographic Algorithms Program (NGCC): 41 key-encapsulation mechanisms, 34
digital signatures and 9 key-exchange protocols, plus 35 hash functions. This
part of the project takes the public-key candidates from their official
submission packages and runs their reference implementations on an ESP32-C3.

- [inventory.md](inventory.md): what is in each package.
- [status.md](status.md): how far each candidate got on the chip, and why.
- `results/ngcc/`: the same as data, one file per parameter set.

These are candidates in an open evaluation, not standards. Several of them
have published attacks. The numbers here say what a reference implementation
costs on a small RISC-V microcontroller; they say nothing about security.

## The packages are not in this repository

The submission packages carry no license for redistribution, and other
evaluators do not redistribute them either. `tools/ngcc/fetch.py` downloads
them from <https://www.niccs.org.cn/niccs/Proposal/pc/list.html> into
`downloads/ngcc/` (4.7 GB, never committed) and records URL, size and SHA-256
of each in `results/ngcc/packages.json`. Everything that is committed here
(tools, the generic suite, small description files, results) was written for
this project.

## How the generic port works

Every package has to implement one uniform interface, because ICCS supplies
the program that generates the known-answer-test (KAT) files and that program
calls it:

- KEM: `kem_get_pk_len_bytes()` and friends, `kem_keygen`, `kem_enc`, `kem_dec`
- signature: `sig_keygen`, `sig_sign`, `sig_verify`
- key exchange: `kex_init_a`, `kex_init_b`, `kex_generate_pass1_msg_a`, ...,
  `kex_derive_ss_a`, `kex_derive_ss_b`
- randomness: `get_random_number(&drng_algorithm, ...)` from the ICCS
  deterministic generator `drng.c`, which is part of every package

`suites/ngcc/pqc_suite/suite.c` drives a candidate through that interface and
nothing else, so one file serves all of them. For each parameter set ("unit")
`tools/ngcc/ngcc.py` does the rest:

1. **Find the units.** A unit is a file that defines `<kind>_get_pk_len_bytes`,
   outside directories for optimized or additional implementations. Its
   directory is the parameter set; code of the reference tree that belongs to
   no parameter set is treated as shared.
2. **Choose the sources.** All C files of the unit and the shared code,
   without files that have a `main()`, without operating-system randomness and
   cycle counters. The package's own build scripts are never run. Each file is
   test-compiled for the host and for RV32; files that do not compile are left
   out and recorded.
3. **Keep names apart.** The candidate is compiled as its own static library
   with its own include paths, and a generated header renames every global
   symbol it defines (`#define name ngc_name`), so its `shake128`, `sha256` or
   `poly_add` cannot collide with this project's or the C library's. Only the
   uniform interface keeps its names. Being a library, only the objects that
   are actually needed get linked.
4. **Wrap the randomness.** Calls to `get_random_number` and `randombytes` go
   to the suite. For the known-answer test they reach the real ICCS generator;
   for the timed runs they reach the harness generator that every other suite
   of this project uses.
5. **Host first.** The unit is built for the build machine and run in a sandbox
   (no network, no file writes, no child processes). This gives the sizes, a
   first verdict and reference fingerprints.
6. **Device.** The same sources are built into a RAM image, loaded with
   `esptool load_ram`, and run. A job's stack is a share of the free heap (60 %
   at first; other splits are tried when a run overflows the stack or runs out
   of heap).

7. **The `opt` variant.** Most candidates do not bring their own hash but call
   the SM3-based helper functions that ICCS supplies with the KAT framework
   (`auxfunc.c`: `sm3hash`, `pseudohash`, `pseudoXOF`). For those the tool
   builds a copy of that file with one added line: when the optimized
   primitives are selected, its SM3 compression function calls
   `pqc_sm3_compress` from `components/prims` instead of running its own loop.
   The suite then runs everything twice, `ref` (the package as it is) and
   `opt` (same code, this project's SM3), and checks that both produce the
   same bytes. Candidates with their own Keccak or SM3 code have only `ref`.

What a run checks:

- **KAT**: seeded exactly like test vector 0 of the ICCS KAT program, keygen,
  encapsulation/signing and decapsulation/verification must succeed, and the
  fingerprints of the outputs are compared with the first vector of the
  package's own KAT file.
- **Round trip and tampering**: shared secrets agree; a corrupted ciphertext,
  message or signature is rejected.
- **Device against host**: the fingerprints of all outputs of the device run
  equal those of the host run.

Parameter-set names, claimed security levels and the `-D` flags that select a
parameter set come from the independent evaluation data in
<https://github.com/SYF19256/NGCC-Round1-Cryptographic-Algorithms-Evaluation>
(CC BY 4.0).

## Commands

```
python3 tools/ngcc/fetch.py                  download the packages
python3 tools/ngcc/unpack.py                 unpack them (checked, nothing is executed)
tools/ngcc/ngcc units [candidate]            list parameter sets
tools/ngcc/ngcc host <candidate> [param]     host build and sandboxed run
tools/ngcc/ngcc hostall [-j N] [--failed]    the same for everything
tools/ngcc/ngcc devbuild --lowest            build RAM images of each candidate's lowest level (in parallel)
tools/ngcc/ngcc devrun --missing             run them on the board, several per hold of the device lock
tools/ngcc/ngcc devbuild --retry             rebuild failed units with another stack/heap split
python3 tools/ngcc/report.py inventory       docs/ngcc/inventory.md
python3 tools/ngcc/report.py status          docs/ngcc/status.md
```

`devrun` uses the same device lock and the same `esptool load_ram` procedure
as `tools/pqc run`, but keeps the lock for several images in a row (at most
about five minutes), because one image runs for only a few seconds.

A single unit can also go through the project's normal tool:

```
tools/ngcc/ngcc gen kem_CheetahKEM Cheetah128
tools/pqc run ngcc -D NGCC_CAND=kem_CheetahKEM -D NGCC_PARAM=Cheetah128 --build-name ngcc_cheetah
```

## Adding or fixing a candidate

Most candidates need nothing. When one does, put a description file in
`tools/ngcc/cands/<candidate id>.json`:

```json
{
  "all": {
    "defs": ["SOME_MODE=1"],
    "exclude": ["*_avx2.c", "tests/*"],
    "extra_dirs": ["Implementations/common"],
    "incs": ["Implementations/Reference_Implementation/include"],
    "rename": {"their_random_bytes": "ngcc_rb_sz"},
    "copts": ["-fwrapv"],
    "rb_len": 32
  },
  "units": { "Param-128": { "defs": ["LEVEL=1"] } }
}
```

- `defs`, `copts`, `incs`: extra definitions, compiler options, include paths.
- `exclude`: file patterns to leave out; `extra_dirs`, `extra_srcs`: code
  outside the unit that it needs.
- `rename`: map a candidate's own random-byte function to `ngcc_rb_sz`
  (length is `size_t`) or `ngcc_rb_ull` (length is `unsigned long long`).
- `rb_len`: width of the length argument of the candidate's `randombytes`
  when the automatic detection is wrong.
- `stack`, `iters`, `host_iters`: fixed stack size and iteration counts.

`build/ngcc_gen/<unit>/info.json` shows what the tool decided for a unit:
sources, dropped files with the reason, definitions, and the source scan.
