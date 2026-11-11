# slhdsa: SLH-DSA (FIPS 205), all 12 parameter sets

## Sources

- SLH-DSA: slhdsa-c, https://github.com/pq-code-package/slhdsa-c, commit
  47451432bb04 (2026-11-02), Apache-2.0 OR ISC OR MIT. Copied to
  `third_party/slhdsa-c`; the local changes are listed in its `SOURCE.md`.
  One code base serves all 12 parameter sets, so they fit one image.
- Known-answer tests: NIST ACVP sample vectors (usnistgov/ACVP-Server
  v1.1.0.40). `gen_kat.py` downloads them into `downloads/acvp/` and writes
  `pqc_suite/kat_vectors.h`, which is committed. Signatures are 8 to 50 KB
  each, so expected signatures are stored as their SHA-256.
- `pqc_suite/slh_sha2_c3.c`, `slh_sha2_c3_tmpl.h`, `slh_shake_fast.c`,
  `slh_glue.c`, `suite.c`, `hosttest/`: written for this project.

Only pure SLH-DSA is built (internal and external interfaces). The pre-hash
variants (`slh_prehash.c`) are not.

## What is tested

For every parameter set and for both variants:

- key generation from the seeds of an ACVP keyGen vector, compared with the
  expected public and private key;
- deterministic signing of an ACVP sigGen vector (external interface, pure,
  with a context), compared with the expected signature;
- verification of that signature, and rejection of seven changed inputs (a
  flipped bit in R, in the FORS part, in the middle and at the end of the
  hypertree part, a different message, a different context, a signature one
  byte short);
- for the six "f" sets, hedged signing of a second ACVP vector (internal
  interface, with additional randomness), compared and verified;
- two ACVP sigVer vectors stored in full: a valid signature for
  SLH-DSA-SHA2-128s, and for SLH-DSA-SHAKE-128s one with a modified
  hypertree part that must be rejected. (Two more were dropped to make the
  image fit: see the note on memory below.)

The timed operations are these tests themselves. `tools/pqc check slhdsa`
compares the fingerprints of all keys and signatures with a host run of the
unmodified slhdsa-c code.

`hosttest/run.sh` builds the suite for the build machine with the ESP32-C3
code paths enabled and a register-level model of the SHA accelerator in
place of the real one. It runs all the tests above in a few seconds, without
the board. The model has the two properties the code relies on (registers
keep their contents; a start compresses the block as it is at that moment),
and `suites/hashes` checks both on the real chip.

## Results

ESP32-C3 revision v1.1, 160 MHz, ESP-IDF v5.5.3, `-O2`, code in RAM.
`ref` is slhdsa-c as it comes, with its own portable C hash functions. `opt`
is described below. Times are milliseconds; stack is the peak of the task
that ran the operation, in bytes. Nothing uses the heap.

RESULTS_TABLE

RESULTS_REMARKS

## The reference variant

