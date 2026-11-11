#!/usr/bin/env python3
"""Generate sha512_rv32imc.S: the SHA-512 compression function for a 32-bit
RISC-V CPU without a rotate instruction (ESP32-C3, RV32IMC).

    python3 sha512_gen.py            write sha512_rv32imc.S next to this file
    python3 sha512_gen.py --test     run the generated instructions in a small
                                     RV32 interpreter against Python's SHA-512

Why assembly. A 64-bit value is two registers here, and SHA-512 has eight
working variables: 16 registers, plus the cached a^b of the Maj trick, plus
the message word being built. A compiler runs out of the 28 registers it may
use and spills about ten values per round; on this CPU a load that is used by
the next instruction also stalls. This code keeps all of it in registers, and
places every load at least two instructions before its first use:

    working variables   s0-s11, a4-a7     (rotate through the rounds)
    a ^ b of last round  two registers that swap with two temporaries
    temporaries          t0-t4 (five at most are live)
    message word         t5 (high), t6 (low)
    round constants      ra (pointer into the table)
    arguments            a0 (chaining value), a1 (block)

Per round: Sigma1 and Sigma0 are 6 shifts and 5 XORs per half; Ch is 3
instructions per half; Maj is 3 per half, using Maj(a,b,c) = ((a^b) & (b^c))
^ b where b^c is the a^b of the previous round; each 64-bit addition is add,
sltu, add, add. That is 86 instructions. A message word for round 16 and
later is 62 more. One block is about 11,000 instructions.

Interface (see the top of the generated file):

    void pqc_sha512_compress_rv32(uint32_t state[16], const uint32_t block[32]);

state is the chaining value and block the 16 message words, both as (high,
low) pairs of native 32-bit words. For a block given as bytes, word j is the
big-endian value of bytes 4j..4j+3. Leaving that conversion to the caller
lets a caller that builds its block from words skip it for the parts that
are constants.
"""
import hashlib
import os
import struct
import sys

K = [
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
    0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
    0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
    0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
    0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
    0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
    0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
    0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
    0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
    0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
    0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
    0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
    0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
    0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
    0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
    0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817,
]

STATE = ["s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
         "a4", "a5", "a6", "a7"]          # (high, low) of a, b, c, d, e, f, g, h
SAVED = ["ra"] + ["s%d" % i for i in range(12)]
WH, WL = "t5", "t6"
KP = "ra"
ST, BLK = "a0", "a1"
RING = 0                                   # sp + 8*i: W[i] high, +4: low
SAVE = 128                                 # saved registers above the ring
END = SAVE + 4 * len(SAVED)                # end of the constant table
FRAME = (END + 4 + 15) & ~15


