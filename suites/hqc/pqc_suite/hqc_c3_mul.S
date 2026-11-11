/* hqc_c3_shift_xor_asm(acc, b, p, nwords): acc ^= b * X^p over GF(2).
 *
 *   a0  acc     32-bit words of the accumulator
 *   a1  b       nwords 32-bit words
 *   a2  p       bit position, any value >= 0
 *   a3  nwords  public
 *
 * Writes acc[p >> 5] .. acc[(p >> 5) + nwords]. This is the inner loop of
 * every HQC polynomial product in the ESP32-C3 build (see hqc_c3_pke.c).
 *
 * The instruction sequence depends only on nwords. The secret position p
 * decides where in acc the words go (p >> 5) and the shift count (p & 31);
 * there is no branch, loop bound or early exit that depends on it.
 *
 * Eight instructions per word, eight words per loop iteration:
 *   t  = b[i]
 *   acc[i] ^= (t << s) | carry
 *   carry = (t >> 1) >> (31 - s)       two shifts, so that s = 0 needs no mask
 */
    .section .text.hqc_c3_shift_xor_asm, "ax", @progbits
    .globl  hqc_c3_shift_xor_asm
    .type   hqc_c3_shift_xor_asm, @function
    .balign 4

    .macro STEP off
    lw      a4, \off(a1)
    lw      a5, \off(a0)
    sll     t4, a4, a2
    or      t4, t4, t3
    xor     a5, a5, t4
    sw      a5, \off(a0)
    srli    a4, a4, 1
    srl     t3, a4, t1
    .endm

hqc_c3_shift_xor_asm:
    srli    t0, a2, 5
    slli    t0, t0, 2
    add     a0, a0, t0          /* acc += p >> 5 (words) */
    not     t1, a2              /* srl/sll use the low 5 bits: 31 - s */
    li      t3, 0               /* carry */
    andi    t0, a3, 7
    beqz    t0, 2f
1:  STEP    0                   /* nwords mod 8 single steps */
    addi    a0, a0, 4
    addi    a1, a1, 4
    addi    t0, t0, -1
    bnez    t0, 1b
2:  srli    a3, a3, 3
    beqz    a3, 4f
    slli    a3, a3, 5
    add     a3, a1, a3          /* end of b */
3:  STEP    0
    STEP    4
    STEP    8
    STEP    12
    STEP    16
    STEP    20
    STEP    24
    STEP    28
    addi    a1, a1, 32
    addi    a0, a0, 32
    bne     a1, a3, 3b
4:  lw      a5, 0(a0)
    xor     a5, a5, t3
    sw      a5, 0(a0)
    ret
    .size   hqc_c3_shift_xor_asm, . - hqc_c3_shift_xor_asm
