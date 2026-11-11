#include <string.h>
#include "hwaes.h"
#include "lac_port.h"
#include "pqc_prims.h"
#include "pqclean_aes/aes.h"

void lac_aes256ctr(uint8_t *out, size_t len, const uint8_t *key)
{
    static const uint8_t zero_nonce[AESCTR_NONCEBYTES];
    size_t full = len / 16, rem = len % 16;
    aes256ctx ctx;

    if (pqc_prims_selected() == PQC_PRIMS_OPT && pqm_hwaes_ctr(out, full, key, 32, 0, 0)) {
        if (rem) {
            uint8_t last[16];
            pqm_hwaes_ctr(last, 1, key, 32, (uint32_t)full, 0);
            memcpy(out + 16 * full, last, rem);
        }
        return;
    }
    aes256_ctr_keyexp(&ctx, key);
    aes256_ctr(out, len, zero_nonce, &ctx);
    aes256_ctx_release(&ctx);
}
