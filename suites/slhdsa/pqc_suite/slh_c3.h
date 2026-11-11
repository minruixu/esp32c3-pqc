/* ESP32-C3 instantiation of SLH-DSA with SHA-2: see slh_sha2_c3.c. */
#ifndef SLH_C3_H
#define SLH_C3_H

#include "slh_dsa.h"

/* The six SHA-2 parameter sets with the hash functions replaced by versions
 * that run SHA-256 on the hardware accelerator. Same inputs, same outputs. */
extern slh_param_t slh_c3_sha2_128s, slh_c3_sha2_128f;
extern slh_param_t slh_c3_sha2_192s, slh_c3_sha2_192f;
extern slh_param_t slh_c3_sha2_256s, slh_c3_sha2_256f;

/* Fill in the parameter sets above. Call once before using them. */
void slh_c3_init(void);

/* The six SHAKE parameter sets with F, PRF, H and the WOTS+ chain written
 * directly on the Keccak state (slh_shake_fast.c). Same inputs and outputs. */
extern slh_param_t slh_fast_shake_128s, slh_fast_shake_128f;
extern slh_param_t slh_fast_shake_192s, slh_fast_shake_192f;
extern slh_param_t slh_fast_shake_256s, slh_fast_shake_256f;
void slh_fast_shake_init(void);

/* Erase the secret-key words this file keeps between calls. */
void slh_c3_wipe(void);

/* Set by any code that used the accelerator's message registers for
 * something else, so that the SLH-DSA code reloads them. */
extern unsigned slh_c3_m_dirty;

#endif /* SLH_C3_H */
