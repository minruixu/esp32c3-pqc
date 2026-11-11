/* Instruction cost model of the ESP32-C3 core, measured rather than assumed.
 * Each function runs a0 iterations of a block of identical instructions;
 * the caller measures cycles and subtracts the empty loop.
 * a1 points to a scratch buffer of at least 256 bytes. */
    .text
    .option norvc

.macro CM_BEGIN name
    .globl \name
    .type \name, %function
    .balign 16
\name:
.endm

.macro CM_LOOP_END
    addi a0, a0, -1
    bnez a0, 1b
    ret
.endm

/* empty loop: only the counter and the taken branch */
CM_BEGIN cm_empty
1:  CM_LOOP_END

/* 16 dependent additions */
CM_BEGIN cm_add_dep
1:  .rept 16
    add t0, t0, t1
    .endr
    CM_LOOP_END

/* 16 independent xors */
CM_BEGIN cm_xor_indep
1:  .rept 4
    xor t0, t1, t2
    xor t3, t4, t5
    xor t6, a2, a3
    xor a4, a5, a6
    .endr
    CM_LOOP_END

/* 16 shifts by a constant, dependent */
CM_BEGIN cm_shift_dep
1:  .rept 8
    slli t0, t0, 7
    srli t0, t0, 3
    .endr
    CM_LOOP_END

/* 16 shifts by a register amount */
CM_BEGIN cm_shift_var
    li t1, 5
1:  .rept 8
    sll t0, t0, t1
    srl t0, t0, t1
    .endr
    CM_LOOP_END

/* 16 loads, results not used */
CM_BEGIN cm_load_indep
1:  .rept 4
    lw t0, 0(a1)
    lw t1, 4(a1)
    lw t2, 8(a1)
    lw t3, 12(a1)
    .endr
    CM_LOOP_END

/* 8 x (load, use in the very next instruction) */
CM_BEGIN cm_load_use0
1:  .rept 8
    lw t0, 0(a1)
    xor t1, t1, t0
    .endr
    CM_LOOP_END

/* 8 x (load, one unrelated instruction, use) -> 24 instructions */
CM_BEGIN cm_load_use1
1:  .rept 8
    lw t0, 0(a1)
    xor t3, t4, t5
    xor t1, t1, t0
    .endr
    CM_LOOP_END

/* 8 x (load A, load B, use A, use B) -> 32 instructions */
CM_BEGIN cm_load2_use2
1:  .rept 8
    lw t0, 0(a1)
    lw t2, 4(a1)
    xor t1, t1, t0
    xor t3, t3, t2
    .endr
    CM_LOOP_END

/* 8 x (load, load whose address depends on nothing, pointer-chase free) */
CM_BEGIN cm_load_addr_dep
    sw a1, 0(a1)
    mv t0, a1
1:  .rept 16
    lw t0, 0(t0)
    .endr
    CM_LOOP_END

/* 16 stores */
CM_BEGIN cm_store
1:  .rept 4
    sw t0, 0(a1)
    sw t1, 4(a1)
    sw t2, 8(a1)
    sw t3, 12(a1)
    .endr
    CM_LOOP_END

/* 8 x (store, load back the same word) */
CM_BEGIN cm_store_load
1:  .rept 8
    sw t0, 0(a1)
    lw t1, 0(a1)
    .endr
    CM_LOOP_END

/* 16 byte loads and 16 byte stores */
CM_BEGIN cm_loadb
1:  .rept 16
    lbu t0, 1(a1)
    .endr
    CM_LOOP_END

CM_BEGIN cm_loadh
1:  .rept 16
    lh t0, 2(a1)
    .endr
    CM_LOOP_END

/* multiplications, dependent chains */
CM_BEGIN cm_mul_dep
    li t1, 3
1:  .rept 16
    mul t0, t0, t1
    .endr
    CM_LOOP_END

CM_BEGIN cm_mul_indep
    li t1, 3
1:  .rept 4
    mul t0, t2, t1
    mul t3, t4, t1
    mul t5, t6, t1
    mul a2, a3, t1
    .endr
    CM_LOOP_END

CM_BEGIN cm_mulh_dep
    li t1, 0x7fffffff
