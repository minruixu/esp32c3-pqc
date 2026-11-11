#pragma once
/* Every function returns 0 on success. */
int kem_kat(void);
int kem_keygen(void);
int kem_encaps(void);
int kem_decaps(void);

int dsa_kat(void);
int dsa_keygen(void);
int dsa_sign(void);
int dsa_verify(void);
