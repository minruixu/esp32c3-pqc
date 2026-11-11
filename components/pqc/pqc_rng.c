/* Random source for both libraries. esp_fill_random() is a true RNG only
 * while Wi-Fi/BLE is running or bootloader_random_enable() is active;
 * otherwise it is pseudo-random and must not be used for real keys. */
#include <stddef.h>
#include <stdint.h>
#include "esp_random.h"

int randombytes(uint8_t *out, size_t outlen)
{
    esp_fill_random(out, outlen);
    return 0;
}
