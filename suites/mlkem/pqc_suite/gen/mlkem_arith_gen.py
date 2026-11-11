#!/usr/bin/env python3
"""Generator of mlkem_arith_rv32imc.S: ML-KEM polynomial arithmetic for the
ESP32-C3 (RV32IMC), plugged into mlkem-native's native-backend interface.

    python3 mlkem_arith_gen.py > ../mlkem_arith_rv32imc.S    generate
    python3 mlkem_arith_gen.py --check                       verify and count

What is specific to this core (see suites/keccak/NOTES.md for the measured
instruction costs):

* `mul` takes one cycle but `mulh` takes five. All modular multiplications
  therefore use Plantard arithmetic written with `mul` and `srai` only
  (5 single-cycle instructions for a multiplication by a constant), instead
  of the usual forms that need the high half of a product.
* A load followed directly by a use of the value costs an extra cycle; the
  scheduler in rvgen.py keeps the two apart.
* gp and tp are not used, so interrupts can stay enabled.

Structure: the forward NTT merges layers 1-4 (16 coefficients in registers)
and layers 5-7; no reduction is needed in between because Plantard outputs
are smaller than q/2. The inverse NTT merges layers 7-5 and 4-1 and folds the
final scaling into the twiddle factors, so only 48 extra multiplications are
needed instead of 128. The base multiplication accumulates 32-bit products
of all vector entries and reduces once per coefficient.

Every routine is executed in the interpreter of rvgen.py and compared with a
Python model of the reference algorithm before it is printed.
"""
import random
import sys

from rvgen import Sim, emit, function, regs_of, s32, schedule

Q = 3329
N = 256
QINV32 = pow(Q, -1, 1 << 32)
ALPHA = 3


def brv7(k):
    return int("{:07b}".format(k)[::-1], 2)


Z = [pow(17, brv7(k), Q) for k in range(128)]       # zetas, normal domain
F_INTT = pow(2, 9, Q)                                # 2^16 / 128


def centered(x):
    x %= Q
    return x - Q if x > Q // 2 else x


def plant_const(c):
    """32-bit constant W such that plant(b, W) == b * c (mod q)."""
    return (centered(c * (-(1 << 32))) * QINV32) & 0xFFFFFFFF


def plant_model(b, w):
    x = s32(b * w)
    return (((x >> 16) + (1 << ALPHA)) * Q) >> 16


def montgomery_reduce_model(a):
    t = s32(((a * pow(Q, -1, 1 << 16)) & 0xFFFF) << 16) >> 16
    return (a - t * Q) >> 16


# ------------------------------------------------------------------ registers

COEF = ["s%d" % i for i in range(12)] + ["a2", "a3", "a4", "a5"]
P, ZP, QR, END = "a0", "a1", "a6", "a7"
TMP = ["t0", "t1"]
WREG = ["t2", "t3", "t4", "t5"]


class Emit:
    def __init__(self):
        self.ins = []
        self.t = 0
        self.w = 0

    def tmp(self):
        self.t += 1
        return TMP[self.t % len(TMP)]

    def wreg(self):
        self.w += 1
        return WREG[self.w % len(WREG)]

    def plant(self, dst, src, w):
        """dst = src * const(w) mod q, |dst| <= q/2"""
        self.ins += [("mul", dst, src, w), ("srai", dst, dst, 16), ("addi", dst, dst, 1 << ALPHA),
                     ("mul", dst, dst, QR), ("srai", dst, dst, 16)]

    def ct(self, a, b, w):
        t = self.tmp()
        self.plant(t, b, w)
        self.ins += [("sub", b, a, t), ("add", a, a, t)]

    def gs(self, a, b, w):
        t = self.tmp()
        self.ins += [("sub", t, b, a), ("add", a, a, b)]
        self.plant(b, t, w)


# ------------------------------------------------------------------ NTT

def ntt_tables():
    w1 = [Z[k] for k in range(1, 16)]
    w2 = []
    for b in range(16):
        w2 += [Z[16 + b], Z[32 + 2 * b], Z[32 + 2 * b + 1]] + [Z[64 + 4 * b + g] for g in range(4)]
    return [plant_const(c) for c in w1 + w2]