class Gen:
    def __init__(self):
        self.code = []                     # (op, operands...) or ("label", name)
        self.x = ["a2", "a3"]              # (high, low) of a ^ b from the last round
        self.tmp = ["t0", "t1", "t2", "t3", "t4"]

    def e(self, op, *args):
        self.code.append((op,) + args)

    # ----- pieces -----
    def sigma(self, dst, scratch, terms):
        """dst = XOR of the terms; a term is (register, 'r' or 'l', amount)."""
        first = True
        for reg, kind, amt in terms:
            op = "srli" if kind == "r" else "slli"
            if first:
                self.e(op, dst, reg, amt)
                first = False
            else:
                self.e(op, scratch, reg, amt)
                self.e("xor", dst, dst, scratch)

    def add64(self, hi, lo, shi, slo, carry):
        """(hi, lo) += (shi, slo)"""
        self.e("add", lo, lo, slo)
        self.e("sltu", carry, lo, slo)
        self.e("add", hi, hi, shi)
        self.e("add", hi, hi, carry)

    def round(self, r, koff):
        """One round with the working variables as they are after r rounds."""
        v = [STATE[(2 * ((i - r) % 8)) + j] for i in range(8) for j in range(2)]
        ah, al, bh, bl, ch, cl, dh, dl, eh, el, fh, fl, gh, gl, hh, hl = v
        t0, t1, t2, t3, t4 = self.tmp
        xh, xl = self.x
        # round constant, loaded early
        self.e("lw", t3, koff + 4, KP)
        self.e("lw", t4, koff, KP)
        # h += Sigma1(e)
        self.sigma(t0, t1, [(el, "r", 14), (eh, "l", 18), (el, "r", 18), (eh, "l", 14),
                            (eh, "r", 9), (el, "l", 23)])
        self.e("add", hl, hl, t0)
        self.e("sltu", t2, hl, t0)
        self.sigma(t0, t1, [(eh, "r", 14), (el, "l", 18), (eh, "r", 18), (el, "l", 14),
                            (el, "r", 9), (eh, "l", 23)])
        self.e("add", hh, hh, t0)
        self.e("add", hh, hh, t2)
        # h += Ch(e, f, g)
        self.e("xor", t0, fl, gl)
        self.e("and", t0, t0, el)
        self.e("xor", t0, t0, gl)
        self.e("xor", t1, fh, gh)
        self.e("and", t1, t1, eh)
        self.e("xor", t1, t1, gh)
        self.add64(hh, hl, t1, t0, t2)
        # h += K + W
        self.add64(hh, hl, t4, t3, t2)
        self.add64(hh, hl, WH, WL, t2)
        # d += h
        self.add64(dh, dl, hh, hl, t2)
        # h += Sigma0(a)
        self.sigma(t0, t1, [(al, "r", 28), (ah, "l", 4), (ah, "r", 2), (al, "l", 30),
                            (ah, "r", 7), (al, "l", 25)])
        self.e("add", hl, hl, t0)
        self.e("sltu", t2, hl, t0)
        self.sigma(t0, t1, [(ah, "r", 28), (al, "l", 4), (al, "r", 2), (ah, "l", 30),
                            (al, "r", 7), (ah, "l", 25)])
        self.e("add", hh, hh, t0)
        self.e("add", hh, hh, t2)
        # h += Maj(a, b, c) = ((a ^ b) & (b ^ c)) ^ b; x is b ^ c, t3:t4 become a ^ b
        self.e("xor", t4, al, bl)
        self.e("and", xl, xl, t4)
        self.e("xor", xl, xl, bl)
        self.e("xor", t3, ah, bh)
        self.e("and", xh, xh, t3)
        self.e("xor", xh, xh, bh)
        self.add64(hh, hl, xh, xl, t2)
        # the old x registers are free now; t3:t4 hold the new x
        self.x = [t3, t4]
        self.tmp = [t0, t1, t2, xh, xl]

    def load_w(self, i):
        """W[i] from the block into WH:WL and the ring."""
        self.e("lw", WH, 8 * i, BLK)
        self.e("lw", WL, 8 * i + 4, BLK)
        self.e("sw", WH, RING + 8 * i, "sp")
        self.e("sw", WL, RING + 8 * i + 4, "sp")

    def sched(self, i):
        """W[t] = sigma1(W[t-2]) + W[t-7] + sigma0(W[t-15]) + W[t-16], ring slot i."""
        t0, t1, t2, t3, t4 = self.tmp
        x, y, z = (i + 1) & 15, (i + 14) & 15, (i + 9) & 15
        self.e("lw", t0, RING + 8 * x + 4, "sp")      # xl
        self.e("lw", t1, RING + 8 * x, "sp")          # xh
        self.e("lw", WL, RING + 8 * i + 4, "sp")
        self.e("lw", WH, RING + 8 * i, "sp")
        self.sigma(t2, t3, [(t0, "r", 1), (t1, "l", 31), (t0, "r", 8), (t1, "l", 24),
                            (t0, "r", 7), (t1, "l", 25)])
        self.sigma(t3, t4, [(t1, "r", 1), (t0, "l", 31), (t1, "r", 8), (t0, "l", 24),
                            (t1, "r", 7)])
        self.e("lw", t0, RING + 8 * y + 4, "sp")      # yl
        self.e("lw", t1, RING + 8 * y, "sp")          # yh
        self.add64(WH, WL, t3, t2, t4)
        self.sigma(t2, t3, [(t0, "r", 19), (t1, "l", 13), (t1, "r", 29), (t0, "l", 3),
                            (t0, "r", 6), (t1, "l", 26)])
        self.sigma(t3, t4, [(t1, "r", 19), (t0, "l", 13), (t0, "r", 29), (t1, "l", 3),
                            (t1, "r", 6)])
        self.e("lw", t0, RING + 8 * z + 4, "sp")      # zl
        self.e("lw", t1, RING + 8 * z, "sp")          # zh
        self.add64(WH, WL, t3, t2, t4)
        self.add64(WH, WL, t1, t0, t4)
        self.e("sw", WL, RING + 8 * i + 4, "sp")
        self.e("sw", WH, RING + 8 * i, "sp")

    # ----- the function -----
    def build(self):
        self.e("addi", "sp", "sp", -FRAME)
        for n, r in enumerate(SAVED):
            self.e("sw", r, SAVE + 4 * n, "sp")
        for n, r in enumerate(STATE):
            self.e("lw", r, 4 * n, ST)
        self.e("la", KP, "pqc_sha512_k")
        self.e("addi", "t0", KP, 640)
        self.e("sw", "t0", END, "sp")
        bh, bl, ch, cl = STATE[2], STATE[3], STATE[4], STATE[5]
        self.e("xor", self.x[0], bh, ch)
        self.e("xor", self.x[1], bl, cl)
        for i in range(16):
            self.load_w(i)
            self.round(i, 8 * i)
        self.e("addi", KP, KP, 128)
        self.e("label", "1")
        for i in range(16):
            self.sched(i)
            self.round(i, 8 * i)
        self.e("addi", KP, KP, 128)
        self.e("lw", "t0", END, "sp")
        self.e("beq", KP, "t0", "2f")     # the loop body is too long for one branch
        self.e("j", "1b")
        self.e("label", "2")
        assert self.x == ["a2", "a3"]
        # chaining value += working variables
        t0, t1, t2 = "t0", "t1", "t2"
        for i in range(8):
            hi, lo = STATE[2 * i], STATE[2 * i + 1]
            self.e("lw", t0, 8 * i + 4, ST)
            self.e("lw", t1, 8 * i, ST)
            self.e("add", t0, t0, lo)
            self.e("sltu", t2, t0, lo)
            self.e("add", t1, t1, hi)
            self.e("add", t1, t1, t2)
            self.e("sw", t0, 8 * i + 4, ST)
            self.e("sw", t1, 8 * i, ST)
        for n, r in enumerate(SAVED):
            self.e("lw", r, SAVE + 4 * n, "sp")
        self.e("addi", "sp", "sp", FRAME)
        self.e("ret")
        return self.code


