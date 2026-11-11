/* ML-DSA-87 only: let mldsa-native take its large temporaries from the heap
 * (MLD_CONFIG_CUSTOM_ALLOC_FREE) instead of the stack. A signature needs
 * about 113 KB of them in one piece on the stack, and the heap of the
 * ESP32-C3 is made of three separate blocks of at most 128 KB, of which a
 * RAM-loaded image leaves none entirely free. From the heap the same memory
 * is taken in several smaller pieces. The harness reports it in the "heap"
 * column; stack plus heap is what the operation needs. */
#ifndef PQC_MLDSA_HEAP_ALLOC_H
#define PQC_MLDSA_HEAP_ALLOC_H

#define MLD_CONFIG_CUSTOM_ALLOC_FREE

#if !defined(__ASSEMBLER__)
#include <stdint.h>
#include <stdlib.h>

/* malloc with 32-byte alignment; the distance to the raw block is kept in
 * the byte in front of the returned pointer */
static inline void *pqc_mldsa_alloc(size_t n)
{
    uint8_t *raw = (uint8_t *)malloc(n + 32);
    uint8_t *p;
    if (raw == NULL) {
        return NULL;
    }
    p = (uint8_t *)(((uintptr_t)raw + 32) & ~(uintptr_t)31);
    p[-1] = (uint8_t)(p - raw);
    return p;
}

static inline void pqc_mldsa_free(void *ptr)
{
    if (ptr != NULL) {
        uint8_t *p = (uint8_t *)ptr;
        free(p - p[-1]);
    }
}

#define MLD_CUSTOM_ALLOC(v, T, N) T *(v) = (T *)pqc_mldsa_alloc(sizeof(T) * (N))
#define MLD_CUSTOM_FREE(v, T, N) pqc_mldsa_free(v)
#endif /* !__ASSEMBLER__ */

#endif /* PQC_MLDSA_HEAP_ALLOC_H */
