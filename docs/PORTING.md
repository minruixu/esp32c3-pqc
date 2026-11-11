# How the project is organised, and how to add an algorithm

## Layout

```
CMakeLists.txt            ESP-IDF project; -D PQC_SUITE=<name> picks one suite
sdkconfig.ram             configuration of the RAM-only image
main/                     app_main: bench_begin(); suite_main(); bench_end();
components/bench/         harness: jobs, timing, stack and heap use, fingerprints
components/prims/         shared Keccak, SHA-2, SM3 with switchable backends
  opt/                    ESP32-C3 optimized backends (assembly, hardware drivers)
suites/<name>/pqc_suite/  one algorithm family = one image
third_party/<lib>/        upstream code whose license allows redistribution
downloads/                upstream code that is fetched, never committed
host/                     the same suites built for the build machine
tools/pqc                 build, load into RAM, run, collect, compare
results/<name>.json       device results; <name>.host.json host results
```

## Why one image per suite, and why from RAM

The test board keeps its own firmware. Each suite is built as a RAM-only
ESP-IDF application and loaded with `esptool load_ram`; flash is never
written, and a reset brings the original firmware back. The price is that
code, data, stacks and heap share about 385 KB, so each algorithm family gets
its own image. `@START` reports the free heap of an image; that is the budget
for task stacks and buffers.

## A suite

`suites/<name>/pqc_suite/` holds:

- `suite.cmake` sets `SUITE_SRCS`, `SUITE_INCS`, `SUITE_DEFS`, `SUITE_COPTS`
  and `SUITE_DEVICE_SRCS` (device-only files such as assembly). It is included
  by the device build and by the host build, so the two cannot drift apart.
- `CMakeLists.txt` registers the ESP-IDF component from those variables (copy
  the one in `suites/mlkem`).
- `suite.c` implements `suite_main()` with the calls in `bench.h`.
- `NOTES.md` records where the code came from (URL, commit, license), what
  was changed, which optimizations were tried and what they gained, and what
  does not work.
- `prims_off.txt` (optional) names optimized backends to leave out of this
  image, one per line: `keccak`, `sha256`, `sm3`. All backends are linked
  into every image (about 14 KB); a suite that is short of RAM drops the ones
  its algorithms never call.

`suites/mlkem` is the model to copy.

## Rules for suite code

- Job names are `<algorithm>/<variant>/<operation>`: `mlkem768/ref/keygen`.
  `ref` is the upstream portable code with the reference primitives. `opt` is
  the same algorithm with everything this project adds for the ESP32-C3.
- Call `pqc_prims_select(PQC_PRIMS_REF or PQC_PRIMS_OPT)` before the jobs of a
  variant. Route Keccak-f[1600], SHA-256 compression and SM3 compression
  through `pqc_prims.h` (directly, or through `fips202.h`, `sha2.h` and
  `pqc_sm3.h`), so that an optimized primitive speeds up every algorithm.
- Reseed with `bench_rng_seed()` before each job. Timings and outputs are then
  reproducible, and the device can be compared with the host.
- Print `bench_digest()` of every output (keys, ciphertexts, signatures,
  shared secrets) under the name of the variant that produced it. `tools/pqc
  check` compares device and host label by label; the host runs the reference
  code for both variants, so a wrong optimization shows up as a mismatch.
- Run every known-answer test the upstream project ships and report it with
  `bench_check()`.
- Keep large buffers `static`, not on the stack of `suite_main()` (8 KB).
  Give each job the stack it needs; the harness reports how much was used.
- Do not use `malloc` in new code. Upstream code that does is measured: the
  harness wraps `malloc` and reports the peak per job.

## Commands

```
tools/pqc run <suite>            build, load into RAM, run, save results/<suite>.json
tools/pqc run <suite> -f TEXT    only jobs whose name contains TEXT (comma-separated list)
tools/pqc run <suite> --check    also run on the host and compare fingerprints
tools/pqc host <suite>           host build and run only
tools/pqc run <suite> --merge    add to the saved results instead of replacing them
tools/pqc check <suite>          compare saved device and host results
tools/pqc size <suite>           flash and RAM footprint per library
tools/pqc md5                    checksum of the whole flash, to confirm nothing wrote to it
tools/pqc reset                  reset the board back into its own firmware
python3 tools/runall.py a b c    run several suites with --check and write a summary
```

Jobs that take minutes (reference key generation of the tall hash-based
trees, for example) are skipped in a plain run and say so in the results:
`"<job>/skipped": "run with -f <job>"`. Run them with that filter and
`--merge`.

The board is shared through a lock file, so several runs queue up instead of
colliding. A stack overflow or a crash is reported as "RUN DID NOT FINISH"
with the line that caused it; the full device log is in `results/logs/`.

## Upstream code

Code with a license that allows redistribution goes to `third_party/<lib>/`
with its license file and a `SOURCE.md` (URL, commit, date, local changes).
Everything else is fetched into `downloads/` by a script under `tools/` and is
never committed; the suite then builds from `downloads/`.
