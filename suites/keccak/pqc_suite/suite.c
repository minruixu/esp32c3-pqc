/* Keccak-f[1600] implementations compared on the device, a correctness check
 * of the optimized permutation, and the measured instruction costs of the
 * ESP32-C3 core that the assembly in this project is tuned for. */
#include <stdint.h>
#include <string.h>
#include "bench.h"
#include "pqc_prims.h"

#ifndef PQC_HOST
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#define CYCLES() ((uint32_t)esp_cpu_get_cycle_count())
#else
#define CYCLES() 0u
#endif

void kbench_mlkn_permute(uint64_t *state);
#ifndef PQC_HOST
void pqrv_keccak_loop(uint64_t *state);
void pqrv_keccak_unroll(uint64_t *state);
#endif

typedef struct {
    const char *name;
    void (*fn)(uint64_t *state);
    int irq_off; /* the implementation clobbers gp/tp */
} impl_t;

static const impl_t impls[] = {
    {"ref", pqc_keccakf1600_ref, 0},        /* PQClean C */
    {"mlkn", kbench_mlkn_permute, 0},       /* mlkem-native C */
#ifndef PQC_HOST
    {"pqrv", pqrv_keccak_loop, 1},          /* PQRV RV32IM assembly */
    {"pqrv_unroll", pqrv_keccak_unroll, 1}, /* PQRV RV32IM assembly, unrolled */
#endif
    {"opt", pqc_keccakf1600_opt, 0},        /* this project */
};
#define NIMPL (sizeof(impls) / sizeof(impls[0]))

#define NPERM 1000
static const impl_t *I;
static uint64_t st[25];
static uint32_t cyc_per_perm;

/* The state is copied to the job's stack: that is where callers keep it, and
 * in a RAM-loaded image static data is slower to reach than the stack (see
 * the cost model below). */
static int job_perm(void *arg)
{
    uint32_t c0;
    uint64_t s[25];
    (void)arg;
    memcpy(s, st, sizeof(s));
#ifndef PQC_HOST
    if (I->irq_off) {
        portDISABLE_INTERRUPTS();
    }
#endif
    c0 = CYCLES();
    for (int i = 0; i < NPERM; i++) {
        I->fn(s);
    }
    cyc_per_perm = (CYCLES() - c0) / NPERM;
    memcpy(st, s, sizeof(s));
#ifndef PQC_HOST
    if (I->irq_off) {
        portENABLE_INTERRUPTS();
    }
#endif
    return 0;
}

/* opt must equal ref on random states, and when iterated */
static int job_verify(void *arg)
{
    uint64_t a[25], b[25];
    (void)arg;

    for (int i = 0; i < 2000; i++) {
        randombytes((uint8_t *)a, sizeof(a));
        if (i == 0) memset(a, 0, sizeof(a));
        if (i == 1) memset(a, 0xff, sizeof(a));
        memcpy(b, a, sizeof(a));
        pqc_keccakf1600_ref(a);
        pqc_keccakf1600_opt(b);
        if (memcmp(a, b, sizeof(a)) != 0) return 1 + i;
    }
    for (int i = 0; i < 2000; i++) {
        pqc_keccakf1600_ref(a);
        pqc_keccakf1600_opt(b);
    }
    return memcmp(a, b, sizeof(a)) != 0 ? -1 : 0;
}

#ifndef PQC_HOST
/* ---------- instruction cost model ---------- */
typedef void (*cm_fn)(uint32_t n, uint32_t *mem);
#define CM(name) void name(uint32_t n, uint32_t *mem)
CM(cm_empty); CM(cm_add_dep); CM(cm_xor_indep); CM(cm_shift_dep); CM(cm_shift_var);
CM(cm_load_indep); CM(cm_load_use0); CM(cm_load_use1); CM(cm_load2_use2); CM(cm_load_addr_dep);
CM(cm_store); CM(cm_store_load); CM(cm_loadb); CM(cm_loadh); CM(cm_mul_dep); CM(cm_mul_indep);
CM(cm_mulh_dep); CM(cm_mulhu_dep); CM(cm_mul_mulh_pair); CM(cm_divu); CM(cm_remu_small);
CM(cm_jump); CM(cm_branch_nt); CM(cm_branch_t); CM(cm_call); CM(cm_cxor);
CM(cm_xor_misaligned); CM(cm_empty_misaligned); CM(cm_mixed);
CM(cm_m_l1a1); CM(cm_m_l1a3); CM(cm_m_l2a6); CM(cm_m_l4a4); CM(cm_m_s1a1); CM(cm_m_s1a3);
CM(cm_m_l1a2u); CM(cm_m_l1u0a2); CM(cm_m_l32); CM(cm_m_a64); CM(cm_m_lsp); CM(cm_m_clw);
CM(cm_m_keccak_like);
CM(cm_mul_zero); CM(cm_mul_small); CM(cm_mul_large); CM(cm_mulh_zero); CM(cm_mulh_small);
CM(cm_mulh_large); CM(cm_mulh_neg); CM(cm_mulhu_large); CM(cm_divu_small); CM(cm_divu_large);
CM(cm_divu_byzero);

