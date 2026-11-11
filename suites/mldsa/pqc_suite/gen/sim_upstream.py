#!/usr/bin/env python3
"""Run mldsa-native's RV32IM NTT assembly in the interpreter of rvgen.py to
predict its cycle count on the ESP32-C3 (and to check the interpreter's cost
model against the device). usage: sim_upstream.py"""
import os
import random
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "mlkem", "pqc_suite", "gen"))
from rvgen import Sim, BRANCH1, BRANCH2, LOADS, STORES, RRR, RRI  # noqa: E402

SRC = os.path.join(HERE, "..", "..", "..", "..", "third_party", "mldsa-native", "src", "native", "rv32im", "src")
Q = 8380417


def parse(path, entry):
    prog = [("label", entry)]
    text = re.sub(r"/\*.*?\*/", "", open(path).read(), flags=re.S)
    text = text.replace("\\\n", " ")        # join continued preprocessor lines
    for line in text.splitlines():
        line = line.split("//")[0].strip()
        if not line or line.startswith(("#", ".", "MLD_ASM_FN_SIZE", "MLD_ASM_FN_SYMBOL")):
            continue
        m = re.match(r"^(\w+):$", line)
        if m:
            prog.append(("label", m.group(1)))
            continue
        op, _, rest = line.partition(" ")
        args = [a.strip() for a in rest.split(",")] if rest else []
        num = lambda s: int(s, 0)
        if op in LOADS or op in STORES:
            m = re.match(r"(-?\w+)\((\w+)\)", args[1])
            prog.append((op, args[0], m.group(2), num(m.group(1))))
        elif op in RRR:
            prog.append((op, args[0], args[1], args[2]))
        elif op in RRI:
            prog.append((op, args[0], args[1], num(args[2])))
        elif op in ("mv", "neg", "not"):
            prog.append((op, args[0], args[1]))
        elif op == "li":
            prog.append((op, args[0], num(args[1])))
        elif op == "lui":
            prog.append(("li", args[0], (num(args[1]) << 12) & 0xFFFFFFFF))
        elif op in BRANCH2:
            prog.append((op, args[0], args[1], args[2]))
        elif op in BRANCH1:
            prog.append((op, args[0], args[1]))
        elif op == "ret":
            prog.append(("ret",))
        elif op == "j":
            prog.append(("j", args[0]))
        else:
            raise ValueError("cannot parse: " + line)
    return prog


def zetas():
    text = open(os.path.join(SRC, "rv32im_zetas.c")).read()
    body = text[text.index("{", text.index("mld_rv32im_ntt_zetas")):]
    body = body[:body.index("};")]
    return [int(x) for x in re.findall(r"-?\d+", re.sub(r"/\*.*?\*/", "", body, flags=re.S))]


def main():
    random.seed(5)
    tab = zetas()
    regs = {"sp": 0x80000, "ra": 0}
    for r in ["s%d" % i for i in range(12)] + ["t%d" % i for i in range(7)] + ["a%d" % i for i in range(8)]:
        regs.setdefault(r, 0)
    for fn, f in (("ntt", "mldsa_ntt_rv32im_asm.S"), ("intt", "mldsa_intt_rv32im_asm.S")):
        prog = parse(os.path.join(SRC, f), fn)
        sim = Sim(prog)
        for i, z in enumerate(tab):
            sim.write(0x40000 + 4 * i, z & 0xFFFFFFFF, 4)
        for i in range(256):
            sim.write(0x10000 + 4 * i, random.randint(-Q + 1, Q - 1) & 0xFFFFFFFF, 4)
        sim.run(fn, dict(regs, a0=0x10000, a1=0x40000))
        print("%-5s %6d instructions, %6d cycles predicted (%d table words)" % (fn, sim.insns, sim.cycles, len(tab)))


if __name__ == "__main__":
    main()
