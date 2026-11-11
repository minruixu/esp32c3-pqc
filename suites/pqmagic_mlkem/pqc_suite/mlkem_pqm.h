#ifndef MLKEM_PQM_H
#define MLKEM_PQM_H

typedef struct {
    int (*keypair_internal)(unsigned char *pk, unsigned char *sk, const unsigned char *coins);
    int (*enc_internal)(unsigned char *ct, unsigned char *ss, const unsigned char *pk,
                        const unsigned char *coins);
} mlkem_pqm_derand;

#endif