typedef struct {
    const char *name;
    cm_fn fn;
    int count;   /* instructions (or events) in the block */
    cm_fn base;  /* loop to subtract */
} cm_t;

static const cm_t cms[] = {
    {"add_dependent", cm_add_dep, 16, cm_empty},
    {"xor_independent", cm_xor_indep, 16, cm_empty},
    {"xor_compressed", cm_cxor, 16, cm_empty},
    {"xor_32bit_at_odd_halfword", cm_xor_misaligned, 16, cm_empty_misaligned},
    {"mixed_16_32bit", cm_mixed, 16, cm_empty},
    {"shift_const", cm_shift_dep, 16, cm_empty},
    {"shift_by_register", cm_shift_var, 16, cm_empty},
    {"load_unused", cm_load_indep, 16, cm_empty},
    {"load_plus_use_next_insn", cm_load_use0, 8, cm_empty},
    {"load_other_use", cm_load_use1, 8, cm_empty},
    {"load_load_use_use", cm_load2_use2, 8, cm_empty},
    {"load_pointer_chase", cm_load_addr_dep, 16, cm_empty},
    {"load_byte", cm_loadb, 16, cm_empty},
    {"load_half_signed", cm_loadh, 16, cm_empty},
    {"store", cm_store, 16, cm_empty},
    {"store_then_load", cm_store_load, 8, cm_empty},
    {"mul_dependent", cm_mul_dep, 16, cm_empty},
    {"mul_independent", cm_mul_indep, 16, cm_empty},
    {"mulh_dependent", cm_mulh_dep, 16, cm_empty},
    {"mulhu_dependent", cm_mulhu_dep, 16, cm_empty},
    {"mul_then_mulh_of_product", cm_mul_mulh_pair, 8, cm_empty},
    {"divu_31bit_by_12bit", cm_divu, 4, cm_empty},
    {"remu_13bit_by_12bit", cm_remu_small, 4, cm_empty},
    {"jump", cm_jump, 8, cm_empty},
    {"branch_not_taken", cm_branch_nt, 8, cm_empty},
    {"branch_taken", cm_branch_t, 8, cm_empty},
    {"call_and_return", cm_call, 4, cm_empty},
    /* memory patterns: cycles per instruction of the whole block */
    {"pat_8x(lw,alu)", cm_m_l1a1, 16, cm_empty},
    {"pat_8x(lw,alu,alu,alu)", cm_m_l1a3, 32, cm_empty},
    {"pat_4x(lw,lw,6alu)", cm_m_l2a6, 32, cm_empty},
    {"pat_4x(4lw,4alu)", cm_m_l4a4, 32, cm_empty},
    {"pat_8x(sw,alu)", cm_m_s1a1, 16, cm_empty},
    {"pat_8x(sw,alu,alu,alu)", cm_m_s1a3, 32, cm_empty},
    {"pat_8x(lw,alu,alu,use)", cm_m_l1a2u, 32, cm_empty},
    {"pat_8x(lw,use,alu,alu)", cm_m_l1u0a2, 32, cm_empty},
    {"pat_32lw", cm_m_l32, 32, cm_empty},
    {"pat_64alu", cm_m_a64, 64, cm_empty},
    {"pat_16lw_sp", cm_m_lsp, 16, cm_empty},
    {"pat_16c.lw", cm_m_clw, 16, cm_empty},
    {"pat_keccak_lane", cm_m_keccak_like, 40, cm_empty},
    /* operand dependence of the multiplier and divider */
    {"mul_0x0", cm_mul_zero, 16, cm_empty},
    {"mul_3x5", cm_mul_small, 16, cm_empty},
    {"mul_ffffffffx7fffffff", cm_mul_large, 16, cm_empty},
    {"mulh_0x0", cm_mulh_zero, 16, cm_empty},
    {"mulh_3x5", cm_mulh_small, 16, cm_empty},
    {"mulh_80000000x7fffffff", cm_mulh_large, 16, cm_empty},
    {"mulh_-1x-1", cm_mulh_neg, 16, cm_empty},
    {"mulhu_ffffffffxffffffff", cm_mulhu_large, 16, cm_empty},
    {"divu_7by3", cm_divu_small, 16, cm_empty},
    {"divu_ffffffffby3", cm_divu_large, 16, cm_empty},
    {"divu_by0", cm_divu_byzero, 16, cm_empty},
};

static uint32_t cm_static[64];
static uint32_t *cm_mem;

static uint32_t cm_time(cm_fn fn, uint32_t n)
{
    uint32_t best = UINT32_MAX;
    for (int r = 0; r < 5; r++) {
        uint32_t c0 = CYCLES();
        fn(n, cm_mem);
        uint32_t c = CYCLES() - c0;
        if (c < best) best = c;
    }
    return best;
}

