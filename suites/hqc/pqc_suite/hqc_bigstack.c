/* Runs a job on a statically allocated stack (device only).
 *
 * The harness takes task stacks from the heap, and on the ESP32-C3 the heap
 * consists of regions of at most 128 KB. The reference code of HQC-3 needs
 * 132 KB of stack for decapsulation, more than any heap block. A static
 * array has no such limit, so the image "hqc3ref" runs it on the stack below
 * and prints the same @RESULT lines as bench_run().
 *
 * (The reference code of HQC-5 needs 211 KB; together with the firmware
 * that is more than the 321 KB a RAM-loaded image may occupy.) */
#include <stdio.h>
#include "bench.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef HQC_BIGSTACK_BYTES
#define HQC_BIGSTACK_BYTES 140000
#endif

typedef struct {
    bench_fn fn;
    void *arg;
    int iters, done, rc;
    uint64_t us_sum;
    uint32_t us_min, us_max, stack_used;
    TaskHandle_t waiter;
} big_job_t;

static StackType_t big_stack[HQC_BIGSTACK_BYTES];
static StaticTask_t big_tcb;

static void big_task(void *param)
{
    big_job_t *j = param;

    for (int i = 0; i < j->iters; i++) {
        int64_t t0 = esp_timer_get_time();
        int rc = j->fn(j->arg);
        uint32_t dt = (uint32_t)(esp_timer_get_time() - t0);

        if (rc != 0) {
            j->rc = rc;
            break;
        }
        j->us_sum += dt;
        if (dt < j->us_min) j->us_min = dt;
        if (dt > j->us_max) j->us_max = dt;
        j->done++;
    }
    j->stack_used = sizeof(big_stack) - uxTaskGetStackHighWaterMark(NULL);
    xTaskNotifyGive(j->waiter);
    vTaskDelete(NULL);
}

int hqc_run_bigstack(const char *name, bench_fn fn, void *arg, int iters)
{
    big_job_t j = {.fn = fn, .arg = arg, .iters = iters, .us_min = UINT32_MAX};
    char nm[72];

    if (!bench_selected(name)) {
        return 0;
    }
    snprintf(nm, sizeof(nm), "%s", name);
    j.waiter = xTaskGetCurrentTaskHandle();
    xTaskCreateStatic(big_task, "big", sizeof(big_stack), &j, 10, big_stack, &big_tcb);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(50)); /* the idle task retires the finished task */
    printf("@RESULT|%s|%d|%lu|%lu|%lu|%lu|0|%d\n", nm, j.done,
           (unsigned long)(j.done ? j.us_sum / (uint64_t)j.done : 0),
           (unsigned long)(j.done ? j.us_min : 0), (unsigned long)j.us_max,
           (unsigned long)j.stack_used, j.rc);
    fflush(stdout);
    return j.rc ? j.rc : (j.done != iters);
}