1:  .rept 16
    mulh t0, t0, t1
    .endr
    CM_LOOP_END

CM_BEGIN cm_mulhu_dep
    li t1, 0x7fffffff
1:  .rept 16
    mulhu t0, t0, t1
    .endr
    CM_LOOP_END

/* mul followed directly by a use of the product (Montgomery style) */
CM_BEGIN cm_mul_mulh_pair
    li t1, 0x12345
1:  .rept 8
    mul t0, t2, t1
    mulh t3, t0, t1
    .endr
    CM_LOOP_END

CM_BEGIN cm_divu
    li t1, 3329
    li t2, 0x7fffffff
1:  .rept 4
    divu t0, t2, t1
    .endr
    CM_LOOP_END

CM_BEGIN cm_remu_small
    li t1, 3329
    li t2, 5000
1:  .rept 4
    remu t0, t2, t1
    .endr
    CM_LOOP_END

/* 8 taken unconditional jumps */
CM_BEGIN cm_jump
1:  .rept 8
    j 2f
2:
    .endr
    CM_LOOP_END

/* 8 conditional branches, never taken */
CM_BEGIN cm_branch_nt
    li t0, 1
1:  .rept 8
    beqz t0, 3f
    .endr
    CM_LOOP_END
3:  ret

/* 8 conditional branches, always taken, forward */
CM_BEGIN cm_branch_t
    li t0, 0
1:  .rept 8
    beqz t0, 2f
2:
    .endr
    CM_LOOP_END

/* 4 call/return pairs */
CM_BEGIN cm_call
    mv t6, ra
1:  .rept 4
    jal ra, cm_leaf
    .endr
    addi a0, a0, -1
    bnez a0, 1b
    mv ra, t6
    ret
cm_leaf:
    ret

/* 16 compressed xors (16-bit encodings) */
    .option rvc
CM_BEGIN cm_cxor
1:  .rept 16
    c.xor a2, a3
    .endr
    .option norvc
    CM_LOOP_END

/* 16 32-bit xors placed at addresses that are 2 mod 4 */
CM_BEGIN cm_xor_misaligned
    .option rvc
    c.nop
    .option norvc
1:  .rept 4
    xor t0, t1, t2
    xor t3, t4, t5
    xor t6, a2, a3
    xor a4, a5, a6
    .endr
    addi a0, a0, -1
    bnez a0, 1b
    ret

/* the empty loop at an address that is 2 mod 4 */
CM_BEGIN cm_empty_misaligned
    .option rvc
    c.nop
    .option norvc
1:  addi a0, a0, -1
    bnez a0, 1b
    ret

/* mixed 16-bit/32-bit stream: c.xor, xor, c.xor, xor ... (16 instructions) */
CM_BEGIN cm_mixed
1:  .rept 8
    .option rvc
    c.xor a2, a3
    .option norvc
    xor t0, t1, t2
    .endr
    CM_LOOP_END

/* ---- memory access patterns: how do loads and stores interact? ---- */
    .option norvc

/* (lw, xor) x 8, the xor does not use the loaded value */
CM_BEGIN cm_m_l1a1
1:  .rept 8
    lw t0, 0(a1)
    xor t3, t4, t5
    .endr
    CM_LOOP_END

/* (lw, xor, xor, xor) x 8 */
CM_BEGIN cm_m_l1a3
1:  .rept 8
    lw t0, 0(a1)
    xor t3, t4, t5
    xor t6, a2, a3
    xor a4, a5, a6
    .endr
    CM_LOOP_END

/* (lw, lw, xor x 6) x 4 */
CM_BEGIN cm_m_l2a6
1:  .rept 4
    lw t0, 0(a1)
    lw t1, 4(a1)
    xor t3, t4, t5
    xor t6, a2, a3
    xor a4, a5, a6
    xor t3, t4, t5
    xor t6, a2, a3
    xor a4, a5, a6
    .endr
    CM_LOOP_END

/* (lw x 4, xor x 4) x 4 */
CM_BEGIN cm_m_l4a4
1:  .rept 4
    lw t0, 0(a1)
    lw t1, 4(a1)
    lw t2, 8(a1)
    lw a7, 12(a1)
    xor t3, t4, t5
    xor t6, a2, a3
    xor a4, a5, a6
    xor t3, t4, t5
    .endr
    CM_LOOP_END

