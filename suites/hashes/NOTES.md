# hashes: the hash primitives, timed and tested

This suite times each hash primitive the project uses, checks the optimized
versions against the reference ones, and probes how the ESP32-C3 SHA
accelerator behaves. The numbers are from the board (ESP32-C3 revision v1.1,
160 MHz, ESP-IDF v5.5.3, `-O2`, code in RAM). The attempts behind them,
including the rejected ones, are in `journal/experiments/hashes.jsonl`.

## Sources

- `suite.c`, `hw_probe.c`: written for this project.
- Reference SHA-256, SHA-512 and Keccak: PQClean `common/` as found in
  `components/prims` (public domain). Reference SM3: `components/prims/sm3.c`.
- "slhdsa-c" rows: the portable C of `third_party/slhdsa-c` (Apache-2.0 OR
  ISC OR MIT), measured because it is the baseline of the SLH-DSA suite.
- `components/prims/opt/sha256_hw.c`, `sha256_hw.h`, `sm3_fast.c`,
  `sha512_gen.py` and the `sha512_rv32imc.S` it generates: written for this
  project.
- The optimized Keccak-f[1600] is not part of this line of work; it is
  measured here only because SLH-DSA with SHAKE depends on it.

## Results

| Primitive | ref us | ref cycles | opt us | opt cycles | Speedup |
|---|---:|---:|---:|---:|---:|
| SHA-256, one block per call | 27.64 | 4422 | 3.94 | 630 | 7.0x |
| SHA-256, 16 blocks per call | 24.43 | 3909 | 1.36 | 218 | 18.0x |
| SHA-256 of a 55-byte message | 36.69 | 5870 | 9.44 | 1510 | 3.9x |
| SM3, one block | 39.06 | 6249 | 26.34 | 4215 | 1.5x |
| SM3 of a 55-byte message | 41.16 | 6585 | 28.43 | 4550 | 1.4x |
| SHA-512, one block | 80.80 | 12928 | 69.16 | 11066 | 1.2x |
| SHA-512, one block given as bytes | 80.80 | 12928 | 72.65 | 11624 | 1.1x |
| Keccak-f[1600] | 134.60 | 21536 | 78.93 | 12629 | 1.7x |

| Other measurements | us | cycles |
|---|---:|---:|
| SHA-256, slhdsa-c's portable C, one block | 26.21 | 4194 |
| SHA-512, slhdsa-c's portable C, one block | 85.29 | 13646 |
| Keccak-f[1600], slhdsa-c's portable C | 118.45 | 18953 |
| SHA-256 of a 55-byte message with pqc_sha256_hw_short() | 3.79 | 606 |
| SHA-256 accelerator used directly: load state, one block, read 32 bytes | 2.46 | 394 |
| SHA-256 accelerator used directly: load state, one block, read 16 bytes | 2.28 | 364 |

One job runs the primitive 1000 times (200 for SHA-512 and Keccak);
microseconds are the job time divided by that count, cycles are microseconds
times 160. `tools/pqc run hashes` reproduces them, `suites/hashes/make_table.py`
prints these tables from `results/hashes.json`.

## SHA-256 on the accelerator

What `hw_probe.c` measured, in CPU cycles:

| Step | Cycles |
|---|---:|
| Write the 8 words of the chaining value, straight-line code | 68 |
| Write the 16 words of a block, straight-line code | 134 |
| The same 16 writes in a loop | 226 |
| Start and wait until idle (7 polls of the busy flag) | 88 |
| Read 8 words of the result | 62 |

The compression itself is the cheap part. One register access costs 8 to 9
cycles, and a block needs 16 writes for the message, 8 for the chaining value
and 8 reads for the result. The optimized code is organised around making
fewer accesses:

- `pqc_sha256_compress_opt()` loads the chaining value once per call and
  reads it once, whatever the number of blocks. If the caller's chaining
  value is the SHA-256 initial value, the accelerator loads it itself
  (`SHA_START`).
