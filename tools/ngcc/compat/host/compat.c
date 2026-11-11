/* Functions that some packages expect from glibc and that the host's C
 * library may lack. Host builds only; the device has newlib's. */
#include <stddef.h>

#if defined(__APPLE__)
void explicit_bzero(void *p, size_t n)
{
    volatile unsigned char *q = p;
    while (n--) {
        *q++ = 0;
    }
}
#endif
