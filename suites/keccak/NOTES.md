# Keccak-f[1600] on the ESP32-C3, and what the core costs

This suite is a micro-benchmark. It times five Keccak-f[1600] implementations
on the device, checks the optimized one against the reference, and measures
the cost of single instructions and of short instruction patterns. The
assembly in this project is tuned for the numbers in the second half of this
file.

## Keccak-f[1600]

Cycles per permutation at 160 MHz, state on the caller's stack, measured with
the cycle counter over 1000 permutations (`keccakf1600/<name>/x1000`):

| Implementation | Cycles | Microseconds | Relative |
|---|---:|---:|---:|
| PQClean C (`ref`, the project's reference primitive) | 21,540 | 134.6 | 1.00 |
| mlkem-native C (`mlkn`) | 21,542 | 134.6 | 1.00 |
| PQRV RV32IM assembly (`pqrv`) | 14,941 | 93.4 | 1.44 |
| PQRV RV32IM assembly, unrolled (`pqrv_unroll`) | 15,157 | 94.8 | 1.42 |
| This project (`opt`, `components/prims/opt/keccakf1600_rv32imc.S`) | 12,631 | 78.9 | 1.71 |

All five reach the same state after 5000 permutations, and `opt` equals `ref`
on 2000 random states and on a chain of 2000 permutations
(`keccakf1600/opt/equals_ref`).

The PQRV code is listed for comparison only. It uses `gp` and `tp` as scratch
registers. ESP-IDF addresses some data through `gp`, also inside interrupt
handlers, so code that changes `gp` is only safe with interrupts masked; the
benchmark masks them around the PQRV calls.

### How `opt` is built

`components/prims/opt/keccak_gen.py` generates the assembly and, before it
prints anything, runs it in a small interpreter against a plain Python
Keccak-f[1600]. The design:

- Standard lane layout: the function is a drop-in `void f(uint64_t state[25])`.
- No `gp`, no `tp`: 27 registers besides the state pointer.
- A round reads one copy of the state and writes the other (the caller's
  buffer and a copy on the stack, alternating). Every state word is loaded
  once and stored once per round, and no lane is ever moved.
- The column parities that theta of the next round needs are collected while
  the outputs of chi are still in registers. Five of the ten 32-bit parity
  words have a register; the other five are recomputed from memory. A
  register more would save 5 instructions per round, a register less costs 5.
- Six lanes are kept complemented (the pattern known from XKCP). RV32I has no
  and-not instruction, so plain chi costs 15 instructions per half row; with
  the complemented lanes it costs 11. The generator derives the instruction
  for every output from the complement flags instead of copying a table, and
  shares one inverted copy of an input per half row where two outputs need it.
- The round constants come from a table through a pointer kept on the stack,
  because no register is left for it.
- The straight-line code of each round goes through a scheduler that reorders
  independent instructions so that no instruction uses a register loaded by
  the one before it (see below: that costs a cycle).

Result: 515 and 517 instructions for the two round bodies, 12,571 instructions
per permutation, 12,631 measured cycles, 256 bytes of stack.

### What was tried

| Step | Instructions per permutation | Cycles |
|---|---:|---:|
| First working version (one NOT per chi output where the flags of both operands agree) | 12,715 | not measured |
| Shared inverted copy of one chi input per half row | 12,571 | 12,631 |

A search over other sets of complemented lanes (local search from 300 random
starting sets) found nothing cheaper than the XKCP set: 55 instructions of
chi per half round, one NOT per row.

Not done: folding the complementing of the six lanes into the first and last
round (about 70 instructions per permutation), and a version with the lanes
bit-interleaved, which would save about 20 instructions per round but changes
the layout of the state, so every caller would have to convert.

### Linking

`pqc_keccakf1600_opt` has a weak default in `components/prims/prims.c`. The
linker takes a member out of a static library only to resolve an undefined
symbol, and the weak default already resolves this one, so the assembly file
would be left out. `components/prims` is linked as a whole archive for that
reason. (Before that, the suites here forced the file in with
`-u pqc_keccakf1600_rv32imc_present`; the symbol still exists.)

## The cost of instructions on this core

Measured with the cycle counter (`cpu/model/*`), 5000 iterations of blocks of
16 identical instructions, the empty loop subtracted, data on the task stack:

| Instruction | Cycles |
|---|---:|
| add, xor, and, or, shifts (constant or register amount) | 1 |
| 16-bit (compressed) encodings; 32-bit encodings at odd half-word addresses | 1 |
| `mul` | 1 |
| `mulh`, `mulhu` | 5 |
| `divu`, `remu` | 23 to 35, depending on the operands |
| load or store (word, half word, byte) | 1 |
| load, when the next instruction uses the loaded value | 2 |
| load, when a later instruction uses the value | 1 |
| store followed by a load of the same word | 1 + 1 |
| branch not taken | 1 |
| branch taken | 3 |
| jump | 2 |
| call and return | 4 |

What follows from it:

- `mul` and `mulh` take the same time for every operand pair tried (zero,
  small, large, negative): they can be used on secret data. Division cannot.
- `mulh` is five times as expensive as `mul`. Modular multiplication should be
  written with `mul` alone where the modulus allows it (ML-KEM: Plantard
  arithmetic with `mul` and `srai`), and with as few `mulh` as possible where
  it does not (ML-DSA: one per butterfly, Barrett multiplication with a
  precomputed quotient, instead of two for Montgomery).
- A load must not be followed directly by the instruction that uses it.
- Alignment and instruction width do not matter for speed.
- Taken branches are expensive enough to justify unrolling short loops.

The C library in ROM: `memcpy` of 168 bytes takes 136 cycles when source and
destination are word aligned and 1,207 cycles when they are misaligned
relative to each other; `memset` of 200 bytes takes 141 cycles.

### Static data is slower in a RAM-loaded image

The same measurements with the data in a static buffer instead of on the
stack: loads and stores take 1.06 to 1.34 cycles on average instead of 1.
The internal memory of the ESP32-C3 consists of three blocks of 128 KB. In
the small image of this suite the code and the static data sit in the first
block, while task stacks are allocated from the heap in the second.
Instruction fetch and data access seem to get in each other's way when they
go to the same block. This matters for constant tables
(round constants, twiddle factors), which a RAM-loaded image keeps next to
the code: the measured cost of the assembly routines is a few percent above
the instruction count for that reason. In a firmware that runs from flash the
layout is different, and this was not measured.

## Files

- `suite.c`: the benchmark, the check, the cost model driver.
- `cpumodel.S`: the instruction patterns.
- `mlkn_keccak.c`: mlkem-native's C permutation built on its own.
- `pqrv_keccak_loop.S`, `pqrv_keccak_unroll.S`: wrappers around
  `third_party/pqrv/sha3/`.
