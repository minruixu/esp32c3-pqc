#!/usr/bin/env python3
"""Generator of keccakf1600_rv32imc.S: Keccak-f[1600] for RV32I cores without
bit-manipulation extensions, tuned for the ESP32-C3.

    python3 keccak_gen.py > keccakf1600_rv32imc.S     generate
    python3 keccak_gen.py --check                      verify and count only

Design (see the header of the generated file for the short version):

* The 64-bit lanes stay in their standard little-endian layout, so the
  function is a drop-in `void f(uint64_t state[25])`.
* gp and tp are never touched, so the code is safe with interrupts enabled
  under FreeRTOS. That leaves 27 registers besides the state pointer.
* A round reads one copy of the state and writes the other (the caller's
  buffer and a copy on the stack, alternating), so every word is loaded once
  and stored once per round and no lane ever has to be moved.
* The column parities needed by theta of the next round are accumulated while
  the outputs of chi are still in registers. Five of the ten parity words fit
  in registers; the other five are recomputed from memory.
* Lane complementing (six lanes are kept inverted) removes four of the five
  NOTs per row of chi; RV32I has no and-not instruction.
* On this core a load followed directly by a use of the loaded register
  costs one extra cycle. The generated straight-line code is passed through a
  small scheduler that reorders independent instructions to avoid that.

Everything the generator emits is first executed by the interpreter below and
compared with a plain Python Keccak-f[1600] on random states.
"""
import random
import sys

MASK32 = 0xFFFFFFFF
MASK64 = 0xFFFFFFFFFFFFFFFF

# ---------------------------------------------------------------- reference

RC = []
RHO = [[0] * 5 for _ in range(5)]


def _init_tables():
    r = 1
    for _ in range(24):
        c = 0
        for j in range(7):
            r = ((r << 1) ^ ((r >> 7) * 0x71)) & 0xFF
            if r & 2:
                c ^= 1 << ((1 << j) - 1)
        RC.append(c)
    x, y = 1, 0
    for t in range(24):
        RHO[x][y] = ((t + 1) * (t + 2) // 2) % 64
        x, y = y, (2 * x + 3 * y) % 5


_init_tables()


def rol64(v, n):
    n %= 64
    return ((v << n) | (v >> (64 - n))) & MASK64 if n else v


def keccak_f_ref(a):
    a = list(a)
    for rnd in range(24):
        c = [a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20] for x in range(5)]
        d = [c[(x - 1) % 5] ^ rol64(c[(x + 1) % 5], 1) for x in range(5)]
        b = [0] * 25
        for x in range(5):
            for y in range(5):
                b[y + 5 * ((2 * x + 3 * y) % 5)] = rol64(a[x + 5 * y] ^ d[x], RHO[x][y])
        for y in range(5):
            for x in range(5):
                a[x + 5 * y] = b[x + 5 * y] ^ (~b[(x + 1) % 5 + 5 * y] & b[(x + 2) % 5 + 5 * y] & MASK64)
        a[0] ^= RC[rnd]
    return a


assert RC[0] == 1 and RC[1] == 0x8082 and RC[23] == 0x8000000080008008
assert RHO[1][0] == 1 and RHO[0][1] == 36 and RHO[4][4] == 14

# ---------------------------------------------------------------- layout

# Lanes kept complemented in memory (the XKCP "Bebigokimisa" set).
COMPL = {1, 2, 8, 12, 17, 20}

T0, T1 = "a1", "a2"                                   # scratch
ROW = [("s0", "s1"), ("a3", "a4"), ("a5", "a6"), ("a7", "t0"), ("t1", "t2")]  # (lo, hi) of 5 lanes
DREG = {(0, 0): "s2", (0, 1): "s3", (1, 0): "s4", (1, 1): "s5", (2, 0): "s6",
        (2, 1): "s7", (3, 0): "s8", (3, 1): "s9", (4, 0): "s10", (4, 1): "s11"}
# parity words (column, half) that are accumulated in registers
KREG = {(0, 0): "t3", (0, 1): "t4", (1, 0): "t5", (1, 1): "t6", (2, 0): "ra"}
LOADED = [(2, 1), (3, 0), (3, 1), (4, 0), (4, 1)]     # recomputed from memory

FRAME = 256
E_OFF = 0            # second copy of the state: sp + 0 .. 199
RC_SLOT = 200        # pointer to the next round constant
SAVE_OFF = 204       # ra, s0..s11
SAVED = ["ra"] + ["s%d" % i for i in range(12)]

