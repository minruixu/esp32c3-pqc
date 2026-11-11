/* Device side of the harness (ESP32-C3, ESP-IDF). */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "hal/usb_serial_jtag_ll.h"
#include "bench.h"
#include "bench_priv.h"

#ifndef PQC_SUITE_NAME
#define PQC_SUITE_NAME "?"
#endif

typedef struct {
    bench_fn fn;
    void *arg;
    int iters;
    uint32_t stack;
    TaskHandle_t waiter;
    int rc;
    int done;
    uint64_t us_sum;
    uint32_t us_min, us_max, stack_used;
} job_t;

/* ---------- heap use of the algorithms: malloc and friends are wrapped ---------- */
static size_t heap_cur, heap_peak;

void *__real_malloc(size_t n);
void *__real_calloc(size_t n, size_t m);
void *__real_realloc(void *p, size_t n);
void __real_free(void *p);

static void heap_add(void *p)
{
    if (p) {
        heap_cur += heap_caps_get_allocated_size(p);
        if (heap_cur > heap_peak) {
            heap_peak = heap_cur;
        }
    }
}

static void heap_sub(void *p)
{
    if (p) {
        size_t n = heap_caps_get_allocated_size(p);
        heap_cur = (heap_cur > n) ? heap_cur - n : 0;
    }
}

void *__wrap_malloc(size_t n)
{
    void *p = __real_malloc(n);
    heap_add(p);
    return p;
}

void *__wrap_calloc(size_t n, size_t m)
{
    void *p = __real_calloc(n, m);
    heap_add(p);
    return p;
}

void *__wrap_realloc(void *p, size_t n)
{
    void *q;
    heap_sub(p);
    q = __real_realloc(p, n);
    heap_add(q ? q : p);
    return q;
}

void __wrap_free(void *p)
{
    heap_sub(p);
    __real_free(p);
}

/* ---------- jobs ---------- */
static void job_task(void *param)
{
    job_t *j = param;

    for (int i = 0; i < j->iters; i++) {
        int64_t t0 = esp_timer_get_time();
        int rc = j->fn(j->arg);
        uint32_t dt = (uint32_t)(esp_timer_get_time() - t0);

        if (rc != 0) {
            j->rc = rc;
            break;
        }
        j->us_sum += dt;
        if (dt < j->us_min) {
            j->us_min = dt;
        }
        if (dt > j->us_max) {
            j->us_max = dt;
        }
        j->done++;
    }
    j->stack_used = j->stack - uxTaskGetStackHighWaterMark(NULL);
    xTaskNotifyGive(j->waiter);
    vTaskDelete(NULL);
}

int bench_run(const char *name, bench_fn fn, void *arg, int iters, uint32_t stack_bytes)
{
    job_t j;
    char nm[BENCH_NAME_MAX];
    size_t heap_base;

    if (!bench_selected(name)) {
        return 0;
    }
    snprintf(nm, sizeof(nm), "%s", name);
    memset(&j, 0, sizeof(j));
    j.fn = fn;
    j.arg = arg;
    j.iters = iters;
    j.stack = stack_bytes;
    j.us_min = UINT32_MAX;
    j.waiter = xTaskGetCurrentTaskHandle();

    heap_base = heap_cur;
    heap_peak = heap_cur;
    if (xTaskCreate(job_task, "job", stack_bytes, &j, 10, NULL) != pdPASS) {
        j.rc = -100; /* not enough free memory for this stack */
    } else {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(20)); /* the idle task frees the finished stack */
    }
    if (j.rc != 0 || j.done != j.iters) {
        bench_fails++;
    }
    bench_results++;
    printf("@RESULT|%s|%d|%lu|%lu|%lu|%lu|%lu|%d\n", nm, j.done,
           (unsigned long)(j.done ? j.us_sum / (uint64_t)j.done : 0),
           (unsigned long)(j.done ? j.us_min : 0), (unsigned long)j.us_max,
           (unsigned long)j.stack_used, (unsigned long)(heap_peak - heap_base), j.rc);
    fflush(stdout);
    return j.rc;
}

/* ---------- start and end of a run ---------- */
void bench_begin(void)
{
    char line[BENCH_FILTER_MAX];
    size_t n = 0;
    int64_t last = -1000000;

    /* Wait for "GO [filter]" from tools/pqc, so that no output is lost. */
    for (;;) {
        uint8_t ch;
        int64_t now = esp_timer_get_time();

        if (now - last >= 300000) {
            printf("@READY|%s\n", PQC_SUITE_NAME);
            fflush(stdout);
            last = now;
        }
        while (usb_serial_jtag_ll_read_rxfifo(&ch, 1) == 1) {
            if (ch == '\n' || ch == '\r') {
                line[n] = '\0';
                if (strncmp(line, "GO", 2) == 0) {
                    const char *f = line + 2;
                    while (*f == ' ') {
                        f++;
                    }
                    snprintf(bench_filter, BENCH_FILTER_MAX, "%s", f);
                    goto started;
                }
                n = 0;
            } else if (n < sizeof(line) - 1) {
                line[n++] = (char)ch;
            }
        }
        vTaskDelay(1);
    }
started:
    printf("@START|%s|%s|%lu|%lu\n", PQC_SUITE_NAME, bench_filter,
           (unsigned long)esp_rom_get_cpu_ticks_per_us(),
           (unsigned long)esp_get_free_heap_size());
    fflush(stdout);
}

void bench_end(void)
{
    for (;;) {
        printf("@DONE|%d|%d|%lu\n", bench_fails, bench_results,
               (unsigned long)esp_get_minimum_free_heap_size());
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
