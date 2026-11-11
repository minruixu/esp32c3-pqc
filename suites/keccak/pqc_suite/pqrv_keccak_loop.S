/* PQRV's RV32IM Keccak-f[1600] (looped), for comparison only: it uses gp and
 * tp as scratch registers, so the caller must keep interrupts masked. */
#define KeccakF1600_StatePermute_RV32ASM pqrv_keccak_loop
#define constants_keccak pqrv_keccak_loop_rc
#define loop_start pqrv_keccak_loop_top
#include "../../../third_party/pqrv/sha3/fips202_rv32im.S"
