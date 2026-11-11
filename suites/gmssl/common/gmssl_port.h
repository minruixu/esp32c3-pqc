/* Glue between the unchanged GmSSL sources and this project. */
#ifndef GMSSL_PORT_H
#define GMSSL_PORT_H

/* 0: GmSSL's own SM3 and SHA-3 code.
 * 1: SHA-3 and SHAKE as a direct sponge around pqc_keccakf1600() of
 *    components/prims (whichever backend pqc_prims_select() chose); SM3
 *    either with lean buffering around pqc_sm3_compress(), or GmSSL's own
 *    SM3 if gmssl_port_calibrate() found its compression function quicker.
 * Same outputs either way. Switch only between operations. */
extern int gmssl_fast;

/* Time the two SM3 compression functions (on the device) and let fast mode
 * use the quicker one. Call after pqc_prims_select(). Without a call, and on
 * the host, fast mode uses components/prims. */
void gmssl_port_calibrate(void);


/* Run fn(arg) on a large static stack and return its result (bigstack.c,
 * device only; the host build calls fn directly). *stack_used is raised to
 * the peak stack use if that is higher. */
int gmssl_run_bigstack(int (*fn)(void *), void *arg, unsigned *stack_used);

#endif
