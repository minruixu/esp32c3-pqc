#include "hwaes.h"

#ifdef PQC_HOST

int pqm_hwaes_ctr(uint8_t *out, size_t nblocks, const uint8_t *key, unsigned keybytes,
                  uint32_t first, int little_endian)
{
    (void)out; (void)nblocks; (void)key; (void)keybytes; (void)first; (void)little_endian;
    return 0;
}

#else

#include <string.h>
#include "esp_private/esp_crypto_lock_internal.h"
#include "hal/aes_hal.h"
#include "hal/aes_ll.h"
#include "soc/hwcrypto_reg.h"

static int ready;

/* The accelerator works on one block at a time: four words in, start, wait,
 * four words out. Only the counter word changes from block to block, so the
 * other three input words are written once per call. */
int pqm_hwaes_ctr(uint8_t *out, size_t nblocks, const uint8_t *key, unsigned keybytes,
                  uint32_t first, int little_endian)
{
    volatile uint32_t *const in = (volatile uint32_t *)AES_TEXT_IN_BASE;
    volatile uint32_t *const res = (volatile uint32_t *)AES_TEXT_OUT_BASE;
    const int unaligned = ((uintptr_t)out & 3) != 0;

    if (!ready) {
        AES_RCC_ATOMIC()
        {
            aes_ll_enable_bus_clock(true);
            aes_ll_reset_register();
        }
        ready = 1;
    }
    aes_hal_setkey(key, keybytes, ESP_AES_ENCRYPT);
    in[0] = 0;
    in[1] = 0;
    in[2] = 0;
    in[3] = 0;
    for (size_t i = 0; i < nblocks; i++) {
        uint32_t c = first + (uint32_t)i;
        uint32_t w[4];

        if (little_endian) {
            in[0] = c;
        } else {
            in[3] = __builtin_bswap32(c);
        }
        REG_WRITE(AES_TRIGGER_REG, 1);
        while (REG_READ(AES_STATE_REG) != ESP_AES_STATE_IDLE) {
        }
        if (unaligned) {
            w[0] = res[0];
            w[1] = res[1];
            w[2] = res[2];
            w[3] = res[3];
            memcpy(out + 16 * i, w, 16);
        } else {
            uint32_t *o = (uint32_t *)(void *)(out + 16 * i);
            o[0] = res[0];
            o[1] = res[1];
            o[2] = res[2];
            o[3] = res[3];
        }
    }
    return 1;
}

#endif