A_BASE, S_BASE = "a0", "sp"


def word(base, lane, half):
    return (base, (E_OFF if base == S_BASE else 0) + 8 * lane + 4 * half)


# ---------------------------------------------------------------- code blocks

class Block:
    def __init__(self):
        self.ins = []

    def lw(self, rd, mem):
        self.ins.append(("lw", rd, mem[0], mem[1]))

    def sw(self, rs, mem):
        self.ins.append(("sw", rs, mem[0], mem[1]))

    def op(self, name, rd, ra, rb):
        self.ins.append((name, rd, ra, rb))

    def not_(self, rd, ra):
        self.ins.append(("not", rd, ra, None))


def uses_defs(i):
    """registers read, registers written, memory cell read, memory cell written"""
    o = i[0]
    if o == "lw":
        fixed = i[2] in (A_BASE, S_BASE)
        return ([] if fixed else [i[2]]), [i[1]], ((i[2], i[3]) if fixed else None), None
    if o == "sw":
        return [i[1]], [], None, (i[2], i[3])
    if o in ("xor", "or", "and"):
        return [i[2], i[3]], [i[1]], None, None
    if o in ("not", "slli", "srli", "addi"):
        return [i[2]], [i[1]], None, None
    raise ValueError(o)


def schedule(ins):
    """Reorder a straight-line block without changing what it computes, so
    that an instruction does not use a register loaded by its predecessor."""
    n = len(ins)
    preds = [set() for _ in range(n)]
    last_def, readers = {}, {}
    for j, i in enumerate(ins):
        ru, rd, mr, mw = uses_defs(i)
        res_r = list(ru) + ([("m",) + mr] if mr else [])
        res_w = list(rd) + ([("m",) + mw] if mw else [])
        for r in res_r:
            if r in last_def:
                preds[j].add(last_def[r])
            readers.setdefault(r, []).append(j)
        for r in res_w:
            if r in last_def:
                preds[j].add(last_def[r])
            for k in readers.get(r, []):
                if k != j:
                    preds[j].add(k)
            last_def[r] = j
            readers[r] = []
    succs = [[] for _ in range(n)]
    npred = [len(p) for p in preds]
    for j in range(n):
        for p in preds[j]:
            succs[p].append(j)
    ready = sorted(j for j in range(n) if npred[j] == 0)
    out, prev_load_reg = [], None
    stalls = 0
    while ready:
        pick = None
        for j in ready:
            if prev_load_reg is None or prev_load_reg not in uses_defs(ins[j])[0]:
                pick = j
                break
        if pick is None:
            pick = ready[0]
            stalls += 1
        ready.remove(pick)
        out.append(ins[pick])
        prev_load_reg = ins[pick][1] if ins[pick][0] == "lw" else None
        for s in succs[pick]:
            npred[s] -= 1
            if npred[s] == 0:
                ready.append(s)
        ready.sort()
    assert len(out) == n
    return out, stalls


# ---------------------------------------------------------------- chi planning

def chi_term(f0, f1, f2, g):
    """Cheapest way to get out = b0 ^ (~b1 & b2) with complement flag g from
    registers with flags f0, f1, f2: (cost, kind, invert_which, invert_out)."""
    if f1 != f2:
        kind, tflag = ("and", 0) if f1 else ("or", 1)
        inv_out = (f0 ^ tflag) != g
        return (2 + inv_out, kind, None, inv_out)
    tflag = f0 ^ g
    # f1 == f2 == 0: u = ~b1 & b2 ; ~u = b1 | ~b2
    # f1 == f2 == 1: u = r1 & ~r2 ; ~u = ~r1 | r2
    if f1 == 0:
        return (3, "and", 1, False) if tflag == 0 else (3, "or", 2, False)
    return (3, "and", 2, False) if tflag == 0 else (3, "or", 1, False)