`slh_glue.c` puts the project's backend switch in front of slhdsa-c's
SHA-256 compression, SHA-512 compression and Keccak permutation. With the
reference backend selected it calls slhdsa-c's own C functions, so `ref` is
the upstream code and nothing else. (Its primitives are a little faster on
this chip than PQClean's in `components/prims`: see `suites/hashes`.)

## The optimized variant, SHA-2 sets

Every F, PRF, H and T_l is SHA-256(PK.seed block || ADRSc || data), and for
F and PRF that is one compression from a chaining value that is fixed for
the key. The ESP32-C3 SHA accelerator does a compression in about 80 CPU
cycles, but each register access costs 8 to 9, and a naive call makes 32 of
them. `slh_sha2_c3.c` is built around making fewer:

1. **No byte swaps, no copies.** The accelerator keeps the chaining value in
   the byte order of the standard and takes the block in memory order. The
   stored midstate goes straight to the H registers and the result is read
   straight into the next block.
2. **Write only what changed.** The message registers keep their contents.
   Inside a WOTS+ chain with n = 16, five of the sixteen words change from
   one step to the next. The address words, the zero padding and the length
   are tracked in a copy in RAM and written only when they differ.
3. **Read only n bytes** of the result: 4, 6 or 8 words.
4. **Two hashes in flight.** The accelerator copies the block when it
   starts, so while it compresses one block the next can be written. The
   next block of a chain is not known until its previous step is read back,
   so two chains are advanced in turn: while chain a is compressed, the
   block of chain b is written, and the other way round. The same is done
   for the two leaves of a FORS pair and, in verification, for the chains
   of a WOTS+ signature (two slots, refilled as chains end). The 67, 51 or
   35 PRF calls that start the chains of one WOTS+ key differ in one
   register only.
5. **SHA-512 in assembly.** For n = 24 and 32 the standard uses SHA-512 for
   H and T_l. The accelerator cannot do it, and once F and PRF are on the
   accelerator SHA-512 is most of what is left (three quarters of a
   SLH-DSA-SHA2-256s signature is the SHA-512 of FORS tree nodes).
   `components/prims/opt/sha512_rv32imc.S` is generated by `sha512_gen.py`
   next to it: all sixteen halves of the working variables stay in
   registers, no load is used by the instruction after it, 11,042
   instructions per block, 69 microseconds instead of 85 for slhdsa-c's C.
   H builds its block from words, keeps padding and length in place between
   calls, and starts from a midstate stored as native words, so nothing is
   byte-swapped twice.

Item 4 needs to see more than one hash at a time, which slhdsa-c's
per-hash function pointers do not allow. Three optional members were added
to its parameter structure (`wots_pkgen`, `fors_leaf2`, `wots_chains`), and
three loops in `slh_dsa.c` call them when they are set. slhdsa-c's own
parameter sets leave them unset.

Measured steps, SLH-DSA-SHA2-128f on the board:

STEPS_TABLE

## The optimized variant, SHAKE sets

There is no hardware for Keccak on this chip. The `opt` variant uses the
project's optimized Keccak-f[1600] (`components/prims/opt`, not part of this
suite) and `slh_shake_fast.c`, which writes the 25 lanes of the state
directly for F, PRF, H and the chain steps instead of going through
slhdsa-c's incremental SHA-3 interface. Every one of these hashes is a
single permutation.

SHAKE_REMARKS

## What did not work, or was left out

- Reading the accelerator's result without waiting: always wrong.
- SHA-512 rewritten in C with 32-bit halves: 83.2 microseconds per block
  against 84.8 for slhdsa-c's C and 81.0 for PQClean's. The compiler spills
  about ten values per round, and a load used by the next instruction stalls
  this CPU. Dropped for the assembly version.
- PQClean's SHA-512 for the optimized variant: 4 % faster than slhdsa-c's,
  but it is 42 KB of straight-line code and the image no longer fits in RAM.
- SHA-512 variants specialised for the block of H (known zero words, two
  rounds that are the same for a whole FORS instance): would save 4 to 6 %
  of each H, but each variant is another 13 KB and the image has no room.
- Interleaving the tree nodes (H) the way the leaves are: not done. With
  n = 16 they are under 5 % of the hashes; with n = 24 or 32 they are
  SHA-512 and never reach the accelerator.
- A lower-stack variant: not needed, the peak is 4 to 7 KB.
- Pre-hash SLH-DSA: not built.

## Memory of the image

All 12 parameter sets, both variants and the test vectors are one RAM image:
about 238 KB of code and constants plus 55 KB of buffers (50 KB of that is
the signature buffer for SLH-DSA-*-256f). That leaves 28 KB of the main RAM
segment free. An earlier build that left only 10 KB there crashed during
start-up, before any output (twice, with two different code sets); with
20 KB it ran. To get back above that, two of four fully stored ACVP sigVer
vectors (8 KB each) were dropped. If the image grows again, split it into a
SHA-2 and a SHAKE suite.

## Timing and secret data

Nothing added here branches on secret data or indexes memory with it. The
register writes that are skipped when a word is unchanged concern the
address words, the padding and the length, which are public. The two-slot
scheduler of the verification chains follows the digits of the message
digest, which are public. The accelerator's busy time was the same in every
measurement (7 polls). The SHA-512 assembly and the SM3 code are straight
lines. Whether the accelerator's power consumption depends on the data was
not looked at.

## Using this outside the benchmark

- One key at a time, one task at a time: the midstate, the copy of the
  message registers and the accelerator itself are single instances. Nothing
  takes a lock. A program that also uses the SHA accelerator elsewhere must
  hold ESP-IDF's SHA lock around each SLH-DSA call.
- `slh_c3_wipe()` erases the words derived from SK.seed that
  `slh_sha2_c3.c` keeps between calls. Call it after signing.
- The timings were taken with the code in RAM. In a flash build, put
  `slh_sha2_c3.c` and `sha512_rv32.c` in RAM as well (IRAM) or expect cache
  misses to add to them; this was not measured.
