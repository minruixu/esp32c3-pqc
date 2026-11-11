#ifndef MLDSA_PQM_H
#define MLDSA_PQM_H

#include "kemsig.h"

typedef struct {
    const sig_alg *alg;
    int (*kat)(void *arg);
} mldsa_pqm_kat;

#endif