def chi_plan(f, g):
    """f: complement flags of the five inputs, g: wanted flags of the outputs.
    Returns (cost, pre, shadow, ops): pre = inputs to invert in place first,
    shadow = an input of which an inverted copy is kept in a scratch register,
    ops[x] = (kind, invert_which, invert_out, role_that_uses_the_shadow)."""
    best = None
    for mask in range(32):
        pre = [x for x in range(5) if mask >> x & 1]
        ff = [f[x] ^ (mask >> x & 1) for x in range(5)]
        for sh in [None, 0, 1, 2, 3, 4]:
            cost, ops = len(pre) + (sh is not None), []
            for x in range(5):
                idx = [x, (x + 1) % 5, (x + 2) % 5]
                cands = []
                for role in [None, 0, 1, 2]:
                    if role is not None and idx[role] != sh:
                        continue
                    fl = [ff[k] for k in idx]
                    if role is not None:
                        fl[role] ^= 1
                    c, kind, inv_in, inv_out = chi_term(fl[0], fl[1], fl[2], g[x])
                    cands.append((c, kind, inv_in, inv_out, role))
                c = min(cands, key=lambda t: t[0])
                cost += c[0]
                ops.append(c[1:])
            if best is None or cost < best[0]:
                best = (cost, pre, sh, ops)
    return best


def column_flags():
    fc = [0] * 5
    for i in COMPL:
        fc[i % 5] ^= 1
    return [fc[(x - 1) % 5] ^ fc[(x + 1) % 5] for x in range(5)]


# ---------------------------------------------------------------- one round

def gen_round(src, dst, rc_off, bump):
    b = Block()
    creg = dict(KREG)
    # parity words that do not have a register of their own
    for idx, (x, h) in enumerate(LOADED):
        r = ROW[idx][0]
        b.lw(r, word(src, x, h))
        for y in range(1, 5):
            t = T0 if y & 1 else T1
            b.lw(t, word(src, x + 5 * y, h))
            b.op("xor", r, r, t)
        creg[(x, h)] = r
    # D[x] = C[x-1] ^ rol(C[x+1], 1)
    for x in range(5):
        for h in (0, 1):
            d = DREG[(x, h)]
            b.ins.append(("slli", d, creg[((x + 1) % 5, h)], 1))
            b.ins.append(("srli", T0, creg[((x + 1) % 5, 1 - h)], 31))
            b.op("xor", d, d, T0)
            b.op("xor", d, d, creg[((x - 1) % 5, h)])
    fd = column_flags()
    for Y in range(5):
        lanes, flags = [], []
        for X in range(5):
            y = X
            x = (3 * (Y - 3 * y)) % 5
            assert (2 * x + 3 * y) % 5 == Y
            lane, n = x + 5 * y, RHO[x][y]
            lo, hi = ROW[X]
            b.lw(lo, word(src, lane, 0))
            b.lw(hi, word(src, lane, 1))
            b.op("xor", lo, lo, DREG[(x, 0)])
            b.op("xor", hi, hi, DREG[(x, 1)])
            if n >= 32:
                lo, hi, n = hi, lo, n - 32
            if n:
                b.ins.append(("srli", T0, lo, 32 - n))
                b.ins.append(("slli", lo, lo, n))
                b.ins.append(("srli", T1, hi, 32 - n))
                b.ins.append(("slli", hi, hi, n))
                b.op("or", lo, lo, T1)
                b.op("or", hi, hi, T0)
            lanes.append((lo, hi))
            flags.append((1 if lane in COMPL else 0) ^ fd[x])
        want = [1 if (X + 5 * Y) in COMPL else 0 for X in range(5)]
        _, pre, shadow, ops = chi_plan(flags, want)
        for X in pre:
            b.not_(lanes[X][0], lanes[X][0])
            b.not_(lanes[X][1], lanes[X][1])
        for h in (0, 1):
            if shadow is not None:
                b.not_(T1, lanes[shadow][h])
            for X in (1, 2, 3, 4, 0):
                rr = [lanes[X][h], lanes[(X + 1) % 5][h], lanes[(X + 2) % 5][h]]
                kind, inv_in, inv_out, role = ops[X]
                if role is not None:
                    rr[role] = T1
                r0, r1, r2 = rr
                if inv_in == 1:
                    b.not_(T0, r1)
                    b.op(kind, T0, T0, r2)
                elif inv_in == 2:
                    b.not_(T0, r2)
                    b.op(kind, T0, r1, T0)
                else:
                    b.op(kind, T0, r1, r2)
                key = (X, h)
                out = KREG[key] if (key in KREG and Y == 0) else T0
                b.op("xor", out, r0, T0)
                if inv_out:
                    b.not_(out, out)
                if X == 0 and Y == 0:
                    # iota; lanes 3 and 4 of this half are no longer needed
                    p, c = lanes[3][h], lanes[4][h]
                    b.lw(p, (S_BASE, RC_SLOT))
                    b.ins.append(("lw", c, p, rc_off + 4 * h))
                    b.op("xor", out, out, c)
                    if h == 1 and bump:
                        b.ins.append(("addi", p, p, bump))
                        b.sw(p, (S_BASE, RC_SLOT))
                b.sw(out, word(dst, X + 5 * Y, h))
                if key in KREG and Y > 0:
                    b.op("xor", KREG[key], KREG[key], out)
    return b.ins


