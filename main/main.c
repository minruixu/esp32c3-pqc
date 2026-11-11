/* Post-quantum feasibility test for ESP32-C3:
 * ML-KEM-768 (FIPS 203) and ML-DSA-65 (FIPS 204).
 * Each operation runs in its own task so its peak stack use can be read. */
#include <inttypes.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "mldsa_native.h"
#include "mlkem_native.h"
#include "tests.h"

typedef struct {
    const char *name;
    int (*fn)(void);
    int iters;
    uint32_t stack;
    /* results */
    int rc;
    int done;
    int64_t us_sum, us_min, us_max;
    uint32_t stack_used;
    TaskHandle_t waiter;
} job_t;

#define KEM_STACK (24 * 1024)
#define DSA_STACK (84 * 1024)

#define JOB(n, f, i, s) {.name = n, .fn = f, .iters = i, .stack = s}
static job_t jobs[] = {
    JOB("ML-KEM-768 known-answer test", kem_kat,    1,  KEM_STACK),
    JOB("ML-KEM-768 keygen",            kem_keygen, 20, KEM_STACK),
    JOB("ML-KEM-768 encaps",            kem_encaps, 20, KEM_STACK),
    JOB("ML-KEM-768 decaps",            kem_decaps, 20, KEM_STACK),
    JOB("ML-DSA-65  known-answer test", dsa_kat,    1,  DSA_STACK),
    JOB("ML-DSA-65  keygen",            dsa_keygen, 10, DSA_STACK),
    JOB("ML-DSA-65  sign",              dsa_sign,   30, DSA_STACK),
    JOB("ML-DSA-65  verify",            dsa_verify, 10, DSA_STACK),
};
#define NJOBS (sizeof(jobs) / sizeof(jobs[0]))

static void job_task(void *arg)
{
    job_t *j = arg;
    j->us_sum = 0;
    j->us_min = INT64_MAX;
    j->us_max = 0;
    j->done = 0;
    for (int i = 0; i < j->iters; i++) {
        int64_t t0 = esp_timer_get_time();
        int rc = j->fn();
        int64_t dt = esp_timer_get_time() - t0;
        if (rc != 0) {
            j->rc = rc;
            break;
        }
        j->us_sum += dt;
        if (dt < j->us_min) j->us_min = dt;
        if (dt > j->us_max) j->us_max = dt;
        j->done++;
        vTaskDelay(1); /* let the idle task run; not counted in the timing */
    }
    j->stack_used = j->stack - uxTaskGetStackHighWaterMark(NULL);
    xTaskNotifyGive(j->waiter);
    vTaskDelete(NULL);
}

static void report(uint32_t heap_start)
{
    int fails = 0;
    printf("\n==== PQC on ESP32-C3 @ %" PRIu32 " MHz ====\n",
           (uint32_t)esp_rom_get_cpu_ticks_per_us());
    printf("ML-KEM-768: pk %d, sk %d, ct %d, ss %d bytes\n",
           MLKEM_PUBLICKEYBYTES(768), MLKEM_SECRETKEYBYTES(768),
           MLKEM_CIPHERTEXTBYTES(768), MLKEM_BYTES);
    printf("ML-DSA-65 : pk %d, sk %d, sig %d bytes%s\n",
           MLDSA_PUBLICKEYBYTES(65), MLDSA_SECRETKEYBYTES(65), MLDSA_BYTES(65),
#ifdef MLD_CONFIG_REDUCE_RAM
           "  [MLD_CONFIG_REDUCE_RAM]"
#else
           ""
#endif
    );
    printf("%-30s %5s %9s %9s %9s %9s  %s\n", "operation", "runs", "avg ms",
           "min ms", "max ms", "stack B", "result");
    for (size_t i = 0; i < NJOBS; i++) {
        job_t *j = &jobs[i];
        int ok = (j->rc == 0 && j->done == j->iters);
        if (!ok) fails++;
        double avg = j->done ? (double)j->us_sum / j->done / 1000.0 : 0;
        printf("%-30s %5d %9.2f %9.2f %9.2f %9" PRIu32 "  %s",
               j->name, j->done, avg, j->done ? j->us_min / 1000.0 : 0,
               j->us_max / 1000.0, j->stack_used, ok ? "PASS" : "FAIL");
        if (!ok) printf(" (rc=%d)", j->rc);
        printf("\n");
    }
    printf("build: %s\n",
#ifdef CONFIG_APP_BUILD_TYPE_RAM
           "RAM-loaded (flash untouched)"
#else
           "flash"
#endif
    );
    printf("heap: %" PRIu32 " B free at start, %" PRIu32 " B free now, %" PRIu32
           " B lowest ever\n",
           heap_start, esp_get_free_heap_size(),
           esp_get_minimum_free_heap_size());
    printf("PQC_DONE fails=%d\n", fails);
}

void app_main(void)
{
    uint32_t heap_start = esp_get_free_heap_size();
    for (size_t i = 0; i < NJOBS; i++) {
        job_t *j = &jobs[i];
        j->waiter = xTaskGetCurrentTaskHandle();
        printf("running %s ...\n", j->name);
        if (xTaskCreate(job_task, "pqc", j->stack, j, 5, NULL) != pdPASS) {
            j->rc = -100; /* could not allocate the task stack */
            continue;
        }
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(20)); /* idle task frees the finished stack */
    }
    for (;;) {
        report(heap_start);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
