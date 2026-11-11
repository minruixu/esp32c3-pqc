/* Included by slh_sha2_c3.c once per hash length, with NW = n / 4 defined.
 *
 * The two functions here work on several independent hashes at once. The
 * accelerator needs about 80 CPU cycles per compression, and it takes its
 * copy of the message registers when it starts. So while it compresses the
 * block of one hash, the block of the next one is written, and the wait
 * that is left is short. Two WOTS+ chains (or two FORS leaves) are advanced
 * in turn, because the next block of a chain is only known once its
 * previous step has been read back. */

#define C3_CAT_(a, b) a##b
#define C3_CAT(a, b) C3_CAT_(a, b)
#define FN(name) C3_CAT(name, NW)
#define WOTS_LEN (8 * NW + 3) /* lg_w = 4: 2n digits plus 3 checksum digits */

#define LOAD_MID()                         \
    do {                                   \
        UNROLL                             \
        for (unsigned q = 0; q < 8; q++) { \
            H[q] = m[q];                   \
        }                                  \
    } while (0)

#define PUT_DATA(lo5, x)                    \
    do {                                    \
        uint32_t prev_ = (lo5);             \
        UNROLL                              \
        for (unsigned q = 0; q < NW; q++) { \
            uint32_t w_ = (x)[q];           \
            M[5 + q] = prev_ | (w_ << 16);  \
            prev_ = w_ >> 16;               \
        }                                   \
        M[5 + NW] = prev_ | 0x00800000u;    \
    } while (0)

#define READ_OUT(x)                         \
    do {                                    \
        UNROLL                              \
        for (unsigned q = 0; q < NW; q++) { \
            (x)[q] = H[q];                  \
        }                                   \
    } while (0)

/* Algorithm 6, wots_PKgen: all WOTS+ chains of the key pair selected in
 * ADRS, then their compression with T_len. */
static void FN(wots_pkgen)(slh_var_t *var, uint8_t *pk)
{
    uint32_t sk[WOTS_LEN][NW];
    uint32_t m[8], xa[NW], xb[NW];
    const uint32_t *a = var->adrs->u32;
    const uint32_t a2 = a[2], a3 = a[3], a5 = a[5];
    /* word 2 without the type byte; the type sits in bits 8..15 */
    const uint32_t m2 = (a3 >> 24) | (a5 << 16);
    unsigned k;

    sync();
    put(0, (a[0] >> 24) | (a2 << 8));
    put(1, (a2 >> 24) | (a3 << 8));
    put(3, a5 >> 16); /* chain addresses are below 256: their high bytes are zero */
    tail(6 + NW, LENW(22 + 4 * NW));
    UNROLL
    for (unsigned q = 0; q < 8; q++) {
        m[q] = c3.mid[q];
    }

    /* 1. The secret values: PRF with chain address k and hash address 0.
     *    From one block to the next only the chain address changes. */
    M[2] = m2 | (ADRS_WOTS_PRF << 8);
    M[4] = 0;
    UNROLL
    for (unsigned q = 0; q <= NW; q++) {
        M[5 + q] = c3.prf[q];
    }
    for (k = 0; k < WOTS_LEN; k++) {
        LOAD_MID();
        pqc_sha256_hw_start();
        M[4] = (k + 1) << 8; /* next block, while this one is compressed */
        pqc_sha256_hw_wait();
        READ_OUT(sk[k]);
    }

    /* 2. The chains, 15 steps each, two chains at a time. */
    M[2] = m2 | (ADRS_WOTS_HASH << 8);
    for (k = 0; k + 1 < WOTS_LEN; k += 2) {
        const uint32_t ka = k << 8, kb = (k + 1) << 8;

        UNROLL
        for (unsigned q = 0; q < NW; q++) {
            xa[q] = sk[k][q];
            xb[q] = sk[k + 1][q];
        }
        M[4] = ka;
        PUT_DATA(0, xa);
        for (uint32_t lo = 0; lo < (15u << 8); lo += 0x100) {
            LOAD_MID();
            pqc_sha256_hw_start(); /* step of chain a */
            M[4] = kb;
            PUT_DATA(lo, xb);
            pqc_sha256_hw_wait();
            READ_OUT(xa);
            LOAD_MID();
            pqc_sha256_hw_start(); /* step of chain b */
            M[4] = ka;
            PUT_DATA(lo + 0x100, xa); /* unused after the last step */
            pqc_sha256_hw_wait();
            READ_OUT(xb);
        }
        UNROLL
        for (unsigned q = 0; q < NW; q++) {
            sk[k][q] = xa[q];
            sk[k + 1][q] = xb[q];
        }
    }
    /* the number of chains is odd: the last one on its own */
    UNROLL
    for (unsigned q = 0; q < NW; q++) {
        xa[q] = sk[WOTS_LEN - 1][q];
    }
    M[4] = (WOTS_LEN - 1) << 8;
    for (uint32_t lo = 0; lo < (15u << 8); lo += 0x100) {
        LOAD_MID();
        PUT_DATA(lo, xa);
        pqc_sha256_hw_run();
        READ_OUT(xa);
    }
    UNROLL
    for (unsigned q = 0; q < NW; q++) {
        sk[WOTS_LEN - 1][q] = xa[q];
    }
    c3.sh[2] = m2 | (ADRS_WOTS_HASH << 8);
    c3.sh[4] = (WOTS_LEN - 1) << 8;

    /* 3. T_len over the chain ends. */
    adrs_set_type_and_clear_not_kp(var, ADRS_WOTS_PK);
    var->prm->h_t(var, pk, (const uint8_t *)sk, 4 * NW * WOTS_LEN);
}