def gen_ntt():
    prog = [("li", QR, Q), ("la", ZP, "pqc_mlkem_ntt_w"), ("addi", END, P, 32), ("label", ".Lntt_1")]
    e = Emit()
    for i in range(16):
        e.ins.append(("lh", COEF[i], P, 32 * i))
    widx = 0
    for layer in range(4):
        half = 8 >> layer
        for g in range(1 << layer):
            w = e.wreg()
            e.ins.append(("lw", w, ZP, 4 * widx))
            widx += 1
            for i in range(half):
                e.ct(COEF[2 * half * g + i], COEF[2 * half * g + half + i], w)
    for i in range(16):
        e.ins.append(("sh", COEF[i], P, 32 * i))
    e.ins.append(("addi", P, P, 2))
    prog += schedule(e.ins, readonly_bases=[ZP])
    prog += [("bne", P, END, ".Lntt_1"),
             ("addi", P, P, -32), ("addi", ZP, ZP, 60), ("addi", END, P, 512), ("label", ".Lntt_2")]
    e = Emit()
    for i in range(16):
        e.ins.append(("lh", COEF[i], P, 2 * i))
    widx = 0
    for layer in range(3):
        half = 8 >> layer
        for g in range(1 << layer):
            w = e.wreg()
            e.ins.append(("lw", w, ZP, 4 * widx))
            widx += 1
            for i in range(half):
                e.ct(COEF[2 * half * g + i], COEF[2 * half * g + half + i], w)
    for i in range(16):
        e.ins.append(("sh", COEF[i], P, 2 * i))
    e.ins += [("addi", P, P, 32), ("addi", ZP, ZP, 28)]
    prog += schedule(e.ins, readonly_bases=[ZP])
    prog += [("bne", P, END, ".Lntt_2")]
    return function("pqc_mlkem_ntt", prog, regs_of(prog))


def ntt_ref(r):
    r = list(r)
    k, length = 1, 128
    while length >= 2:
        for start in range(0, N, 2 * length):
            z = Z[k]
            k += 1
            for j in range(start, start + length):
                t = z * r[j + length]
                r[j + length] = (r[j] - t) % Q
                r[j] = (r[j] + t) % Q
        length >>= 1
    return r


# ------------------------------------------------------------------ inverse NTT

def intt_tables():
    w1 = []
    for b in range(16):
        w1 += [Z[127 - (4 * b + g)] for g in range(4)] + [Z[63 - (2 * b + g)] for g in range(2)] + [Z[31 - b]]
    w2 = []
    for g in range(8):
        w2.append(Z[15 - g] * F_INTT)
    for g in range(4):
        w2 += [Z[7 - g] * F_INTT, Z[7 - g]]
    for g in range(2):
        w2 += [Z[3 - g] * F_INTT, Z[3 - g]]
    w2 += [Z[1] * F_INTT, Z[1]]
    return [plant_const(c) for c in w1 + w2]


ONE, FREG = "t6", "ra"


def gen_intt():
    prog = [("li", QR, Q), ("la", ZP, "pqc_mlkem_intt_w"), ("li", ONE, plant_const(1)),
            ("addi", END, P, 512), ("label", ".Lintt_1")]
    e = Emit()
    for i in range(16):
        e.ins.append(("lh", COEF[i], P, 2 * i))
    widx = 0
    for g in range(4):                       # layer 7: length 2
        w = e.wreg()
        e.ins.append(("lw", w, ZP, 4 * widx))
        widx += 1
        for i in range(2):
            e.gs(COEF[4 * g + i], COEF[4 * g + 2 + i], w)
    for g in range(2):                       # layer 6: length 4
        w = e.wreg()
        e.ins.append(("lw", w, ZP, 4 * widx))
        widx += 1
        for i in range(4):
            e.gs(COEF[8 * g + i], COEF[8 * g + 4 + i], w)
    w = e.wreg()                             # layer 5: length 8
    e.ins.append(("lw", w, ZP, 4 * widx))
    for i in range(8):
        e.gs(COEF[i], COEF[8 + i], w)
    # coefficients 0 and 1 are plain sums of 8 inputs: bring them back to 16 bits
    e.plant(COEF[0], COEF[0], ONE)
    e.plant(COEF[1], COEF[1], ONE)
    for i in range(16):
        e.ins.append(("sh", COEF[i], P, 2 * i))
    e.ins += [("addi", P, P, 32), ("addi", ZP, ZP, 28)]
    prog += schedule(e.ins, readonly_bases=[ZP])
    prog += [("bne", P, END, ".Lintt_1"),
             ("addi", P, P, -512), ("li", FREG, plant_const(F_INTT)), ("addi", END, P, 32),
             ("label", ".Lintt_2")]
    e = Emit()
    for i in range(16):
        e.ins.append(("lh", COEF[i], P, 32 * i))
    widx = 0

    def load():
        nonlocal widx
        w = e.wreg()
        e.ins.append(("lw", w, ZP, 4 * widx))
        widx += 1
        return w

    for g in range(8):                       # layer 4: length 16
        e.gs(COEF[2 * g], COEF[2 * g + 1], load())
    for g in range(4):                       # layer 3: length 32
        wf, w = load(), load()
        e.gs(COEF[4 * g], COEF[4 * g + 2], wf)
        e.gs(COEF[4 * g + 1], COEF[4 * g + 3], w)
    for g in range(2):                       # layer 2: length 64
        wf, w = load(), load()
        e.gs(COEF[8 * g], COEF[8 * g + 4], wf)
        for i in range(1, 4):
            e.gs(COEF[8 * g + i], COEF[8 * g + 4 + i], w)
    wf, w = load(), load()                   # layer 1: length 128
    e.gs(COEF[0], COEF[8], wf)
    for i in range(1, 8):
        e.gs(COEF[i], COEF[8 + i], w)
    e.plant(COEF[0], COEF[0], FREG)          # the only path without a twiddle factor
    for i in range(16):
        e.ins.append(("sh", COEF[i], P, 32 * i))
    e.ins.append(("addi", P, P, 2))
    prog += schedule(e.ins, readonly_bases=[ZP])
    prog += [("bne", P, END, ".Lintt_2")]
    return function("pqc_mlkem_intt_tomont", prog, regs_of(prog))


