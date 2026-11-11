/* Device only: what the SHA accelerator costs, step by step, in CPU cycles,
 * and two properties the fast paths rely on. */
#include <string.h>
#include "esp_cpu.h"
#include "bench.h"
#include "pqc_prims.h"
#include "sha256_hw.h"

#define N 256

void hw_probe(void)
{
    static uint32_t st[8], blk[16], out[8], back[16];
    uint32_t t0, c_load, c_text, c_run, c_read, c_all, c_empty;
    uint32_t sw[8], hw[8];
    int same;

    pqc_sha256_hw_init();
    randombytes((uint8_t *)st, sizeof(st));
    randombytes((uint8_t *)blk, sizeof(blk));

    t0 = esp_cpu_get_cycle_count();
    for (int i = 0; i < N; i++) {
        __asm__ volatile("");
    }
    c_empty = esp_cpu_get_cycle_count() - t0;

    t0 = esp_cpu_get_cycle_count();
    for (int i = 0; i < N; i++) {
        pqc_sha256_hw_load(st);
    }
    c_load = esp_cpu_get_cycle_count() - t0;

    t0 = esp_cpu_get_cycle_count();
    for (int i = 0; i < N; i++) {
        for (int k = 0; k < 16; k++) {
            PQC_SHA_M[k] = blk[k];
        }
    }
    c_text = esp_cpu_get_cycle_count() - t0;

    t0 = esp_cpu_get_cycle_count();
    for (int i = 0; i < N; i++) {
        pqc_sha256_hw_run();
    }
    c_run = esp_cpu_get_cycle_count() - t0;

    t0 = esp_cpu_get_cycle_count();
    for (int i = 0; i < N; i++) {
        pqc_sha256_hw_read(out, 8);
    }
    c_read = esp_cpu_get_cycle_count() - t0;

    t0 = esp_cpu_get_cycle_count();
    for (int i = 0; i < N; i++) {
        pqc_sha256_hw_load(st);
        pqc_sha256_hw_block(blk);
        pqc_sha256_hw_read(out, 8);
    }
    c_all = esp_cpu_get_cycle_count() - t0;

    bench_info("sha256hw-cycles",
               "load8=%u text16=%u run=%u read8=%u whole=%u loop=%u (CPU cycles each, average of %d)",
               (unsigned)(c_load / N), (unsigned)(c_text / N), (unsigned)(c_run / N),
               (unsigned)(c_read / N), (unsigned)(c_all / N), (unsigned)(c_empty / N), N);

    /* Straight-line register traffic, values already in CPU registers. */
    {
        uint32_t a = st[0], b = st[1], c = st[2], d = st[3];
        uint32_t polls = 0, c_w16, c_r8, c_go;
        volatile uint32_t *M = PQC_SHA_M;
        volatile uint32_t *H = PQC_SHA_H;

        t0 = esp_cpu_get_cycle_count();
        for (int i = 0; i < N; i++) {
            M[0] = a; M[1] = b; M[2] = c; M[3] = d; M[4] = a; M[5] = b; M[6] = c; M[7] = d;
            M[8] = a; M[9] = b; M[10] = c; M[11] = d; M[12] = a; M[13] = b; M[14] = c; M[15] = d;
        }
        c_w16 = esp_cpu_get_cycle_count() - t0;

        t0 = esp_cpu_get_cycle_count();
        for (int i = 0; i < N; i++) {
            a += H[0]; b += H[1]; c += H[2]; d += H[3]; a += H[4]; b += H[5]; c += H[6]; d += H[7];
        }
        c_r8 = esp_cpu_get_cycle_count() - t0;
        out[0] = a + b + c + d;

        t0 = esp_cpu_get_cycle_count();
        for (int i = 0; i < N; i++) {
            REG_WRITE(SHA_CONTINUE_REG, 1);
            while (REG_READ(SHA_BUSY_REG)) {
                polls++;
            }
        }
        c_go = esp_cpu_get_cycle_count() - t0;
        bench_info("sha256hw-straight", "write16=%u read8=%u run=%u busy-polls-per-run=%u.%02u",
                   (unsigned)(c_w16 / N), (unsigned)(c_r8 / N), (unsigned)(c_go / N),
                   (unsigned)(polls / N), (unsigned)((polls % N) * 100 / N));

        /* Is the result already there right after the start, without polling? */
        {
            uint32_t ok = 0, exp[8], got0;
            for (int i = 0; i < 2000; i++) {
                randombytes((uint8_t *)blk, sizeof(blk));
                pqc_sha256_hw_load(st);
                pqc_sha256_hw_block(blk);
                pqc_sha256_hw_read(exp, 8);
                pqc_sha256_hw_load(st);
                REG_WRITE(SHA_CONTINUE_REG, 1);
                got0 = H[7];
                while (REG_READ(SHA_BUSY_REG)) {
                }
                if (got0 == exp[7]) {
                    ok++;
                }
            }
            bench_info("sha256hw-read-without-wait", "correct in %u of 2000 trials", (unsigned)ok);
        }
    }

    /* Are the M registers left intact by a compression? */
    pqc_sha256_hw_load(st);
    pqc_sha256_hw_block(blk);
    for (int k = 0; k < 16; k++) {
        back[k] = PQC_SHA_M[k];
    }
    same = memcmp(back, blk, sizeof(blk)) == 0;
    bench_info("sha256hw-text-readback", "%s", same ? "same as written" : "differs from what was written");

    /* Does a second run without rewriting M compress the same block again? */
    for (int i = 0; i < 8; i++) {
        sw[i] = __builtin_bswap32(st[i]);
    }
    pqc_sha256_compress_ref(sw, (const uint8_t *)blk, 1);
    pqc_sha256_compress_ref(sw, (const uint8_t *)blk, 1);
    pqc_sha256_hw_load(st);
    pqc_sha256_hw_block(blk);
    pqc_sha256_hw_run(); /* M not rewritten */
    pqc_sha256_hw_read(hw, 8);
    for (int i = 0; i < 8; i++) {
        hw[i] = __builtin_bswap32(hw[i]);
    }
    bench_check("sha256/opt/hw-keeps-block", memcmp(sw, hw, sizeof(sw)) == 0);

    /* May the next block be written while the previous one is being
     * compressed? Three blocks back to back, 2000 random trials. */
    {
        static uint32_t b3[48];
        int ok = 1;
        for (int t = 0; t < 2000; t++) {
            randombytes((uint8_t *)b3, sizeof(b3));
            randombytes((uint8_t *)st, sizeof(st));
            for (int i = 0; i < 8; i++) {
                sw[i] = __builtin_bswap32(st[i]);
            }
            pqc_sha256_compress_ref(sw, (const uint8_t *)b3, 3);
            pqc_sha256_hw_load(st);
            for (int blkno = 0; blkno < 3; blkno++) {
                for (int k = 0; k < 16; k++) {
                    PQC_SHA_M[k] = b3[16 * blkno + k];
                }
                pqc_sha256_hw_run_nowait();
            }
            pqc_sha256_hw_wait();
            pqc_sha256_hw_read(hw, 8);
            for (int i = 0; i < 8; i++) {
                hw[i] = __builtin_bswap32(hw[i]);
            }
            if (memcmp(sw, hw, sizeof(sw)) != 0) {
                ok = 0;
            }
        }
        bench_check("sha256/opt/hw-next-block-written-while-busy", ok);
    }

    /* Does the accelerator take its copy of the block when it starts? Then
     * overwriting the LAST message word right after the start must not
     * change the result. */
    {
        int ok = 1;
        for (int t = 0; t < 2000; t++) {
            randombytes((uint8_t *)blk, sizeof(blk));
            for (int i = 0; i < 8; i++) {
                sw[i] = __builtin_bswap32(st[i]);
            }
            pqc_sha256_compress_ref(sw, (const uint8_t *)blk, 1);
            pqc_sha256_hw_load(st);
            for (int k = 0; k < 16; k++) {
                PQC_SHA_M[k] = blk[k];
            }
            pqc_sha256_hw_start();
            PQC_SHA_M[15] = ~blk[15];
            PQC_SHA_M[14] = ~blk[14];
            PQC_SHA_M[4] = ~blk[4];
            pqc_sha256_hw_wait();
            pqc_sha256_hw_read(hw, 8);
            for (int i = 0; i < 8; i++) {
                hw[i] = __builtin_bswap32(hw[i]);
            }
            if (memcmp(sw, hw, sizeof(sw)) != 0) {
                ok = 0;
            }
        }
        bench_check("sha256/opt/hw-copies-block-at-start", ok);
    }

    /* Can single M words be replaced while the others stay? */
    {
        uint32_t blk2[16];
        memcpy(blk2, blk, sizeof(blk));
        blk2[5] ^= 0x01020304u;
        blk2[9] = ~blk2[9];
        for (int i = 0; i < 8; i++) {
            sw[i] = __builtin_bswap32(st[i]);
        }
        pqc_sha256_compress_ref(sw, (const uint8_t *)blk2, 1);
        pqc_sha256_hw_load(st);
        pqc_sha256_hw_block(blk);
        pqc_sha256_hw_load(st);
        PQC_SHA_M[5] = blk2[5];
        PQC_SHA_M[9] = blk2[9];
        pqc_sha256_hw_run();
        pqc_sha256_hw_read(hw, 8);
        for (int i = 0; i < 8; i++) {
            hw[i] = __builtin_bswap32(hw[i]);
        }
        bench_check("sha256/opt/hw-partial-block-update", memcmp(sw, hw, sizeof(sw)) == 0);
    }
}