/* Two neighbouring FORS leaves: tree index ADRS.tree_index (even) and the
 * one after it. Each is PRF then F; the four hashes take turns. */
static void FN(fors_leaf2)(slh_var_t *var, uint8_t *out)
{
    uint32_t m[8], xa[NW], xb[NW];
    uint32_t lo, m2;

    adrs_set_type(var, ADRS_FORS_PRF);
    adrs_set_tree_height(var, 0);
    sync();
    lo = head(var); /* words 0..4; the low half of word 5 ends the tree index */
    tail(6 + NW, LENW(22 + 4 * NW));
    m2 = c3.sh[2] & ~0xff00u;
    UNROLL
    for (unsigned q = 0; q < 8; q++) {
        m[q] = c3.mid[q];
    }

    M[5] = c3.prf[0] | lo;
    UNROLL
    for (unsigned q = 1; q <= NW; q++) {
        M[5 + q] = c3.prf[q];
    }
    LOAD_MID();
    pqc_sha256_hw_start(); /* PRF of leaf a */
    M[5] = c3.prf[0] | (lo + 0x100);
    pqc_sha256_hw_wait();
    READ_OUT(xa);
    LOAD_MID();
    pqc_sha256_hw_start(); /* PRF of leaf b */
    M[2] = m2 | (ADRS_FORS_TREE << 8);
    PUT_DATA(lo, xa);
    pqc_sha256_hw_wait();
    READ_OUT(xb);
    LOAD_MID();
    pqc_sha256_hw_start(); /* F of leaf a */
    PUT_DATA(lo + 0x100, xb);
    pqc_sha256_hw_wait();
    READ_OUT(xa);
    LOAD_MID();
    pqc_sha256_hw_run(); /* F of leaf b */
    READ_OUT(xb);

    c3.sh[2] = m2 | (ADRS_FORS_TREE << 8);
    adrs_set_type(var, ADRS_FORS_TREE);
    store_words(out, xa, NW);
    store_words(out + 4 * NW, xb, NW);
}

/* The chains of Algorithm 8, wots_PKFromSig: chain i runs from hash address
 * start[i] to the end. They have different lengths, so two slots, a and b,
 * are kept busy: when a chain ends, its slot takes the next one. The two
 * slots have their own copy of the code so that both values stay in CPU
 * registers. */
static void FN(wots_chains)(slh_var_t *var, uint8_t *out, const uint8_t *in, const uint32_t *start)
{
    uint32_t m[8], xa[NW], xb[NW];
    uint32_t va = 0, vb = 0, ia = 0, ib = 0, m4 = 0;
    int acta = 0, actb = 0;
    unsigned next = 0;
    const uint32_t *a;
    uint32_t a2, a3, a5;

    adrs_set_type(var, ADRS_WOTS_HASH);
    a = var->adrs->u32;
    a2 = a[2];
    a3 = a[3];
    a5 = a[5];
    sync();
    put(0, (a[0] >> 24) | (a2 << 8));
    put(1, (a2 >> 24) | (a3 << 8));
    put(2, (a3 >> 24) | (ADRS_WOTS_HASH << 8) | (a5 << 16));
    put(3, a5 >> 16);
    tail(6 + NW, LENW(22 + 4 * NW));
    UNROLL
    for (unsigned q = 0; q < 8; q++) {
        m[q] = c3.mid[q];
    }

/* Give slot S the next chain that has steps left; copy through the others. */
#define REFILL(S)                                                         \
    do {                                                                  \
        act##S = 0;                                                       \
        while (next < WOTS_LEN) {                                         \
            unsigned i_ = next++;                                         \
            if (start[i_] >= 15) {                                        \
                memmove(out + 4 * NW * i_, in + 4 * NW * i_, 4 * NW);     \
                continue;                                                 \
            }                                                             \
            load_words(x##S, in + 4 * NW * i_, NW);                       \
            v##S = start[i_];                                             \
            i##S = i_;                                                    \
            act##S = 1;                                                   \
            break;                                                        \
        }                                                                 \
    } while (0)

#define WRITE_BLOCK(S)              \
    do {                            \
        m4 = i##S << 8;             \
        M[4] = m4;                  \
        PUT_DATA(v##S << 8, x##S);  \
    } while (0)

/* One step of slot S, whose block is in the registers. Meanwhile the block
 * of the other slot O is written. Then go on with O, or with S again if O
 * has nothing left. */
#define STEP(S, O)                                              \
    step_##S:                                                   \
    LOAD_MID();                                                 \
    pqc_sha256_hw_start();                                      \
    if (act##O) {                                               \
        WRITE_BLOCK(O);                                         \
    }                                                           \
    pqc_sha256_hw_wait();                                       \
    READ_OUT(x##S);                                             \
    if (++v##S == 15) {                                         \
        store_words(out + 4 * NW * i##S, x##S, NW);             \
        REFILL(S);                                              \
    }                                                           \
    if (!act##O) {                                              \
        if (!act##S) {                                          \
            goto done;                                          \
        }                                                       \
        WRITE_BLOCK(S);                                         \
        goto step_##S;                                          \
    }

    REFILL(a);
    REFILL(b);
    if (!acta) {
        goto done;
    }
    WRITE_BLOCK(a);
    for (;;) {
        STEP(a, b)
        STEP(b, a)
    }
done:
    c3.sh[4] = m4;
#undef STEP
#undef REFILL
#undef WRITE_BLOCK
}

#undef LOAD_MID
#undef PUT_DATA
#undef READ_OUT
#undef WOTS_LEN
#undef FN
#undef C3_CAT
#undef C3_CAT_