def render(code):
    out = []
    for ins in code:
        op = ins[0]
        if op == "label":
            out.append("%s:" % ins[1])
        elif op in ("lw", "sw", "lbu"):
            out.append("    %-5s %s, %d(%s)" % (op, ins[1], ins[2], ins[3]))
        elif op == "ret":
            out.append("    ret")
        else:
            out.append("    %-5s %s" % (op, ", ".join(str(a) for a in ins[1:])))
    return out


HEADER = """/* SHA-512 compression for RV32IMC (ESP32-C3). Generated by sha512_gen.py;
 * edit the generator, not this file. The generator explains the design and
 * can run these instructions against a reference (--test).
 *
 *   void pqc_sha512_compress_rv32(uint32_t state[16], const uint32_t block[32]);
 *
 * state: the chaining value as eight (high, low) pairs of native words.
 * block: the 16 message words in the same form (word j of a block of bytes
 * is the big-endian value of bytes 4j..4j+3). Uses %d bytes of stack.
 */
    .section .text.pqc_sha512_compress_rv32, "ax", @progbits
    .option rvc
    .globl pqc_sha512_compress_rv32
    .type pqc_sha512_compress_rv32, @function
    .balign 4
pqc_sha512_compress_rv32:
"""

FOOTER = """    .size pqc_sha512_compress_rv32, .-pqc_sha512_compress_rv32

    .section .rodata.pqc_sha512_k, "a", @progbits
    .balign 4
pqc_sha512_k:
"""


def write_file(code):
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sha512_rv32imc.S")
    with open(path, "w") as f:
        f.write(HEADER % FRAME)
        f.write("\n".join(render(code)) + "\n")
        f.write(FOOTER)
        for k in K:
            f.write("    .word 0x%08x, 0x%08x\n" % (k >> 32, k & 0xffffffff))
    n = sum(1 for c in code if c[0] != "label")
    print("wrote %s: %d instructions in the file" % (os.path.basename(path), n))


# ---------- a small interpreter for the instructions used above ----------
M32 = 0xffffffff


