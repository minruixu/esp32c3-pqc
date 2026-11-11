/* Benchmark and self-test harness. The same suite code runs on the ESP32-C3
 * (bench_esp.c) and on the build machine (host/bench_host.c), and prints
 * machine-readable lines starting with '@' that tools/pqc collects.
 *
 * Naming: "<algorithm>/<variant>/<operation>", for example
 * "mlkem768/ref/keygen". <variant> is "ref" for the portable reference code
 * and "opt" for the ESP32-C3 optimized code. */
#ifndef BENCH_H
#define BENCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*bench_fn)(void *arg); /* returns 0 on success */

/* Run fn(arg) `iters` times and report average/minimum/maximum time. On the
 * device every job gets a fresh task with `stack_bytes` of stack, so the
 * peak stack and heap use of the operation are reported as well. Returns 0
 * when every iteration succeeded, or when the job was filtered out. */
int bench_run(const char *name, bench_fn fn, void *arg, int iters, uint32_t stack_bytes);

/* 1 if `name` passes the filter given on the command line (see tools/pqc). */
int bench_selected(const char *name);

/* "alg/variant/op" in a static buffer; use it before the next call. */
const char *bench_name(const char *alg, const char *variant, const char *op);

/* Print a SHA-256 fingerprint of data. Device and host runs are compared
 * label by label, so equal labels must describe equal bytes. */
void bench_digest(const char *label, const void *data, size_t len);

/* Record the outcome of a self-test, for example a known-answer test. */
void bench_check(const char *label, int ok);

/* Free-form information: sizes, parameters, notes. */
void bench_info(const char *key, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* Deterministic random generator (xoshiro128**), the same stream on device
 * and host. randombytes() is what the algorithms call. Reseed before each
 * job so that timings and outputs are reproducible. Not for real keys. */
void bench_rng_seed(uint32_t seed);
int randombytes(uint8_t *out, size_t outlen);

/* Called by main; a suite only implements suite_main(). */
void bench_begin(void);
void bench_end(void);
void suite_main(void);

#ifdef __cplusplus
}
#endif
#endif /* BENCH_H */