/* (sw, xor) x 8 */
CM_BEGIN cm_m_s1a1
1:  .rept 8
    sw t0, 0(a1)
    xor t3, t4, t5
    .endr
    CM_LOOP_END

/* (sw, xor, xor, xor) x 8 */
CM_BEGIN cm_m_s1a3
1:  .rept 8
    sw t0, 0(a1)
    xor t3, t4, t5
    xor t6, a2, a3
    xor a4, a5, a6
    .endr
    CM_LOOP_END

/* (lw, xor, xor, use) x 8 : use two instructions after the load */
CM_BEGIN cm_m_l1a2u
1:  .rept 8
    lw t0, 0(a1)
    xor t3, t4, t5
    xor t6, a2, a3
    xor t1, t1, t0
    .endr
    CM_LOOP_END

/* (lw, use, xor, xor) x 8 : use right after the load */
CM_BEGIN cm_m_l1u0a2
1:  .rept 8
    lw t0, 0(a1)
    xor t1, t1, t0
    xor t3, t4, t5
    xor t6, a2, a3
    .endr
    CM_LOOP_END

/* 32 loads in one iteration (is the extra cost per load or per loop?) */
CM_BEGIN cm_m_l32
1:  .rept 8
    lw t0, 0(a1)
    lw t1, 4(a1)
    lw t2, 8(a1)
    lw t3, 12(a1)
    .endr
    CM_LOOP_END

/* 64 xors in one iteration */
CM_BEGIN cm_m_a64
1:  .rept 16
    xor t0, t1, t2
    xor t3, t4, t5
    xor t6, a2, a3
    xor a4, a5, a6
    .endr
    CM_LOOP_END

/* 16 loads relative to sp instead of a1 */
CM_BEGIN cm_m_lsp
    addi sp, sp, -32
1:  .rept 4
    lw t0, 0(sp)
    lw t1, 4(sp)
    lw t2, 8(sp)
    lw t3, 12(sp)
    .endr
    addi a0, a0, -1
    bnez a0, 1b
    addi sp, sp, 32
    ret

/* 16 compressed loads */
    .option rvc
CM_BEGIN cm_m_clw
1:  .rept 4
    c.lw a2, 0(a1)
    c.lw a3, 4(a1)
    c.lw a4, 8(a1)
    c.lw a5, 12(a1)
    .endr
    .option norvc
    CM_LOOP_END

/* (lw, slli/srli/or on other regs ...) Keccak-like: 2 loads, 2 xors with
 * the loads, 6 shift/or -> 10 instructions x 4 */
CM_BEGIN cm_m_keccak_like
1:  .rept 4
    lw t0, 0(a1)
    lw t1, 4(a1)
    xor t0, t0, a2
    xor t1, t1, a3
    srli t3, t0, 5
    slli t0, t0, 27
    srli t4, t1, 5
    slli t1, t1, 27
    xor t0, t0, t4
    xor t1, t1, t3
    .endr
    CM_LOOP_END

/* ---- does the multiplier take the same time for all operands? ---- */
.macro CM_MULTEST name, op, va, vb
CM_BEGIN \name
    li t1, \va
    li t2, \vb
1:  .rept 16
    \op t0, t1, t2
    .endr
    CM_LOOP_END
.endm

CM_MULTEST cm_mul_zero,    mul,   0, 0
CM_MULTEST cm_mul_small,   mul,   3, 5
CM_MULTEST cm_mul_large,   mul,   0xffffffff, 0x7fffffff
CM_MULTEST cm_mulh_zero,   mulh,  0, 0
CM_MULTEST cm_mulh_small,  mulh,  3, 5
CM_MULTEST cm_mulh_large,  mulh,  0x80000000, 0x7fffffff
CM_MULTEST cm_mulh_neg,    mulh,  0xffffffff, 0xffffffff
CM_MULTEST cm_mulhu_large, mulhu, 0xffffffff, 0xffffffff
CM_MULTEST cm_divu_small,  divu,  7, 3
CM_MULTEST cm_divu_large,  divu,  0xffffffff, 3
CM_MULTEST cm_divu_byzero, divu,  12345, 0