def intt_ref(r):
    r = list(r)
    length = 2
    k = 127
    while length <= 128:
        for start in range(0, N, 2 * length):
            z = Z[k]
            k -= 1
            for j in range(start, start + length):
                t = r[j]
                r[j] = (t + r[j + length]) % Q
                r[j + length] = (r[j + length] - t) * z % Q
        length <<= 1
    return [x * F_INTT % Q for x in r]


# ------------------------------------------------------------------ base multiplication

def gen_basemul(k):
    """r = sum over the k vector entries of a_v * b_v in the NTT domain, with
    the factor 2^-16 of a Montgomery reduction. a0 = r, a1 = a, a2 = b,
    a3 = cache. Two coefficient pairs per loop iteration."""
    R, A, B, C = "a0", "a1", "a2", "a3"
    QS, QQ, ENDR = "a4", "a5", "a6"
    acc = ["t0", "t1", "t2", "t3"]
    ld = ["s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9"]
    prod = ["a7", "t4", "t5", "t6"]
    name = "pqc_mlkem_basemul_acc_k%d" % k
    lab = ".Lbm%d" % k
    prog = [("li", QS, (pow(Q, -1, 1 << 16) << 16) & 0xFFFFFFFF), ("li", QQ, Q),
            ("addi", ENDR, R, 512), ("label", lab)]
    ins = []
    for pair in range(2):
        t0, t1 = acc[2 * pair], acc[2 * pair + 1]
        for v in range(k):
            a0, a1, b0, b1, c = ld[5 * pair:5 * pair + 5]
            ins += [("lh", a0, A, 512 * v + 4 * pair), ("lh", a1, A, 512 * v + 4 * pair + 2),
                    ("lh", b0, B, 512 * v + 4 * pair), ("lh", b1, B, 512 * v + 4 * pair + 2),
                    ("lh", c, C, 256 * v + 2 * pair)]
            p0, p1 = prod[2 * pair], prod[2 * pair + 1]
            if v == 0:
                ins += [("mul", t0, a1, c), ("mul", p0, a0, b0), ("mul", t1, a0, b1), ("mul", p1, a1, b0),
                        ("add", t0, t0, p0), ("add", t1, t1, p1)]
            else:
                ins += [("mul", p0, a1, c), ("mul", p1, a0, b0), ("add", t0, t0, p0), ("add", t0, t0, p1),
                        ("mul", p0, a0, b1), ("mul", p1, a1, b0), ("add", t1, t1, p0), ("add", t1, t1, p1)]
        for j, t in enumerate((t0, t1)):
            p = prod[2 * pair + j]
            # Montgomery reduction with R = 2^16, using only `mul`
            ins += [("mul", p, t, QS), ("srai", p, p, 16), ("mul", p, p, QQ), ("sub", t, t, p),
                    ("srai", t, t, 16), ("sh", t, R, 4 * pair + 2 * j)]
    ins += [("addi", A, A, 8), ("addi", B, B, 8), ("addi", C, C, 4), ("addi", R, R, 8)]
    prog += schedule(ins)
    prog += [("bne", R, ENDR, lab)]
    return function(name, prog, regs_of(prog))


