#if defined(PQC_PROFILE) && !defined(PQC_HOST)
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "riscv/rvruntime-frames.h"
#include "bench.h"
#include "c3_prof.h"

#define SLOTS 2048 /* power of two */
#define SHIFT 6    /* 64-byte buckets */

static struct {
    uint32_t key;
    uint32_t count;
} table[SLOTS];
static volatile int running;
static uint32_t total, dropped;

static void tick_hook(void)
{
    if (!running) {
        return;
    }
    /* The interrupt entry stored the interrupted context on the task's stack
     * and its address in the first field of the task control block. */
    void **tcb = (void **)xTaskGetCurrentTaskHandle();
    const RvExcFrame *frame = (const RvExcFrame *)tcb[0];
    uint32_t key = (uint32_t)frame->mepc >> SHIFT;
    uint32_t h = (key * 2654435761u) >> 21;

    total++;
    for (int probe = 0; probe < 64; probe++) {
        uint32_t i = (h + probe) & (SLOTS - 1);
        if (table[i].key == key) {
            table[i].count++;
            return;
        }
        if (table[i].count == 0) {
            table[i].key = key;
            table[i].count = 1;
            return;
        }
    }
    dropped++;
}

void c3_prof_start(void)
{
    static int registered;
    memset(table, 0, sizeof(table));
    total = dropped = 0;
    if (!registered) {
        esp_register_freertos_tick_hook(tick_hook);
        registered = 1;
    }
    running = 1;
}

void c3_prof_stop(void) { running = 0; }

void c3_prof_report(const char *label, int top)
{
    char key[64];

    snprintf(key, sizeof(key), "prof/%s/total", label);
    bench_info(key, "%u samples, %u not recorded", (unsigned)total, (unsigned)dropped);
    for (int rank = 0; rank < top; rank++) {
        int best = -1;
        for (int i = 0; i < SLOTS; i++) {
            if (table[i].count && (best < 0 || table[i].count > table[best].count)) {
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        snprintf(key, sizeof(key), "prof/%s/%03d", label, rank);
        bench_info(key, "0x%08x %u", (unsigned)(table[best].key << SHIFT), (unsigned)table[best].count);
        table[best].count = 0;
    }
}
#else
typedef int c3_prof_unused;
#endif