def run(code, state_words, block):
    mem = {}
    ST_A, BLK_A, K_A, SP0 = 0x1000, 0x2000, 0x3000, 0x9000

    def store(addr, val):
        for i in range(4):
            mem[addr + i] = (val >> (8 * i)) & 0xff

    def load(addr):
        return sum(mem.get(addr + i, 0) << (8 * i) for i in range(4))

    for i, w in enumerate(state_words):
        store(ST_A + 4 * i, w)
    for i, w in enumerate(struct.unpack(">32I", block)):
        store(BLK_A + 4 * i, w)
    for i, k in enumerate(K):
        store(K_A + 8 * i, k >> 32)
        store(K_A + 8 * i + 4, k & M32)
    reg = {"sp": SP0, "a0": ST_A, "a1": BLK_A}
    for r in SAVED:
        reg[r] = 0xdead0000 + len(reg)          # must come back unchanged
    saved_before = {r: reg[r] for r in SAVED}
    labels = {c[1]: i for i, c in enumerate(code) if c[0] == "label"}
    pc, steps, loads_used_next = 0, 0, 0
    last_load = None
    while True:
        ins = code[pc]
        op = ins[0]
        pc += 1
        if op == "label":
            continue
        steps += 1
        srcs = []
        if op in ("add", "xor", "and", "or", "sltu"):
            a, b = reg[ins[2]], reg[ins[3]]
            srcs = [ins[2], ins[3]]
            reg[ins[1]] = {"add": (a + b) & M32, "xor": a ^ b, "and": a & b, "or": a | b,
                           "sltu": int(a < b)}[op]
        elif op == "slli":
            srcs = [ins[2]]
            reg[ins[1]] = (reg[ins[2]] << ins[3]) & M32
        elif op == "srli":
            srcs = [ins[2]]
            reg[ins[1]] = reg[ins[2]] >> ins[3]
        elif op == "addi":
            srcs = [ins[2]]
            reg[ins[1]] = (reg[ins[2]] + ins[3]) & M32
        elif op == "lw":
            srcs = [ins[3]]
            reg[ins[1]] = load(reg[ins[3]] + ins[2])
        elif op == "lbu":
            srcs = [ins[3]]
            reg[ins[1]] = mem.get(reg[ins[3]] + ins[2], 0)
        elif op == "sw":
            srcs = [ins[1], ins[3]]
            store(reg[ins[3]] + ins[2], reg[ins[1]])
        elif op == "la":
            reg[ins[1]] = K_A
        elif op == "beq":
            srcs = [ins[1], ins[2]]
            if reg[ins[1]] == reg[ins[2]]:
                pc = labels[ins[3].rstrip("bf")]
        elif op == "j":
            pc = labels[ins[1].rstrip("bf")]
        elif op == "ret":
            break
        else:
            raise ValueError(op)
        if last_load is not None and last_load in srcs:
            loads_used_next += 1
        last_load = ins[1] if op in ("lw", "lbu") else None
    assert reg["sp"] == SP0
    for r in SAVED:
        assert reg[r] == saved_before[r], r
    return [load(ST_A + 4 * i) for i in range(16)], steps, loads_used_next


def ref_compress(state_words, block):
    """SHA-512 compression in plain Python."""
    def ror(x, n):
        return ((x >> n) | (x << (64 - n))) & 0xffffffffffffffff
    h = [(state_words[2 * i] << 32) | state_words[2 * i + 1] for i in range(8)]
    w = list(struct.unpack(">16Q", block))
    for t in range(16, 80):
        s0 = ror(w[t - 15], 1) ^ ror(w[t - 15], 8) ^ (w[t - 15] >> 7)
        s1 = ror(w[t - 2], 19) ^ ror(w[t - 2], 61) ^ (w[t - 2] >> 6)
        w.append((w[t - 16] + s0 + w[t - 7] + s1) & 0xffffffffffffffff)
    a, b, c, d, e, f, g, hh = h
    for t in range(80):
        t1 = (hh + (ror(e, 14) ^ ror(e, 18) ^ ror(e, 41)) + ((e & f) ^ (~e & g)) + K[t] + w[t]) \
            & 0xffffffffffffffff
        t2 = ((ror(a, 28) ^ ror(a, 34) ^ ror(a, 39)) + ((a & b) ^ (a & c) ^ (b & c))) \
            & 0xffffffffffffffff
        hh, g, f, e, d, c, b, a = g, f, e, (d + t1) & 0xffffffffffffffff, c, b, a, \
            (t1 + t2) & 0xffffffffffffffff
    out = [(x + y) & 0xffffffffffffffff for x, y in zip(h, [a, b, c, d, e, f, g, hh])]
    return [w for v in out for w in (v >> 32, v & M32)]


def selftest(code):
    import random
    rnd = random.Random(205)
    # 1. the Python reference against hashlib, on one padded block
    iv = [0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
          0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179]
    ivw = [w for v in iv for w in (v >> 32, v & M32)]
    msg = b"abc"
    blk = msg + b"\x80" + bytes(128 - len(msg) - 1 - 16) + struct.pack(">QQ", 0, 8 * len(msg))
    got = b"".join(struct.pack(">I", w) for w in ref_compress(ivw, blk))
    assert got == hashlib.sha512(msg).digest(), "reference model is wrong"
    # 2. the generated instructions against the reference
    for trial in range(200):
        st = [rnd.getrandbits(32) for _ in range(16)] if trial else ivw
        blk = bytes(rnd.getrandbits(8) for _ in range(128)) if trial else blk
        out, steps, stalls = run(code, st, blk)
        assert out == ref_compress(st, blk), "mismatch in trial %d" % trial
    print("ok: 200 blocks match; %d instructions per block, %d loads used by the "
          "very next instruction" % (steps, stalls))


if __name__ == "__main__":
    program = Gen().build()
    if "--test" in sys.argv:
        selftest(program)
    else:
        selftest(program)
        write_file(program)