def basemul_ref(a, b, cache, k):
    r = []
    for i in range(N // 2):
        t0 = t1 = 0
        for v in range(k):
            a0, a1 = a[v][2 * i], a[v][2 * i + 1]
            b0, b1 = b[v][2 * i], b[v][2 * i + 1]
            t0 += a1 * cache[v][i] + a0 * b0
            t1 += a0 * b1 + a1 * b0
        r += [montgomery_reduce_model(t0), montgomery_reduce_model(t1)]
    return r


# ------------------------------------------------------------------ rejection sampling

def gen_rej_uniform():
    """unsigned f(int16_t *r, unsigned len, const uint8_t *buf, unsigned buflen)
    Appends to r the 12-bit little-endian fields of buf that are below q, in
    order, until len values are stored or buf is used up; returns the count.
    buf must be word aligned and buflen a multiple of 12 (the C wrapper falls
    back to the portable code otherwise). The input is public (it expands the
    public matrix), so the control flow may depend on it, as in the C code.

    Main loop: 12 bytes = 8 candidates. Each candidate is stored
    unconditionally and the output pointer advances only if it was below q,
    so there is no branch per candidate. It runs while 8 more values fit."""
    R, RE, B, BE, R0, LIM, QQ = "a0", "a1", "a2", "a3", "a4", "a5", "a6"
    prog = [("mv", R0, R), ("slli", RE, RE, 1), ("add", RE, R, RE), ("add", BE, B, BE),
            ("addi", LIM, RE, -16), ("li", QQ, Q),
            ("beq", B, BE, ".Lrej_tail"), ("bltu", LIM, R, ".Lrej_tail"), ("label", ".Lrej_fast")]
    w0, w1, w2 = "t0", "t1", "t2"
    ins = [("lw", w0, B, 0), ("lw", w1, B, 4), ("lw", w2, B, 8)]
    cand = []

    def field(dst, word, shl):          # 12 bits starting at bit (20 - shl) of word
        ins.extend([("slli", dst, word, shl), ("srli", dst, dst, 20)])

    c = ["t3", "t4", "t5", "t6", "a7", "t3", "t4", "t5"]
    t = "s1"   # not used: all temporaries below are caller-saved
    # candidates in stream order; emitted one after the other so that the
    # pointer updates stay sequential
    def append(v, flag):
        ins.extend([("sh", v, R, 0), ("sltu", flag, v, QQ), ("slli", flag, flag, 1), ("add", R, R, flag)])

    field("t3", w0, 20)
    append("t3", "t6")
    field("t4", w0, 8)
    append("t4", "t6")
    ins.extend([("srli", "t3", w0, 24), ("slli", "t5", w1, 28), ("srli", "t5", "t5", 20), ("or", "t3", "t3", "t5")])
    append("t3", "t6")
    field("t4", w1, 16)
    append("t4", "t6")
    field("t3", w1, 4)
    append("t3", "t6")
    ins.extend([("srli", "t4", w1, 28), ("slli", "t5", w2, 24), ("srli", "t5", "t5", 20), ("or", "t4", "t4", "t5")])
    append("t4", "t6")
    field("t3", w2, 12)
    append("t3", "t6")
    ins.append(("srli", "t4", w2, 20))
    append("t4", "t6")
    ins.append(("addi", B, B, 12))
    prog += schedule(ins)
    prog += [("bltu", LIM, R, ".Lrej_tail"), ("bne", B, BE, ".Lrej_fast"),
             ("label", ".Lrej_tail"),
             ("beq", R, RE, ".Lrej_done"), ("beq", B, BE, ".Lrej_done"),
             ("lbu", "t0", B, 0), ("lbu", "t1", B, 1), ("lbu", "t2", B, 2), ("addi", B, B, 3),
             ("slli", "t3", "t1", 8), ("or", "t3", "t3", "t0"), ("slli", "t3", "t3", 20), ("srli", "t3", "t3", 20),
             ("srli", "t4", "t1", 4), ("slli", "t5", "t2", 4), ("or", "t4", "t4", "t5"),
             ("bgeu", "t3", QQ, ".Lrej_second"),
             ("sh", "t3", R, 0), ("addi", R, R, 2),
             ("label", ".Lrej_second"),
             ("beq", R, RE, ".Lrej_done"),
             ("bgeu", "t4", QQ, ".Lrej_tail"),
             ("sh", "t4", R, 0), ("addi", R, R, 2),
             ("j", ".Lrej_tail"),
             ("label", ".Lrej_done"),
             ("sub", "a0", R, R0), ("srli", "a0", "a0", 1)]
    return function("pqc_mlkem_rej_uniform", prog, set())


def rej_ref(buf, length):
    out, pos = [], 0
    while len(out) < length and pos + 3 <= len(buf):
        v0 = (buf[pos] | (buf[pos + 1] << 8)) & 0xFFF
        v1 = ((buf[pos + 1] >> 4) | (buf[pos + 2] << 4)) & 0xFFF
        pos += 3
        if v0 < Q:
            out.append(v0)
        if len(out) < length and v1 < Q:
            out.append(v1)
    return out


# ------------------------------------------------------------------ byte packing

def gen_tobytes():
    """void f(uint8_t r[384], const int16_t a[256]): pack 12-bit coefficients
    (in [0, q)), 8 coefficients into three 32-bit words per step. r must be
    word aligned (the C wrapper falls back to the portable code otherwise)."""
    R, A, EN = "a0", "a1", "a2"
    c = ["t0", "t1", "t2", "t3", "t4", "t5", "t6", "a3"]
    w = ["a4", "a5", "a6"]
    t = "a7"
    prog = [("addi", EN, A, 512), ("label", ".Ltobytes")]
    ins = [("lhu", c[i], A, 2 * i) for i in range(8)]
    ins += [("slli", w[0], c[1], 12), ("or", w[0], w[0], c[0]), ("slli", t, c[2], 24), ("or", w[0], w[0], t),
            ("sw", w[0], R, 0),
            ("srli", w[1], c[2], 8), ("slli", t, c[3], 4), ("or", w[1], w[1], t), ("slli", t, c[4], 16),
            ("or", w[1], w[1], t), ("slli", t, c[5], 28), ("or", w[1], w[1], t), ("sw", w[1], R, 4),
            ("srli", w[2], c[5], 4), ("slli", t, c[6], 8), ("or", w[2], w[2], t), ("slli", t, c[7], 20),
            ("or", w[2], w[2], t), ("sw", w[2], R, 8),
            ("addi", A, A, 16), ("addi", R, R, 12)]
    prog += schedule(ins) + [("bne", A, EN, ".Ltobytes")]
    return function("pqc_mlkem_tobytes", prog, set())


def gen_frombytes():
    """void f(int16_t r[256], const uint8_t a[384]): unpack 12-bit fields,
    three 32-bit words into 8 coefficients per step. a must be word aligned."""
    R, A, EN = "a0", "a1", "a2"
    w0, w1, w2 = "t0", "t1", "t2"
    prog = [("addi", EN, R, 512), ("label", ".Lfrombytes")]
    ins = [("lw", w0, A, 0), ("lw", w1, A, 4), ("lw", w2, A, 8)]

    def field(dst, word, shl, k):
        ins.extend([("slli", dst, word, shl), ("srli", dst, dst, 20), ("sh", dst, R, 2 * k)])

    field("t3", w0, 20, 0)
    field("t4", w0, 8, 1)
    ins.extend([("srli", "t5", w0, 24), ("slli", "t6", w1, 28), ("srli", "t6", "t6", 20), ("or", "t5", "t5", "t6"),
                ("sh", "t5", R, 4)])
    field("a3", w1, 16, 3)
    field("a4", w1, 4, 4)
    ins.extend([("srli", "a5", w1, 28), ("slli", "a6", w2, 24), ("srli", "a6", "a6", 20), ("or", "a5", "a5", "a6"),
                ("sh", "a5", R, 10)])
    field("a7", w2, 12, 6)
    ins.extend([("srli", "t3", w2, 20), ("sh", "t3", R, 14)])
    ins += [("addi", A, A, 12), ("addi", R, R, 16)]
    prog += schedule(ins) + [("bne", R, EN, ".Lfrombytes")]
    return function("pqc_mlkem_frombytes", prog, set())


def tobytes_ref(a):
    out = bytearray()
    for i in range(N // 2):
        t0, t1 = a[2 * i], a[2 * i + 1]
        out += bytes([t0 & 0xFF, (t0 >> 8) | ((t1 << 4) & 0xF0), t1 >> 4])
    return bytes(out)


# ------------------------------------------------------------------ simple loops

def gen_tomont():
    """p[i] = p[i] * 2^16 mod q, 8 coefficients per iteration"""
    prog = [("li", QR, Q), ("li", "t6", plant_const(1 << 16)), ("addi", END, P, 512), ("label", ".Ltomont")]
    ins = []
    regs = COEF[:8]
    for i, r in enumerate(regs):
        ins.append(("lh", r, P, 2 * i))
    for i, r in enumerate(regs):
        ins += [("mul", r, r, "t6"), ("srai", r, r, 16), ("addi", r, r, 1 << ALPHA), ("mul", r, r, QR),
                ("srai", r, r, 16), ("sh", r, P, 2 * i)]
    ins.append(("addi", P, P, 16))
    prog += schedule(ins) + [("bne", P, END, ".Ltomont")]
    return function("pqc_mlkem_tomont", prog, regs_of(prog))


def gen_reduce():
    """p[i] = p[i] mod q in [0, q), 8 coefficients per iteration.
    One Plantard multiplication by 1 brings any 16-bit value into
    (-q/2, q/2); adding q to the negative ones gives the canonical value."""
    prog = [("li", QR, Q), ("li", "t6", plant_const(1)), ("addi", END, P, 512), ("label", ".Lreduce")]
    ins = []
    regs = COEF[:8]
    tmps = ["t0", "t1", "t2", "t3"]
    for i, r in enumerate(regs):
        ins.append(("lh", r, P, 2 * i))
    for i, r in enumerate(regs):
        t = tmps[i % 4]
        ins += [("mul", r, r, "t6"), ("srai", r, r, 16), ("addi", r, r, 1 << ALPHA), ("mul", r, r, QR),
                ("srai", r, r, 16), ("srai", t, r, 31), ("and", t, t, QR), ("add", r, r, t),
                ("sh", r, P, 2 * i)]
    ins.append(("addi", P, P, 16))
    prog += schedule(ins) + [("bne", P, END, ".Lreduce")]
    return function("pqc_mlkem_reduce", prog, regs_of(prog))


def mulcache_table():
    t = []
    for i in range(64):
        t += [Z[64 + i], -Z[64 + i]]
    return [plant_const(c) for c in t]


def gen_mulcache():
    """cache[i] = b[2i+1] * (+-zeta) mod q; a0 = cache, a1 = b. 8 per iteration."""
    CA, BP, ZT, EN = "a0", "a1", "a2", "a7"
    prog = [("li", QR, Q), ("la", ZT, "pqc_mlkem_mulcache_w"), ("addi", EN, CA, 256), ("label", ".Lmulcache")]
    ins = []
    regs = COEF[:8]
    ws = ["t0", "t1", "t2", "t3", "t4", "t5", "t6", "ra"]
    for i, r in enumerate(regs):
        ins += [("lh", r, BP, 4 * i + 2), ("lw", ws[i], ZT, 4 * i)]
    for i, r in enumerate(regs):
        ins += [("mul", r, r, ws[i]), ("srai", r, r, 16), ("addi", r, r, 1 << ALPHA), ("mul", r, r, QR),
                ("srai", r, r, 16), ("sh", r, CA, 2 * i)]
    ins += [("addi", CA, CA, 16), ("addi", BP, BP, 32), ("addi", ZT, ZT, 32)]
    prog += schedule(ins, readonly_bases=[ZT]) + [("bne", CA, EN, ".Lmulcache")]
    return function("pqc_mlkem_mulcache", prog, regs_of(prog))


# ------------------------------------------------------------------ checks

SYM = {"pqc_mlkem_ntt_w": 0x40000, "pqc_mlkem_intt_w": 0x41000, "pqc_mlkem_mulcache_w": 0x42000}
POLY = 0x10000


def load_tables(sim):
    for name, tab in (("pqc_mlkem_ntt_w", ntt_tables()), ("pqc_mlkem_intt_w", intt_tables()),
                      ("pqc_mlkem_mulcache_w", mulcache_table())):
        for i, w in enumerate(tab):
            sim.write(SYM[name] + 4 * i, w, 4)


def put_poly(sim, addr, coeffs):
    for i, c in enumerate(coeffs):
        sim.write(addr + 2 * i, c & 0xFFFF, 2)


def get_poly(sim, addr, n=N):
    return [sim.read(addr + 2 * i, 2, True) for i in range(n)]


BASE_REGS = {"sp": 0x80000, "ra": 0}
for _r in ["s%d" % i for i in range(12)] + ["t%d" % i for i in range(7)] + ["a%d" % i for i in range(8)]:
    BASE_REGS.setdefault(_r, 0xDEAD0000)


def check():
    random.seed(2)
    report = {}
    # Plantard multiplication: exhaustive over 17-bit inputs for a few constants
    for c in [1, F_INTT, Z[1], Z[77], Z[127] * F_INTT % Q, 1 << 16]:
        w = plant_const(c)
        for b in range(-(1 << 17), 1 << 17, 7):
            r = plant_model(b, w)
            assert (r - b * c) % Q == 0 and abs(r) <= Q // 2 + 1, (c, b, r)

    progs = {"ntt": gen_ntt(), "intt": gen_intt(), "tomont": gen_tomont(), "reduce": gen_reduce(),
             "mulcache": gen_mulcache()}
    for k in (2, 3, 4):
        progs["basemul%d" % k] = gen_basemul(k)
    progs["rej_uniform"] = gen_rej_uniform()
    progs["tobytes"] = gen_tobytes()
    progs["frombytes"] = gen_frombytes()

    for trial in range(20):
        a = [random.randint(0, Q - 1) for _ in range(N)]
        if trial == 0:
            a = [Q - 1] * N
        sim = Sim(progs["tobytes"], SYM)
        put_poly(sim, POLY, a)
        sim.run("pqc_mlkem_tobytes", dict(BASE_REGS, a0=0x50000, a1=POLY))
        packed = bytes(sim.read(0x50000 + i, 1) for i in range(384))
        assert packed == tobytes_ref(a), "tobytes"
        report["tobytes"] = (sim.insns, sim.cycles, 0)
        raw = bytes(random.getrandbits(8) for _ in range(384)) if trial else packed
        sim = Sim(progs["frombytes"], SYM)
        for i, v in enumerate(raw):
            sim.write(0x50000 + i, v, 1)
        sim.run("pqc_mlkem_frombytes", dict(BASE_REGS, a0=POLY, a1=0x50000))
        got = get_poly(sim, POLY)
        want = []
        for i in range(N // 2):
            b0, b1, b2 = raw[3 * i], raw[3 * i + 1], raw[3 * i + 2]
            want += [b0 | ((b1 & 0x0F) << 8), (b1 >> 4) | (b2 << 4)]
        assert got == want, "frombytes"
        report["frombytes"] = (sim.insns, sim.cycles, max(got))

    cases = [(bytes(random.getrandbits(8) for _ in range(504)), 256) for _ in range(40)]
    cases += [(bytes(504), 256), (bytes([0xFF]) * 504, 256), (bytes(504), 8), (bytes(504), 9),
              (bytes(random.getrandbits(8) for _ in range(168)), 256),
              (bytes(random.getrandbits(8) for _ in range(504)), 100),
              (bytes(random.getrandbits(8) for _ in range(12)), 256),
              (bytes([0xFF, 0x0F, 0x00] * 168), 256), (bytes([0x00, 0xF0, 0xFF] * 168), 256)]
    for buf, length in cases:
        sim = Sim(progs["rej_uniform"], SYM)
        for i, v in enumerate(buf):
            sim.write(0x50000 + i, v, 1)
        for i in range(length + 8):
            sim.write(POLY + 2 * i, 0x7777, 2)
        sim.run("pqc_mlkem_rej_uniform", dict(BASE_REGS, a0=POLY, a1=length, a2=0x50000, a3=len(buf)))
        want = rej_ref(buf, length)
        got_n = sim.reg["a0"]
        assert got_n == len(want), ("rej count", got_n, len(want))
        assert [sim.read(POLY + 2 * i, 2) for i in range(got_n)] == want, "rej values"
        assert all(sim.read(POLY + 2 * i, 2) == 0x7777 for i in range(length, length + 8)), "rej overrun"
        if len(buf) == 504 and length == 256 and buf not in (bytes(504), bytes([0xFF]) * 504):
            report["rej_uniform"] = (sim.insns, sim.cycles, got_n)

    def run(name, entry, regs):
        sim = Sim(progs[name], SYM)
        load_tables(sim)
        return sim, entry, regs

    for trial in range(30):
        bound = Q - 1 if trial % 3 else 32767
        x = [random.randint(-(Q - 1), Q - 1) for _ in range(N)]
        sim = Sim(progs["ntt"], SYM)
        load_tables(sim)
        put_poly(sim, POLY, x)
        sim.run("pqc_mlkem_ntt", dict(BASE_REGS, a0=POLY))
        y = get_poly(sim, POLY)
        assert [v % Q for v in y] == ntt_ref(x), "ntt"
        assert max(abs(v) for v in y) < 8 * Q
        report["ntt"] = (sim.insns, sim.cycles, max(abs(v) for v in y))

        x = [random.randint(-bound, bound) for _ in range(N)]
        if trial == 0:
            x = [32767] * N
        if trial == 1:
            x = [-32768] * N
        sim = Sim(progs["intt"], SYM)
        load_tables(sim)
        put_poly(sim, POLY, x)
        sim.run("pqc_mlkem_intt_tomont", dict(BASE_REGS, a0=POLY))
        y = get_poly(sim, POLY)
        assert [v % Q for v in y] == intt_ref(x), "intt"
        assert max(abs(v) for v in y) < 8 * Q
        report["intt"] = (sim.insns, sim.cycles, max(abs(v) for v in y))

        x = [random.randint(-32768, 32767) for _ in range(N)]
        sim = Sim(progs["tomont"], SYM)
        put_poly(sim, POLY, x)
        sim.run("pqc_mlkem_tomont", dict(BASE_REGS, a0=POLY))
        y = get_poly(sim, POLY)
        assert all((v - c * 65536) % Q == 0 and abs(v) < Q for v, c in zip(y, x)), "tomont"
        report["tomont"] = (sim.insns, sim.cycles, max(abs(v) for v in y))

        sim = Sim(progs["reduce"], SYM)
        put_poly(sim, POLY, x)
        sim.run("pqc_mlkem_reduce", dict(BASE_REGS, a0=POLY))
        y = get_poly(sim, POLY)
        assert y == [c % Q for c in x], "reduce"
        report["reduce"] = (sim.insns, sim.cycles, max(y))

        sim = Sim(progs["mulcache"], SYM)
        load_tables(sim)
        put_poly(sim, POLY, x)
        sim.run("pqc_mlkem_mulcache", dict(BASE_REGS, a0=POLY + 0x1000, a1=POLY))
        y = get_poly(sim, POLY + 0x1000, N // 2)
        for i in range(64):
            assert (y[2 * i] - x[4 * i + 1] * Z[64 + i]) % Q == 0 and abs(y[2 * i]) < Q
            assert (y[2 * i + 1] + x[4 * i + 3] * Z[64 + i]) % Q == 0 and abs(y[2 * i + 1]) < Q
        report["mulcache"] = (sim.insns, sim.cycles, max(abs(v) for v in y))

        for k in (2, 3, 4):
            a = [[random.randint(0, 4095) for _ in range(N)] for _ in range(k)]
            b = [[random.randint(-32768, 32767) for _ in range(N)] for _ in range(k)]
            c = [[random.randint(-(Q - 1), Q - 1) for _ in range(N // 2)] for _ in range(k)]
            if trial == 0:
                a = [[4095] * N for _ in range(k)]
                b = [[-32768] * N for _ in range(k)]
                c = [[-(Q - 1)] * (N // 2) for _ in range(k)]
            sim = Sim(progs["basemul%d" % k], SYM)
            for v in range(k):
                put_poly(sim, 0x20000 + 512 * v, a[v])
                put_poly(sim, 0x30000 + 512 * v, b[v])
                for i, cv in enumerate(c[v]):
                    sim.write(0x38000 + 256 * v + 2 * i, cv & 0xFFFF, 2)
            sim.run("pqc_mlkem_basemul_acc_k%d" % k,
                    dict(BASE_REGS, a0=POLY, a1=0x20000, a2=0x30000, a3=0x38000))
            y = get_poly(sim, POLY)
            assert y == basemul_ref(a, b, c, k), "basemul"
            report["basemul_k%d" % k] = (sim.insns, sim.cycles, max(abs(v) for v in y))
    return progs, report


def main():
    progs, report = check()
    if "--check" in sys.argv:
        for name, (insns, cycles, bound) in report.items():
            print("%-12s %6d instructions, %6d cycles predicted, largest output %d" % (name, insns, cycles, bound))
        return
    out = ["/* ML-KEM polynomial arithmetic for the ESP32-C3 (RV32IMC).",
           " * GENERATED by gen/mlkem_arith_gen.py; do not edit by hand.",
           " *",
           " * Plantard arithmetic with `mul` and `srai` only (on this core `mulh` costs",
           " * five cycles and `mul` one), layers merged 4+3, no use of gp or tp.",
           " * Predicted cost on the ESP32-C3, from the interpreter in gen/rvgen.py:"]
    for name, (insns, cycles, _) in report.items():
        out.append(" *   %-12s %6d instructions, %6d cycles" % (name, insns, cycles))
    out += [" */", "    .text"]
    for name in ["ntt", "intt", "basemul2", "basemul3", "basemul4", "tomont", "reduce", "mulcache",
                 "rej_uniform", "tobytes", "frombytes"]:
        prog = progs[name]
        fname = prog[0][1]
        out += ["", "    .globl %s" % fname, "    .type %s, %%function" % fname, "    .balign 4", emit(prog),
                "    .size %s, .-%s" % (fname, fname)]
    out += ["", "    .section .rodata.pqc_mlkem_tables, \"a\"", "    .balign 4"]
    for name, tab in (("pqc_mlkem_ntt_w", ntt_tables()), ("pqc_mlkem_intt_w", intt_tables()),
                      ("pqc_mlkem_mulcache_w", mulcache_table())):
        out.append("%s:" % name)
        for i in range(0, len(tab), 4):
            out.append("    .word " + ", ".join("0x%08x" % w for w in tab[i:i + 4]))
    print("\n".join(out))


if __name__ == "__main__":
    main()