def gen_entry():
    b = Block()
    for lane in sorted(COMPL):
        for h in (0, 1):
            r = ROW[h][0] if lane % 2 else ROW[h][1]
            b.lw(r, word(A_BASE, lane, h))
            b.not_(r, r)
            b.sw(r, word(A_BASE, lane, h))
    for (x, h), k in KREG.items():
        b.lw(k, word(A_BASE, x, h))
        for y in range(1, 5):
            t = T0 if y & 1 else T1
            b.lw(t, word(A_BASE, x + 5 * y, h))
            b.op("xor", k, k, t)
    return b.ins


def gen_exit():
    b = Block()
    for lane in sorted(COMPL):
        for h in (0, 1):
            r = ROW[h][0] if lane % 2 else ROW[h][1]
            b.lw(r, word(A_BASE, lane, h))
            b.not_(r, r)
            b.sw(r, word(A_BASE, lane, h))
    return b.ins


# ---------------------------------------------------------------- interpreter

RC_ADDR, A_ADDR, SP_ADDR = 0x20000, 0x1000, 0x8000


def run(ins, reg, mem):
    for i in ins:
        o = i[0]
        if o == "lw":
            reg[i[1]] = mem[reg[i[2]] + i[3]]
        elif o == "sw":
            mem[reg[i[2]] + i[3]] = reg[i[1]]
        elif o == "xor":
            reg[i[1]] = reg[i[2]] ^ reg[i[3]]
        elif o == "or":
            reg[i[1]] = reg[i[2]] | reg[i[3]]
        elif o == "and":
            reg[i[1]] = reg[i[2]] & reg[i[3]]
        elif o == "not":
            reg[i[1]] = reg[i[2]] ^ MASK32
        elif o == "slli":
            reg[i[1]] = (reg[i[2]] << i[3]) & MASK32
        elif o == "srli":
            reg[i[1]] = reg[i[2]] >> i[3]
        elif o == "addi":
            reg[i[1]] = (reg[i[2]] + i[3]) & MASK32
        else:
            raise ValueError(o)


def simulate(entry, ra, rb, exit_, state):
    reg = {r: random.getrandbits(32) for r in
           ["ra", "t0", "t1", "t2", "t3", "t4", "t5", "t6"] + ["s%d" % i for i in range(12)] +
           ["a%d" % i for i in range(1, 8)]}
    reg["a0"], reg["sp"] = A_ADDR, SP_ADDR
    mem = {}
    for i, v in enumerate(state):
        mem[A_ADDR + 8 * i] = v & MASK32
        mem[A_ADDR + 8 * i + 4] = v >> 32
    for i in range(50):
        mem[SP_ADDR + E_OFF + 4 * i] = random.getrandbits(32)
    for i, c in enumerate(RC):
        mem[RC_ADDR + 8 * i] = c & MASK32
        mem[RC_ADDR + 8 * i + 4] = c >> 32
    mem[RC_ADDR + 8 * 24] = 0
    mem[SP_ADDR + RC_SLOT] = RC_ADDR
    run(entry, reg, mem)
    n = 0
    while True:
        run(ra, reg, mem)
        run(rb, reg, mem)
        n += 1
        if mem[mem[SP_ADDR + RC_SLOT]] == 0:
            break
    assert n == 12
    run(exit_, reg, mem)
    return [mem[A_ADDR + 8 * i] | (mem[A_ADDR + 8 * i + 4] << 32) for i in range(25)]


# ---------------------------------------------------------------- output

def fmt(i):
    o = i[0]
    if o in ("lw", "sw"):
        return "    %s %s, %d(%s)" % (o, i[1], i[3], i[2])
    if o in ("xor", "or", "and"):
        return "    %s %s, %s, %s" % (o, i[1], i[2], i[3])
    if o == "not":
        return "    not %s, %s" % (i[1], i[2])
    return "    %s %s, %s, %d" % (o, i[1], i[2], i[3])


