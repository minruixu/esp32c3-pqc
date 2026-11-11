/* PQRV's RV32IM Keccak-f[1600] (two rounds unrolled), for comparison only: it
 * uses gp and tp as scratch registers, so the caller must keep interrupts masked. */
#define KeccakF1600_StatePermute_RV32ASM pqrv_keccak_unroll
#define constants_keccak pqrv_keccak_unroll_rc
#define loop_start pqrv_keccak_unroll_top
#include "../../../third_party/pqrv/sha3/fips202_rv32im_unroll.S"
