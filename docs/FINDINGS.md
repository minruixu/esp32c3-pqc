# Findings

What this project learned that is not a number in a table. The per-suite
detail is in each `suites/<name>/NOTES.md`; the order in which things were
found is in the `optimization-journal` branch.

## Where the speed came from

- **The SHA-256 accelerator.** The ESP32-C3 has no SHA-3, SHA-512 or SM3
  hardware, but it has SHA-256, and hash-based signatures are almost nothing
  but SHA-256 calls on short inputs. One compression costs about 4400 cycles
  in portable C and 630 through the accelerator; with the state kept in the
  peripheral, only the changed message words written and two hashes in
  flight, SLH-DSA spends about 195 to 280 cycles per hash. SLH-DSA-SHA2-128
  got 17 to 23 times faster, XMSS with SHA-256 14 to 23 times, and LMS with
  SHA-256 4 to 27 times depending on the Winternitz parameter.
  What was learned about the peripheral: a register access costs 8 to 9
  cycles, starting and waiting 88; it copies the block when started, so the
  next block can be written while the previous one is compressed; it accepts
  partial updates of the message registers; reading the result without
  waiting gives wrong data every time.
- **Keccak-f[1600] in RV32IMC assembly.** 135 µs per permutation in C, 79 µs
  in assembly. Everything built on SHAKE or SHA-3 gains from it: ML-KEM and
  ML-DSA most (together with assembly polynomial arithmetic, 1.8 to 2.1
  times), the SHAKE variants of LMS and XMSS 2 times, SLH-DSA-SHAKE 1.5
  times, HQC and FN-DSA verification about 1.25 times.
- **Choosing a different algorithm for the same result.** HQC's reference
  multiplies two dense polynomials with Karatsuba on 64-bit words; one operand
  is always sparse, and with a shift-and-XOR product the operations of HQC-1
  and HQC-3 are 51 to 82 times faster on this core and need a third of the
  stack. See the caveat below.
- **Arithmetic that fits the core.** FN-DSA has no FPU to use; redoing the
  emulated double-precision addition and multiplication on 32-bit halves made
  signing 1.26 times faster (1.32 together with the assembly Keccak). The
  hardware AES accelerator, together with packed matrix products, made the
  AES family of SCloud+ 6.6 to 9.4 times faster, and LAC 1.2 to 2.2 times.
- **SHA-512 stays slow.** SLH-DSA at the 192 and 256 levels uses SHA-512 for
  part of its hashing. Assembly brought it from 81 to 69 µs per block; there
  is no hardware for it, so those sets gain 3.4 to 12 times instead of 20.

## What did not work

Recorded with numbers in the journal branch; the ones worth knowing:

- `-O3` on FN-DSA: the image no longer fits in RAM, and with `-O3` on the
  signing unit alone it crashed at start-up.
- Routing GmSSL's SM3 through the project's fast SM3: 6.6% slower. GmSSL's own
  unrolled compression is faster on this chip than the project's.
- SHA-512 in C on 32-bit halves: no gain over the generic code.
- An optimized primitive that is never linked. The optimized backends replace
  weak defaults that live in the same static library, and a linker does not
  pull an object out of an archive only to override a weak symbol. For a while
  most suites reported `opt` equal to `ref` for that reason. The library is
  now linked whole.

## Limits of the chip

- A RAM-only image may occupy 321 KB statically; code, data, stacks and heap
  share about 385 KB.
- The heap is split into regions of at most 128 KB, so a single task stack
  can be about 112 KB. Algorithms whose reference code needs more (ML-DSA-87
  from PQMagic, HQC-3) run on a static region or in an image of their own.
- An image that leaves only about 10 KB of the main RAM segment free can
  crash at boot without any output; 20 KB was enough.
- Not feasible in RAM at all: SCloud+ 384 and 512, SPHINCS-alpha 192s and
  256, ReSolveD-alpha, the HQC-5 reference code, GmSSL's SPHINCS+ signing,
  and the NGCC candidates with keys of hundreds of kilobytes.
- Slow to the point of being verification-only: SLH-DSA-SHAKE signing with
  the "s" sets takes three to five minutes even optimized.

## Problems found in upstream code

- **HQC reference v5.1.0:** a bit mask is computed with `1UL`, which is 32
  bits on this target, and HQC-5 shifts by 37. Keys and ciphertexts of HQC-5
  are wrong on any 32-bit platform. Overridden locally.
- **Aigis-Sig+ reference:** signing writes 2 KB past a vector through a stray
  unpack call, and an `int` is read through an `int32_t *`, which is a
  different type on this toolchain; under strict aliasing the compiler dropped
  a store and the code crashed.
- **Aigis-Enc+ reference:** decapsulation outputs the shared secret before the
  re-encryption check and never replaces it.
- **LAC:** the BCH decoder reads and writes one element past an array.
- **SCloud+:** the SM3 helper returns wrong hashes when `malloc` fails, with
  only a message on stderr; on a small device that means wrong keys without
  an error.
- **NGCC packages:** six candidates that pass on the host fail on the chip
  (BAG-Loong, NSS-HQC, TRIKE, Facto-DSA, TRINE, Chinith), most likely 32-bit
  bugs in their reference code; four use 128-bit integer types.

## Before using any of this for real

- **Constant time is not guaranteed by the source.** GCC 14.2 for RV32IMC
  compiled masked selects that were written to be constant-time into
  conditional branches on secret operands, in upstream code as well as in new
  code. The FN-DSA floating-point routines written here use value barriers
  and were checked in the object code; nothing else was audited at that
  level.
- **The fast HQC product leaks on other hardware.** It runs a fixed
  instruction sequence, but secret positions select memory addresses and shift
  counts. On the ESP32-C3, which has no data cache in front of its RAM, its
  cycle count was measured to be identical for all tested supports. On a CPU
  with a cache, or against power analysis, it leaks where the reference does
  not.
- **No protection against power or electromagnetic analysis** anywhere.
- **Random numbers.** The benchmarks use a seeded generator so that runs are
  reproducible. Real keys need the hardware generator, which is a true random
  source only while Wi-Fi or Bluetooth is on or the bootloader entropy source
  is enabled.
- **The SHA-256 accelerator is used without a lock.** Fine in a benchmark
  with one task; a real application that also uses it through ESP-IDF must
  share it properly.
- **Stateful signatures (LMS, XMSS) must never reuse a state.** The suites
  keep the state in RAM; a product has to store it safely across resets.
- **Everything was measured running from RAM.** A flash build compiles
  (`sdkconfig.flash`) but was never run, because the test board is not
  flashed; code executing from flash through the cache may be slower.
- **The SM3 instantiations and the NGCC candidates are not standards.**
