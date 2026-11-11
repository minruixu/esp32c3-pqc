/* Random bytes for the HQC builds: the harness RNG, or a scripted sequence
 * while a known-answer test replays the official test vectors. */
#include <string.h>
#include "bench.h"
#include "hqc_api.h"

static const uint8_t *script;
static size_t script_left;

void hqc_rng_script(const uint8_t *data, size_t len)
{
    script = data;
    script_left = len;
}

int hqc_randombytes(uint8_t *out, size_t outlen)
{
    if (script_left) {
        if (outlen > script_left) {
            return -1;
        }
        memcpy(out, script, outlen);
        script += outlen;
        script_left -= outlen;
        return 0;
    }
    return randombytes(out, outlen);
}
