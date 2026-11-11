/* Runs one function on a large static stack (device only).
 *
 * GmSSL's SPHINCS+ keeps a whole FORS tree (131 KB) in a local variable
 * while signing. The harness allocates job stacks from the heap, where the
 * largest block is smaller than that, so these jobs run in a task whose
 * stack is a static array instead. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gmssl_port.h"

#define BIG_STACK_BYTES (138 * 1024)

static StackType_t big_stack[BIG_STACK_BYTES];
static StaticTask_t big_tcb;

typedef struct {
    int (*fn)(void *);
    void *arg;
    int rc;
    unsigned used;
    TaskHandle_t waiter;
} big_job_t;

static void big_task(void *param)
{
    big_job_t *j = param;

    j->rc = j->fn(j->arg);
    j->used = BIG_STACK_BYTES - uxTaskGetStackHighWaterMark(NULL);
    xTaskNotifyGive(j->waiter);
    vTaskDelete(NULL);
}

int gmssl_run_bigstack(int (*fn)(void *), void *arg, unsigned *stack_used)
{
    big_job_t j = {fn, arg, -1, 0, xTaskGetCurrentTaskHandle()};

    xTaskCreateStatic(big_task, "big", BIG_STACK_BYTES, &j, 10, big_stack, &big_tcb);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(20)); /* let the task finish deleting itself */
    if (stack_used && j.used > *stack_used) {
        *stack_used = j.used;
    }
    return j.rc;
}
