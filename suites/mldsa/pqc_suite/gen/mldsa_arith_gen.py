#!/usr/bin/env python3
"""Generator of mldsa_c3_rv32imc.S: the ESP32-C3 routines of the optimized
ML-DSA build: forward and inverse NTT, and the rejection sampler for the
public matrix. (Pointwise multiplication stays with mldsa-native's own
RV32IM assembly.)

NTT and inverse NTT follow the same arithmetic as mldsa-native's RV32IM
backend: Barrett multiplication by a constant with a precomputed quotient,
which needs one `mulh` (5 cycles on the ESP32-C3) instead of the two of a
Montgomery multiplication. What is different here:

* four layers are merged instead of two, with 16 coefficients in registers,
  so every coefficient is loaded and stored twice per transform instead of
  four times;
* the inverse NTT has no separate scaling pass: the factor 2^32/256 is
  applied in the last layer, where every output is one multiplication anyway;
* gp and tp are not used, and no instruction uses a register loaded by the
  instruction before it.

    python3 mldsa_arith_gen.py > ../mldsa_c3_rv32imc.S    generate
    python3 mldsa_arith_gen.py --check                    verify and count

The code is executed in the interpreter of suites/mlkem/pqc_suite/gen/rvgen.py
and compared with a Python model of the C routine before it is printed.
"""
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "mlkem",
                                "pqc_suite", "gen"))
from rvgen import Sim, emit, function, regs_of, s32, schedule  # noqa: E402

Q = 8380417


def gen_rej_uniform():
    """unsigned f(int32_t *r, unsigned len, const uint8_t *buf, unsigned buflen)
    Appends to r the 23-bit values (three bytes each, little endian, top bit
    dropped) of buf that are below q, in order, until len values are stored
    or buf is used up; returns the count. buf must be word aligned and buflen
    a multiple of 12 (the C wrapper falls back to the portable code
    otherwise). The input is public (it expands the public matrix A).

    Main loop: 12 bytes = 4 candidates, each stored unconditionally; the
    output pointer advances only if the value was below q, so there is no
    branch per candidate. It runs while 4 more values fit."""
    R, RE, B, BE, R0, LIM, QQ = "a0", "a1", "a2", "a3", "a4", "a5", "a6"
    prog = [("mv", R0, R), ("slli", RE, RE, 2), ("add", RE, R, RE), ("add", BE, B, BE),
            ("addi", LIM, RE, -16), ("li", QQ, Q),
            ("beq", B, BE, ".Lmldrej_tail"), ("bltu", LIM, R, ".Lmldrej_tail"), ("label", ".Lmldrej_fast")]
    w0, w1, w2 = "t0", "t1", "t2"
    ins = [("lw", w0, B, 0), ("lw", w1, B, 4), ("lw", w2, B, 8)]

    def append(v, flag):
        ins.extend([("sw", v, R, 0), ("sltu", flag, v, QQ), ("slli", flag, flag, 2), ("add", R, R, flag)])

    ins.extend([("slli", "t3", w0, 9), ("srli", "t3", "t3", 9)])
    append("t3", "t6")
    ins.extend([("srli", "t4", w0, 24), ("slli", "t5", w1, 8), ("or", "t4", "t4", "t5"),
                ("slli", "t4", "t4", 9), ("srli", "t4", "t4", 9)])
    append("t4", "t6")
    ins.extend([("srli", "t3", w1, 16), ("slli", "t5", w2, 16), ("or", "t3", "t3", "t5"),
                ("slli", "t3", "t3", 9), ("srli", "t3", "t3", 9)])
    append("t3", "t6")
    ins.extend([("slli", "t4", w2, 1), ("srli", "t4", "t4", 9)])
    append("t4", "t6")
    ins.append(("addi", B, B, 12))
    prog += schedule(ins)
    prog += [("bltu", LIM, R, ".Lmldrej_tail"), ("bne", B, BE, ".Lmldrej_fast"),
             ("label", ".Lmldrej_tail"),
             ("beq", R, RE, ".Lmldrej_done"), ("beq", B, BE, ".Lmldrej_done"),
             ("lbu", "t0", B, 0), ("lbu", "t1", B, 1), ("lbu", "t2", B, 2), ("addi", B, B, 3),
             ("slli", "t1", "t1", 8), ("slli", "t2", "t2", 16), ("or", "t0", "t0", "t1"), ("or", "t0", "t0", "t2"),
             ("slli", "t0", "t0", 9), ("srli", "t0", "t0", 9),
             ("bgeu", "t0", QQ, ".Lmldrej_tail"),
             ("sw", "t0", R, 0), ("addi", R, R, 4),
             ("j", ".Lmldrej_tail"),
             ("label", ".Lmldrej_done"),
             ("sub", "a0", R, R0), ("srli", "a0", "a0", 2)]
    return function("pqc_mldsa_rej_uniform", prog, set())


