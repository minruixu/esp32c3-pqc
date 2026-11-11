/* PQMagic's sm3_extended(), built under another name: port/pqm_sm3.c
 * provides sm3_extended() and calls this one for the reference variant. */
#define sm3_extended pqm_sm3_extended_upstream
#include "hash/sm3/sm3_extended.c"
