/* Host side of the harness: runs a suite on the build machine so that the
 * device's outputs can be compared with a second, independent build. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "bench.h"
#include "bench_priv.h"

#ifndef PQC_SUITE_NAME
#define PQC_SUITE_NAME "?"
#endif

static uint64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

int bench_run(const char *name, bench_fn fn, void *arg, int iters, uint32_t stack_bytes)
{
    char nm[BENCH_NAME_MAX];
    uint64_t sum = 0, mn = UINT64_MAX, mx = 0;
    int done = 0, rc = 0;

    (void)stack_bytes;
    if (!bench_selected(name)) {
        return 0;
    }
    snprintf(nm, sizeof(nm), "%s", name);
    for (int i = 0; i < iters; i++) {
        uint64_t t0 = now_us(), dt;
        rc = fn(arg);
        dt = now_us() - t0;
        if (rc != 0) {
            break;
        }
        sum += dt;
        if (dt < mn) {
            mn = dt;
        }
        if (dt > mx) {
            mx = dt;
        }
        done++;
    }
    if (rc != 0 || done != iters) {
        bench_fails++;
    }
    bench_results++;
    printf("@RESULT|%s|%d|%llu|%llu|%llu|0|0|%d\n", nm, done,
           (unsigned long long)(done ? sum / (uint64_t)done : 0),
           (unsigned long long)(done ? mn : 0), (unsigned long long)mx, rc);
    return rc;
}

void bench_begin(void)
{
    printf("@START|%s|%s|0|0\n", PQC_SUITE_NAME, bench_filter);
}

void bench_end(void)
{
    printf("@DONE|%d|%d|0\n", bench_fails, bench_results);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        snprintf(bench_filter, BENCH_FILTER_MAX, "%s", argv[1]);
    }
    bench_begin();
    suite_main();
    bench_end();
    return bench_fails ? 1 : 0;
}