HEADER = '''/* Keccak-f[1600] for RV32I cores without bit-manipulation extensions,
 * tuned for the ESP32-C3. GENERATED by keccak_gen.py; do not edit by hand.
 *
 *   void pqc_keccakf1600_opt(uint64_t state[25]);
 *
 * - standard lane layout, so it replaces any C permutation directly;
 * - gp and tp are not used, so it is safe with interrupts enabled;
 * - every state word is loaded once and stored once per round (the rounds
 *   alternate between the caller's buffer and a copy on the stack);
 * - the column parities of the next round are collected while the outputs
 *   of chi are still in registers;
 * - six lanes are kept complemented, which removes 4 of the 5 NOTs per row;
 * - no instruction uses a register loaded by the instruction before it,
 *   which costs an extra cycle on this core.
 *
 * %d instructions per permutation, %d bytes of stack.
 */
'''


def main():
    random.seed(1)
    entry, _ = schedule(gen_entry())
    ra, s1 = schedule(gen_round(A_BASE, S_BASE, 0, 0))
    rb, s2 = schedule(gen_round(S_BASE, A_BASE, 8, 16))
    exit_, _ = schedule(gen_exit())
    for trial in range(40):
        st = [random.getrandbits(64) for _ in range(25)]
        if trial == 0:
            st = [0] * 25
        if trial == 1:
            st = [MASK64] * 25
        assert simulate(entry, ra, rb, exit_, st) == keccak_f_ref(st), "mismatch"
    per_perm = len(entry) + 12 * (len(ra) + len(rb) + 3) + len(exit_) + 2 * len(SAVED) + 8
    if "--check" in sys.argv:
        print("ok: round A %d insns (%d forced load-use), round B %d (%d); %d per permutation"
              % (len(ra), s1, len(rb), s2, per_perm))
        return
    out = [HEADER % (per_perm, FRAME)]
    out.append("#ifdef PQC_KECCAK_IRAM\n    .section .iram1.pqc_keccak, \"ax\"\n#else\n    .text\n#endif")
    out.append("/* A static library member is only linked when something refers to it, and")
    out.append(" * the weak default of pqc_keccakf1600_opt already satisfies every caller.")
    out.append(" * components/prims is therefore linked as a whole archive. The symbol below")
    out.append(" * is kept for build files that still name it with `-u`. */")
    out.append("    .globl pqc_keccakf1600_rv32imc_present")
    out.append("    .set pqc_keccakf1600_rv32imc_present, 1")
    out.append("    .globl pqc_keccakf1600_opt")
    out.append("    .type pqc_keccakf1600_opt, %function")
    out.append("    .balign 4")
    out.append("pqc_keccakf1600_opt:")
    out.append("    addi sp, sp, -%d" % FRAME)
    for k, r in enumerate(SAVED):
        out.append("    sw %s, %d(sp)" % (r, SAVE_OFF + 4 * k))
    out.append("    la %s, pqc_keccak_rc" % T0)
    out.append("    sw %s, %d(sp)" % (T0, RC_SLOT))
    out += [fmt(i) for i in entry]
    out.append("    .balign 4")
    out.append("1:")
    out.append("    /* even round: caller's buffer -> stack copy */")
    out += [fmt(i) for i in ra]
    out.append("    /* odd round: stack copy -> caller's buffer */")
    out += [fmt(i) for i in rb]
    out.append("    lw %s, %d(sp)" % (T0, RC_SLOT))
    out.append("    lw %s, 0(%s)" % (T1, T0))
    out.append("    bnez %s, 1b" % T1)
    out += [fmt(i) for i in exit_]
    for k, r in enumerate(SAVED):
        out.append("    lw %s, %d(sp)" % (r, SAVE_OFF + 4 * k))
    out.append("    addi sp, sp, %d" % FRAME)
    out.append("    ret")
    out.append("    .size pqc_keccakf1600_opt, .-pqc_keccakf1600_opt")
    out.append("")
    out.append("    .section .rodata.pqc_keccak_rc, \"a\"")
    out.append("    .balign 4")
    out.append("pqc_keccak_rc:")
    for c in RC:
        out.append("    .word 0x%08x, 0x%08x" % (c & MASK32, c >> 32))
    out.append("    .word 0    /* end marker */")
    print("\n".join(out))


if __name__ == "__main__":
    main()