- The message block goes to the registers as it is: the accelerator takes
  the bytes in memory order. Only the chaining value is byte-swapped, inline.
  The compiler's `__builtin_bswap32` is a library call on RV32IMC; that alone
  cost 430 cycles per call in the first version.
- The accelerator takes its copy of the message registers when it starts:
  the probe overwrites message words right after the start and still gets
  the right result, 2000 times out of 2000. So the next block is written
  while the previous one is compressed, and a multi-block call hardly waits.
- The message registers keep their contents after a compression, and single
  words can be replaced (both checked by the probe). Code that hashes many
  short messages which share most of their block can use `sha256_hw.h`
  directly and write only what changes; `suites/slhdsa` does.
- `pqc_sha256_hw_short()` hashes a message of at most 55 bytes (one block
  with its padding) without a hash context. Through `sha256()` with the
  optimized backend such a message costs 9.5 microseconds, of which the
  accelerator is under 3: the rest is the byte-wise bookkeeping of the hash
  interface in `components/prims/sha2.c`. The helper does the same in
  3.8 microseconds. This is the shape of a step of an LMS chain.

What did not help, or was not done:

- Reading the result without waiting for the busy flag: wrong every time
  (0 of 2000).
- DMA mode: its set-up takes more register writes than the 16 message words
  it would save, for the short inputs that matter here. Not implemented.
- A lock: none is taken. A program that also reaches the accelerator from
  other tasks (Mbed TLS does) must hold ESP-IDF's SHA lock around the code
  that uses these functions.

## SM3

`pqc_sm3_compress_opt()` (`sm3_fast.c`) is portable C shaped for RV32IMC:
the 64 rounds unrolled with the eight state words rotating through the macro
arguments instead of being moved, round constants stored already rotated,
`FF` and `GG` in their four- and three-operation forms, message expansion
unrolled four words at a time. The compiled round is 40 instructions, which
is what a round needs on a CPU without a rotate instruction: a rotation is
three instructions and a round has seven. Assembly was not tried for that
reason; a few loads are all that is left to remove. The code is 9.4 KB
against 1.5 KB for the reference.

It is checked against the reference on 3000 random chaining values with 1 to
5 blocks at all four alignments, and both backends pass the two examples of
GB/T 32905-2016.

## SHA-512

The accelerator has no SHA-512. `sha512_rv32imc.S` is generated by
`sha512_gen.py`, which also runs the generated instructions in a small RV32
interpreter against Python's SHA-512 (`python3 sha512_gen.py --test`), so the
code was right before it first ran on the board.

A 64-bit value is two registers on this CPU, and SHA-512 has eight working
variables. With the cached `a ^ b` of the Maj trick and the message word
being built, that is 20 registers that should never go to memory. Compiled C
spills about ten values per round, and a load that is used by the very next
instruction stalls this CPU. The assembly keeps everything in registers and
places every load at least two instructions before its use: 11,042
instructions per block, 4 of them loads used immediately.

Interface: `pqc_sha512_compress_rv32(state, block)`, both as (high, low)
pairs of native 32-bit words. A caller that has its block as bytes converts
it first (the row "from bytes" in the table); a caller that builds its block
from words, like H in SLH-DSA, skips that for the constant parts.

Tried before the assembly, and dropped:

- The same algorithm in C with explicit 32-bit halves: 83.2 microseconds per
  block, between slhdsa-c's C (84.8) and PQClean's (81.0). No gain.
- PQClean's SHA-512 is the fastest C of the three, but it is 42 KB of
  straight-line code; the assembly is 14 KB.

It is checked on the board against PQClean's C on 500 random chaining values
with 1 to 3 blocks. It is not behind `pqc_prims.h`, which has no SHA-512
entry point: `components/prims/sha2.c` would have to call it.

## Timing and secret data

All three optimized primitives are straight-line code: no branch and no
memory address depends on the data. The accelerator's busy time was 7 polls
in every measurement.