def rej_ref(buf, length):
    out, pos = [], 0
    while len(out) < length and pos + 3 <= len(buf):
        t = (buf[pos] | (buf[pos + 1] << 8) | (buf[pos + 2] << 16)) & 0x7FFFFF
        pos += 3
        if t < Q:
            out.append(t)
    return out


# ------------------------------------------------------------------ NTT

N = 256


def brv8(k):
    return int("{:08b}".format(k)[::-1], 2)


Z = [pow(1753, brv8(k), Q) for k in range(N)]
F_INTT = pow(2, 32, Q) * pow(256, -1, Q) % Q      # invntt_tomont: 2^32 / 256


def centered(x):
    x %= Q
    return x - Q if x > Q // 2 else x


def twist(c):
    """(c, round(c * 2^32 / q)) for the centered representative of c."""
    c = centered(c)
    return [c & 0xFFFFFFFF, ((c << 32) + Q // 2) // Q & 0xFFFFFFFF]


COEF = ["s%d" % i for i in range(12)] + ["a2", "a3", "a4", "a5"]
P, ZP, QR, END = "a0", "a1", "a6", "a7"
ZREG = [("t2", "t3"), ("t4", "t5")]
DIFF = "t6"


class Emit:
    def __init__(self):
        self.ins = []
        self.z = 0
        self.widx = 0

    def load(self):
        self.z += 1
        zr, zt = ZREG[self.z % len(ZREG)]
        self.ins += [("lw", zr, ZP, 4 * self.widx), ("lw", zt, ZP, 4 * self.widx + 4)]
        self.widx += 2
        return zr, zt

    def barrett(self, dst, x, zw, rounded=False):
        """dst = x * const mod q. Truncating: dst in (-q/4, 5q/4) for
        |x| < 2^31. Rounded: |dst| < 3q/4."""
        zr, zt = zw
        self.ins.append(("mulh", "t0", x, zt))
        if rounded:
            self.ins += [("mul", "t1", x, zt), ("srli", "t1", "t1", 31), ("add", "t0", "t0", "t1")]
        self.ins += [("mul", "t1", x, zr), ("mul", "t0", "t0", QR), ("sub", dst, "t1", "t0")]

    def ct(self, a, b, zw):
        self.barrett("t1", b, zw)
        self.ins += [("sub", b, a, "t1"), ("add", a, a, "t1")]

    def gs(self, a, b, zw):
        """a' = a + b, b' = (b - a) * const"""
        self.ins += [("sub", "t1", b, a), ("add", a, a, b)]
        zr, zt = zw
        self.ins += [("mulh", "t0", "t1", zt), ("mul", "t1", "t1", zr), ("mul", "t0", "t0", QR),
                     ("sub", b, "t1", "t0")]


def ntt_tables():
    tab = []
    for k in range(1, 16):
        tab += twist(Z[k])
    for b in range(16):
        for c in [Z[16 + b], Z[32 + 2 * b], Z[32 + 2 * b + 1]] + [Z[64 + 4 * b + g] for g in range(4)] + \
                 [Z[128 + 8 * b + g] for g in range(8)]:
            tab += twist(c)
    return tab


def gen_ntt():
    prog = [("li", QR, Q), ("la", ZP, "pqc_mldsa_ntt_w"), ("addi", END, P, 64), ("label", ".Lmldntt_1")]
    e = Emit()
    for i in range(16):
        e.ins.append(("lw", COEF[i], P, 64 * i))
    for layer in range(4):
        half = 8 >> layer
        for g in range(1 << layer):
            zw = e.load()
            for i in range(half):
                e.ct(COEF[2 * half * g + i], COEF[2 * half * g + half + i], zw)
    for i in range(16):
        e.ins.append(("sw", COEF[i], P, 64 * i))
    e.ins.append(("addi", P, P, 4))
    prog += schedule(e.ins, readonly_bases=[ZP])
    prog += [("bne", P, END, ".Lmldntt_1"),
             ("addi", P, P, -64), ("addi", ZP, ZP, 120), ("addi", END, P, 1024), ("label", ".Lmldntt_2")]
    e = Emit()
    for i in range(16):
        e.ins.append(("lw", COEF[i], P, 4 * i))
    for layer in range(4):
        half = 8 >> layer
        for g in range(1 << layer):
            zw = e.load()
            for i in range(half):
                e.ct(COEF[2 * half * g + i], COEF[2 * half * g + half + i], zw)
    for i in range(16):
        e.ins.append(("sw", COEF[i], P, 4 * i))
    e.ins += [("addi", P, P, 64), ("addi", ZP, ZP, 120)]
    prog += schedule(e.ins, readonly_bases=[ZP])
    prog += [("bne", P, END, ".Lmldntt_2")]
    return function("pqc_mldsa_ntt", prog, regs_of(prog))


def ntt_ref(a):
    a = list(a)
    k, ln = 0, 128
    while ln > 0:
        for start in range(0, N, 2 * ln):
            k += 1
            z = Z[k]
            for j in range(start, start + ln):
                t = z * a[j + ln] % Q
                a[j + ln] = (a[j] - t) % Q
                a[j] = (a[j] + t) % Q
        ln >>= 1
    return a


# ------------------------------------------------------------------ inverse NTT
# Reference: for len = 1, 2, ..., 128, blocks in ascending order, twiddle
# index k counting down from 255:  t = a[j]; a[j] = t + a[j+len];
# a[j+len] = (t - a[j+len]) * (-zeta_k) = (a[j+len] - t) * zeta_k.
# Then everything is multiplied by 2^32 / 256.

def intt_tables():
    tab = []
    for b in range(16):
        cs = [Z[255 - (8 * b + g)] for g in range(8)]       # len 1
        cs += [Z[127 - (4 * b + g)] for g in range(4)]      # len 2
        cs += [Z[63 - (2 * b + g)] for g in range(2)]       # len 4
        cs += [Z[31 - b]]                                   # len 8
        for c in cs:
            tab += twist(c)
    cs = [Z[15 - g] for g in range(8)] + [Z[7 - g] for g in range(4)] + [Z[3 - g] for g in range(2)]
    cs += [F_INTT, Z[1] * F_INTT]
    for c in cs:
        tab += twist(c)
    return tab


def gen_intt():
    prog = [("li", QR, Q), ("la", ZP, "pqc_mldsa_intt_w"), ("addi", END, P, 1024), ("label", ".Lmldintt_1")]
    e = Emit()
    for i in range(16):
        e.ins.append(("lw", COEF[i], P, 4 * i))
    for layer in range(4):                   # len 1, 2, 4, 8
        half = 1 << layer
        for g in range(8 >> layer):
            zw = e.load()
            for i in range(half):
                e.gs(COEF[2 * half * g + i], COEF[2 * half * g + half + i], zw)
    for i in range(16):
        e.ins.append(("sw", COEF[i], P, 4 * i))
    e.ins += [("addi", P, P, 64), ("addi", ZP, ZP, 120)]
    prog += schedule(e.ins, readonly_bases=[ZP])
    prog += [("bne", P, END, ".Lmldintt_1"),
             ("addi", P, P, -1024), ("addi", END, P, 64), ("label", ".Lmldintt_2")]
    e = Emit()
    for i in range(16):
        e.ins.append(("lw", COEF[i], P, 64 * i))
    for layer in range(3):                   # len 16, 32, 64
        half = 1 << layer
        for g in range(8 >> layer):
            zw = e.load()
            for i in range(half):
                e.gs(COEF[2 * half * g + i], COEF[2 * half * g + half + i], zw)
    fw = e.load()                            # 2^32/256
    zf = e.load()                            # zeta * 2^32/256
    for i in range(8):                       # len 128, with the final scaling
        a, b = COEF[i], COEF[8 + i]
        e.ins += [("sub", DIFF, b, a), ("add", a, a, b)]
        e.barrett(a, a, fw, rounded=True)
        e.barrett(b, DIFF, zf, rounded=True)
    for i in range(16):
        e.ins.append(("sw", COEF[i], P, 64 * i))
    e.ins.append(("addi", P, P, 4))
    prog += schedule(e.ins, readonly_bases=[ZP])
    prog += [("bne", P, END, ".Lmldintt_2")]
    return function("pqc_mldsa_intt_tomont", prog, regs_of(prog))


def intt_ref(a):
    a = list(a)
    k, ln = N, 1
    while ln < N:
        for start in range(0, N, 2 * ln):
            k -= 1
            z = -Z[k]
            for j in range(start, start + ln):
                t = a[j]
                a[j] = (t + a[j + ln]) % Q
                a[j + ln] = (t - a[j + ln]) * z % Q
        ln <<= 1
    return [x * F_INTT % Q for x in a]



# ------------------------------------------------------------------ pointwise multiplication

QINV32 = pow(Q, -1, 1 << 32)


def gen_pointwise():
    """void f(int32_t a[256], const int32_t b[256]): a[i] = a[i]*b[i]*2^-32
    mod q, |result| < q, for |a[i]|, |b[i]| < 9*ceil(5q/4). The same
    Montgomery multiplication as mldsa-native's RV32IM routine, but four
    coefficients per loop iteration and with the loads scheduled, which saves
    the taken branch and the load-use stalls of the one-coefficient loop."""
    A, B, EN, QQ, QI = "a0", "a1", "a2", "a3", "a4"
    prog = [("li", QQ, Q), ("li", QI, QINV32), ("addi", EN, A, 1024), ("label", ".Lmldpw")]
    ins = []
    regs = [("t0", "t1", "t2"), ("t3", "t4", "t5"), ("t6", "a5", "a6"), ("a7", "s0", "s1")]
    for i, (x, y, t) in enumerate(regs):
        ins += [("lw", x, A, 4 * i), ("lw", y, B, 4 * i),
                ("mul", t, x, y), ("mulh", x, x, y), ("mul", t, t, QI), ("mulh", t, t, QQ),
                ("sub", x, x, t), ("sw", x, A, 4 * i)]
    ins += [("addi", A, A, 16), ("addi", B, B, 16)]
    prog += schedule(ins, readonly_bases=[B]) + [("bne", A, EN, ".Lmldpw")]
    return function("pqc_mldsa_pointwise_montgomery", prog, regs_of(prog))


def gen_pointwise_acc(l):
    """void f(int32_t w[256], const int32_t u[l][256], const int32_t v[l][256]):
    w[i] = (sum over the l rows of u[j][i] * v[j][i]) * 2^-32 mod q, |w[i]| < q,
    for u in [0, q) and |v| < 9*ceil(5q/4). The products are accumulated as
    64-bit numbers and reduced once per coefficient, as in the C code; here
    the reduction uses `mulh` (the C compiler emits a 64-bit shift-and-add
    chain for it) and two coefficients are handled per loop iteration."""
    W, U0, V0, U1, V1, EN, QQ, QI = "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
    name = "pqc_mldsa_pointwise_acc_l%d" % l
    lab = ".Lmldacc%d" % l
    # rows 0..3 are reached from a pointer 2048 bytes into the array, rows 4..7
    # from a second one, so that every offset fits the 12-bit immediate
    prog = [("li", QQ, Q), ("li", QI, QINV32), ("addi", EN, W, 1024),
            ("addi", U0, U0, 2047), ("addi", U0, U0, 1), ("addi", V0, V0, 2047), ("addi", V0, V0, 1)]
    if l > 4:
        prog += [("addi", U1, U0, 2047), ("addi", U1, U1, 2047), ("addi", U1, U1, 2),
                 ("addi", V1, V0, 2047), ("addi", V1, V1, 2047), ("addi", V1, V1, 2)]
    prog.append(("label", lab))
    ins = []
    sets = [("t0", "t1", "t2", "t3", "t4"), ("t5", "t6", "s0", "s1", "s2")]   # lo, hi, x, y, t
    for c in range(2):
        lo, hi, x, y, t = sets[c]
        for j in range(l):
            ub, vb = (U0, V0) if j < 4 else (U1, V1)
            off = 1024 * (j % 4) - 2048 + 4 * c
            ins += [("lw", x, ub, off), ("lw", y, vb, off)]
            if j == 0:
                ins += [("mul", lo, x, y), ("mulh", hi, x, y)]
            else:
                ins += [("mul", t, x, y), ("mulh", x, x, y), ("add", lo, lo, t), ("sltu", t, lo, t),
                        ("add", hi, hi, x), ("add", hi, hi, t)]
        ins += [("mul", t, lo, QI), ("mulh", t, t, QQ), ("sub", hi, hi, t), ("sw", hi, W, 4 * c)]
    ins += [("addi", W, W, 8), ("addi", U0, U0, 8), ("addi", V0, V0, 8)]
    if l > 4:
        ins += [("addi", U1, U1, 8), ("addi", V1, V1, 8)]
    prog += schedule(ins, readonly_bases=[U0, V0, U1, V1]) + [("bne", W, EN, lab)]
    return function(name, prog, regs_of(prog))


def gen_chknorm():
    """int f(const int32_t a[256], int32_t B): 0 if every |a[i]| < B, else 1.
    Constant time: all 256 coefficients are always read and no branch
    depends on them. 6 instructions per coefficient, 8 coefficients per loop
    iteration."""
    A, B1, EN, ACC = "a0", "a1", "a2", "a3"
    prog = [("addi", B1, B1, -1), ("li", ACC, 0), ("addi", EN, A, 1024), ("label", ".Lmldchk")]
    ins = []
    xs = ["t0", "t1", "t2", "t3", "t4", "t5", "t6", "a4"]
    ts = ["a5", "a6", "a7", "a5", "a6", "a7", "a5", "a6"]
    for i in range(8):
        x, t = xs[i], ts[i]
        ins += [("lw", x, A, 4 * i), ("srai", t, x, 31), ("xor", x, x, t), ("sub", x, x, t),
                ("sub", x, B1, x), ("or", ACC, ACC, x)]
    ins.append(("addi", A, A, 32))
    prog += schedule(ins) + [("bne", A, EN, ".Lmldchk"), ("srli", "a0", ACC, 31)]
    return function("pqc_mldsa_chknorm", prog, set())


def mont_mul_ref(a, b):
    p = a * b
    t = s32(p * QINV32)
    return (p - t * Q) >> 32



BASE_REGS = {"sp": 0x80000, "ra": 0}
for _r in ["s%d" % i for i in range(12)] + ["t%d" % i for i in range(7)] + ["a%d" % i for i in range(8)]:
    BASE_REGS.setdefault(_r, 0xDEAD0000)
POLY, BUF = 0x10000, 0x50000


def check():
    random.seed(4)
    report = {}
    prog = gen_rej_uniform()
    cases = [(bytes(random.getrandbits(8) for _ in range(840)), 256) for _ in range(40)]
    cases += [(bytes(840), 256), (bytes([0xFF]) * 840, 256), (bytes(840), 4), (bytes(840), 5), (bytes(840), 7),
              (bytes(random.getrandbits(8) for _ in range(168)), 256),
              (bytes(random.getrandbits(8) for _ in range(840)), 100),
              (bytes(random.getrandbits(8) for _ in range(12)), 256),
              (bytes([0x00, 0xE0, 0x7F] * 280), 256), (bytes([0x01, 0xE0, 0x7F] * 280), 256),
              (bytes([0x01, 0xE0, 0xFF] * 280), 256)]
    for buf, length in cases:
        sim = Sim(prog, {})
        for i, v in enumerate(buf):
            sim.write(BUF + i, v, 1)
        for i in range(length + 4):
            sim.write(POLY + 4 * i, 0x77777777, 4)
        sim.run("pqc_mldsa_rej_uniform", dict(BASE_REGS, a0=POLY, a1=length, a2=BUF, a3=len(buf)))
        want = rej_ref(buf, length)
        n = sim.reg["a0"]
        assert n == len(want), ("count", n, len(want))
        assert [sim.read(POLY + 4 * i, 4) for i in range(n)] == want, "values"
        assert all(sim.read(POLY + 4 * i, 4) == 0x77777777 for i in range(length, length + 4)), "overrun"
        if len(buf) == 840 and length == 256 and len(set(buf)) > 8:
            report["rej_uniform"] = (sim.insns, sim.cycles, n)
    progs = {"ntt": gen_ntt(), "intt": gen_intt(), "pointwise": gen_pointwise(), "rej_uniform": prog}
    progs["chknorm"] = gen_chknorm()
    rmax = 6283009                      # MLD_REDUCE32_RANGE_MAX
    for trial in range(60):
        bnd = random.choice([1 << 17, (1 << 19) - 196, 261888 - 120, 95232 - 78, 1, 0, Q - rmax])
        if bnd == 0:
            a = [0] * N
        elif trial % 2:
            a = [random.randint(-bnd - 2, bnd + 2) for _ in range(N)]
        else:
            a = [random.randint(-(bnd - 1), bnd - 1) for _ in range(N)]
        if trial % 5 == 0 and bnd > 0:
            a = [0] * N
            a[random.randrange(N)] = random.choice([bnd, -bnd, bnd - 1, -(bnd - 1), rmax - 1, -rmax])
        sim = Sim(progs["chknorm"], {})
        for i in range(N):
            sim.write(POLY + 4 * i, a[i] & 0xFFFFFFFF, 4)
        sim.run("pqc_mldsa_chknorm", dict(BASE_REGS, a0=POLY, a1=bnd))
        want = 0 if all(abs(v) < bnd for v in a) else 1
        assert sim.reg["a0"] == want, ("chknorm", bnd, want)
        report["chknorm"] = (sim.insns, sim.cycles, want)
    bound = 9 * ((5 * Q + 3) // 4) - 1
    for trial in range(20):
        a = [random.randint(-bound, bound) for _ in range(N)]
        b = [random.randint(-bound, bound) for _ in range(N)]
        if trial == 0:
            a, b = [bound] * N, [-bound] * N
        if trial == 1:
            a, b = [bound] * N, [bound] * N
        sim = Sim(progs["pointwise"], {})
        for i in range(N):
            sim.write(POLY + 4 * i, a[i] & 0xFFFFFFFF, 4)
            sim.write(POLY + 0x1000 + 4 * i, b[i] & 0xFFFFFFFF, 4)
        sim.run("pqc_mldsa_pointwise_montgomery", dict(BASE_REGS, a0=POLY, a1=POLY + 0x1000))
        y = [sim.read(POLY + 4 * i, 4, True) for i in range(N)]
        assert y == [mont_mul_ref(u, v) for u, v in zip(a, b)], "pointwise"
        assert max(abs(v) for v in y) < Q
        assert [sim.read(POLY + 0x1000 + 4 * i, 4, True) for i in range(N)] == b
        report["pointwise"] = (sim.insns, sim.cycles, max(abs(v) for v in y))
        for l in (4, 5, 7):
            name = "pointwise_acc_l%d" % l
            if name not in progs:
                progs[name] = gen_pointwise_acc(l)
            u = [[random.randint(0, Q - 1) for _ in range(N)] for _ in range(l)]
            v = [[random.randint(-bound, bound) for _ in range(N)] for _ in range(l)]
            if trial == 0:
                u = [[Q - 1] * N for _ in range(l)]
                v = [[bound] * N for _ in range(l)]
            if trial == 1:
                u = [[Q - 1] * N for _ in range(l)]
                v = [[-bound] * N for _ in range(l)]
            sim = Sim(progs[name], {})
            UB, VB = 0x100000, 0x200000
            for j in range(l):
                for i in range(N):
                    sim.write(UB + 1024 * j + 4 * i, u[j][i], 4)
                    sim.write(VB + 1024 * j + 4 * i, v[j][i] & 0xFFFFFFFF, 4)
            sim.run("pqc_mldsa_pointwise_acc_l%d" % l, dict(BASE_REGS, a0=POLY, a1=UB, a2=VB))
            y = [sim.read(POLY + 4 * i, 4, True) for i in range(N)]
            want = []
            for i in range(N):
                acc = sum(u[j][i] * v[j][i] for j in range(l))
                t = s32(acc * QINV32)
                want.append((acc - t * Q) >> 32)
            assert y == want, name
            assert max(abs(x) for x in y) < Q, name
            report[name] = (sim.insns, sim.cycles, max(abs(x) for x in y))
    sym = {"pqc_mldsa_ntt_w": 0x40000, "pqc_mldsa_intt_w": 0x44000}
    for trial in range(40):
        x = [random.randint(-(Q - 1), Q - 1) for _ in range(N)]
        if trial == 0:
            x = [Q - 1] * N
        if trial == 1:
            x = [-(Q - 1)] * N
        if trial == 2:
            x = [(Q - 1) * (1 if (i * 7 + i // 16) % 3 else -1) for i in range(N)]
        for name, entry, tab, tsym, ref, bound in (
                ("ntt", "pqc_mldsa_ntt", ntt_tables(), "pqc_mldsa_ntt_w", ntt_ref, 9 * ((5 * Q + 3) // 4)),
                ("intt", "pqc_mldsa_intt_tomont", intt_tables(), "pqc_mldsa_intt_w", intt_ref, Q)):
            sim = Sim(progs[name], sym)
            for i, w in enumerate(tab):
                sim.write(sym[tsym] + 4 * i, w, 4)
            for i, v in enumerate(x):
                sim.write(POLY + 4 * i, v & 0xFFFFFFFF, 4)
            sim.run(entry, dict(BASE_REGS, a0=POLY))
            y = [sim.read(POLY + 4 * i, 4, True) for i in range(N)]
            assert [v % Q for v in y] == ref(x), name
            assert max(abs(v) for v in y) < bound, (name, max(abs(v) for v in y) / Q)
            if trial > 2:
                report[name] = (sim.insns, sim.cycles, max(abs(v) for v in y))
    return progs, report


def main():
    progs, report = check()
    if "--check" in sys.argv:
        for name, (insns, cycles, n) in report.items():
            print("%-12s %6d instructions, %6d cycles predicted (largest output or count: %d)"
                  % (name, insns, cycles, n))
        return
    out = ["/* ESP32-C3 additions to mldsa-native's RV32IM backend.",
           " * GENERATED by gen/mldsa_arith_gen.py; do not edit by hand.",
           " * Predicted cost on the ESP32-C3, from the interpreter in rvgen.py:"]
    for name, (insns, cycles, _) in report.items():
        out.append(" *   %-12s %6d instructions, %6d cycles" % (name, insns, cycles))
    out += [" */", "    .text"]
    for name, prog in progs.items():
        fname = prog[0][1]
        out += ["", "    .globl %s" % fname, "    .type %s, %%function" % fname, "    .balign 4", emit(prog),
                "    .size %s, .-%s" % (fname, fname)]
    out += ["", "    .section .rodata.pqc_mldsa_tables, \"a\"", "    .balign 4"]
    for name, tab in (("pqc_mldsa_ntt_w", ntt_tables()), ("pqc_mldsa_intt_w", intt_tables())):
        out.append("%s:" % name)
        for i in range(0, len(tab), 4):
            out.append("    .word " + ", ".join("0x%08x" % w for w in tab[i:i + 4]))
    print("\n".join(out))


if __name__ == "__main__":
    main()