/* The whole model is measured twice: with the data in a static buffer, which
 * in a RAM-loaded image sits in the same memory block as the code, and with
 * the data on the task stack, which is allocated from the heap. */
static int job_cpumodel(void *arg)
{
    const uint32_t n = 5000;
    uint32_t stackbuf[64];
    const char *where = (const char *)arg;

    memset(stackbuf, 0, sizeof(stackbuf));
    cm_mem = (where[1] == 't' && where[2] == 'a' && where[3] == 't') ? cm_static : stackbuf;
    bench_info("cpu/data_address", "%s buffer at 0x%08x, code at 0x%08x", where,
               (unsigned)(uintptr_t)cm_mem, (unsigned)(uintptr_t)cm_empty);
    for (size_t i = 0; i < sizeof(cms) / sizeof(cms[0]); i++) {
        uint32_t t = cm_time(cms[i].fn, n), b = cm_time(cms[i].base, n);
        uint32_t d = (t > b) ? t - b : 0;
        uint32_t per100 = (uint32_t)(((uint64_t)d * 100) / ((uint64_t)n * cms[i].count));
        char key[64];
        strcpy(key, "cpu/");
        strcat(key, where);
        strcat(key, "/");
        strcat(key, cms[i].name);
        bench_info(key, "%u.%02u cycles each", (unsigned)(per100 / 100), (unsigned)(per100 % 100));
    }
    return 0;
}
/* ---------- the C library routines the sponge code relies on ---------- */
static int job_libc(void *arg)
{
    uint32_t a[64], b[64];
    volatile uint8_t sink = 0;
    uint32_t c0, t;
    (void)arg;

    memset(a, 1, sizeof(a));
    c0 = CYCLES();
    for (int i = 0; i < 1000; i++) {
        memcpy(b, a, 168);
        sink ^= (uint8_t)b[i & 31];
    }
    t = (CYCLES() - c0) / 1000;
    bench_info("cpu/libc/memcpy_168_aligned", "%u cycles", (unsigned)t);
    c0 = CYCLES();
    for (int i = 0; i < 1000; i++) {
        memcpy((uint8_t *)b + 1, (uint8_t *)a + 3, 168);
        sink ^= (uint8_t)b[i & 31];
    }
    t = (CYCLES() - c0) / 1000;
    bench_info("cpu/libc/memcpy_168_unaligned", "%u cycles", (unsigned)t);
    c0 = CYCLES();
    for (int i = 0; i < 1000; i++) {
        memcpy(b, a, 32);
        sink ^= (uint8_t)b[i & 7];
    }
    t = (CYCLES() - c0) / 1000;
    bench_info("cpu/libc/memcpy_32_aligned", "%u cycles", (unsigned)t);
    c0 = CYCLES();
    for (int i = 0; i < 1000; i++) {
        memset(b, 0, 200);
        sink ^= (uint8_t)b[i & 31];
    }
    t = (CYCLES() - c0) / 1000;
    bench_info("cpu/libc/memset_200", "%u cycles", (unsigned)t);
    c0 = CYCLES();
    for (int i = 0; i < 1000; i++) {
        for (int k = 0; k < 42; k++) {
            b[k] = a[k];
        }
        sink ^= (uint8_t)b[i & 31];
    }
    t = (CYCLES() - c0) / 1000;
    bench_info("cpu/libc/word_loop_copy_168", "%u cycles", (unsigned)t);
    return sink & 0;
}
#endif /* !PQC_HOST */

void suite_main(void)
{
    char key[40];

    for (size_t i = 0; i < NIMPL; i++) {
        I = &impls[i];
        if (!bench_selected(bench_name("keccakf1600", I->name, "x1000"))) {
            continue;
        }
        bench_rng_seed(7);
        randombytes((uint8_t *)st, sizeof(st));
        bench_run(bench_name("keccakf1600", I->name, "x1000"), job_perm, NULL, 5, 4096);
        strcpy(key, "keccakf1600/");
        strcat(key, I->name);
        bench_info(key, "%u cycles per permutation", (unsigned)cyc_per_perm);
        /* every implementation must reach the same state */
        bench_digest(bench_name("keccakf1600", "all", "state_after_5000"), st, sizeof(st));
    }
    bench_rng_seed(8);
    bench_check("keccakf1600/opt/equals_ref",
                bench_run("keccakf1600/opt/verify", job_verify, NULL, 1, 4096) == 0);
#ifndef PQC_HOST
    if (bench_selected("cpu/model")) {
        bench_run("cpu/model/static", job_cpumodel, "static", 1, 4096);
        bench_run("cpu/model/stack", job_cpumodel, "stack", 1, 4096);
    }
    if (bench_selected("cpu/libc")) {
        bench_run("cpu/libc", job_libc, NULL, 1, 4096);
    }
#endif
}
