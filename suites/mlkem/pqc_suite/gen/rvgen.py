"""Small toolkit for generating and checking RV32IM assembly for the ESP32-C3.

A program is a list of items:
    ("label", name)
    (op, ...)            one instruction, operands as in GNU as

The toolkit provides
  * Sim        an interpreter with the measured cycle costs of the ESP32-C3
               (see suites/keccak NOTES.md: mul 1 cycle, mulh 5, load/store 1,
               one extra cycle when a load is used by the next instruction,
               taken branch 3, jump 2, call+return 4);
  * schedule   reorders independent instructions inside straight-line runs
               so that no instruction uses a register loaded by the one
               before it;
  * emit       GNU as text.

Generated code is always executed in the interpreter and compared with a
plain Python model before it is written out.
"""

MASK = 0xFFFFFFFF

LOADS = {"lw": (4, True), "lh": (2, True), "lhu": (2, False), "lb": (1, True), "lbu": (1, False)}
STORES = {"sw": 4, "sh": 2, "sb": 1}
RRR = {"add", "sub", "xor", "or", "and", "mul", "mulh", "mulhu", "sll", "srl", "sra", "slt", "sltu"}
RRI = {"addi", "slli", "srli", "srai", "andi", "ori", "xori", "slti", "sltiu"}
BRANCH2 = {"bne", "beq", "blt", "bge", "bltu", "bgeu"}
BRANCH1 = {"bnez", "beqz", "bltz", "bgez", "bgtz", "blez"}


def s32(v):
    v &= MASK
    return v - (1 << 32) if v & 0x80000000 else v


class Sim:
    def __init__(self, prog, symbols=None):
        self.prog = [i for i in prog]
        self.labels = {i[1]: k for k, i in enumerate(self.prog) if i[0] == "label"}
        self.symbols = dict(symbols or {})
        self.mem = {}
        self.reg = {}
        self.insns = 0
        self.cycles = 0

    # ---- memory helpers (little endian, byte addressed)
    def write(self, addr, value, size):
        for k in range(size):
            self.mem[addr + k] = (value >> (8 * k)) & 0xFF

    def read(self, addr, size, signed=False):
        v = 0
        for k in range(size):
            v |= self.mem.get(addr + k, 0) << (8 * k)
        if signed and v >> (8 * size - 1):
            v -= 1 << (8 * size)
        return v

    def run(self, entry, regs, limit=10_000_000):
        r = self.reg
        r.clear()
        r.update(regs)
        r["zero"] = 0
        pc = self.labels[entry]
        prev_load = None
        self.insns = self.cycles = 0
        while True:
            i = self.prog[pc]
            pc += 1
            o = i[0]
            if o == "label":
                continue
            self.insns += 1
            self.cycles += 1
            if self.insns > limit:
                raise RuntimeError("instruction limit")
            used = uses_defs(i)[0]
            if prev_load is not None and prev_load in used:
                self.cycles += 1
            prev_load = None
            if o in LOADS:
                size, signed = LOADS[o]
                r[i[1]] = self.read((r[i[2]] + i[3]) & MASK, size, signed) & MASK
                prev_load = i[1]
            elif o in STORES:
                self.write((r[i[2]] + i[3]) & MASK, r[i[1]], STORES[o])
            elif o in RRR:
                a, b = r[i[2]], r[i[3]]
                if o == "add":
                    v = a + b
                elif o == "sub":
                    v = a - b
                elif o == "xor":
                    v = a ^ b
                elif o == "or":
                    v = a | b
                elif o == "and":
                    v = a & b
                elif o == "mul":
                    v = a * b
                elif o == "mulh":
                    v = (s32(a) * s32(b)) >> 32
                    self.cycles += 4
                elif o == "mulhu":
                    v = (a * b) >> 32
                    self.cycles += 4
                elif o == "sll":
                    v = a << (b & 31)
                elif o == "srl":
                    v = a >> (b & 31)
                elif o == "sra":
                    v = s32(a) >> (b & 31)
                elif o == "slt":
                    v = int(s32(a) < s32(b))
                else:
                    v = int(a < b)
                r[i[1]] = v & MASK
            elif o in RRI:
                a, imm = r[i[2]], i[3]
                if o == "addi":
                    v = a + imm
                elif o == "slli":
                    v = a << imm
                elif o == "srli":
                    v = a >> imm
                elif o == "srai":
                    v = s32(a) >> imm
                elif o == "andi":
                    v = a & (imm & MASK)
                elif o == "ori":
                    v = a | (imm & MASK)
                elif o == "xori":
                    v = a ^ (imm & MASK)
                elif o == "slti":
                    v = int(s32(a) < imm)
                else:
                    v = int(a < (imm & MASK))
                r[i[1]] = v & MASK
            elif o == "li":
                r[i[1]] = i[2] & MASK
                if not -2048 <= s32(i[2]) < 2048 and (i[2] & 0xFFF):
                    self.insns += 1
                    self.cycles += 1
            elif o == "la":
                r[i[1]] = self.symbols[i[2]]
                self.insns += 1
                self.cycles += 1
            elif o == "mv":
                r[i[1]] = r[i[2]]
            elif o == "not":
                r[i[1]] = r[i[2]] ^ MASK
            elif o == "neg":
                r[i[1]] = (-r[i[2]]) & MASK
            elif o in BRANCH2 or o in BRANCH1:
                if o in BRANCH2:
                    a, b, target = r[i[1]], r[i[2]], i[3]
                    cond = {"bne": a != b, "beq": a == b, "blt": s32(a) < s32(b), "bge": s32(a) >= s32(b),
                            "bltu": a < b, "bgeu": a >= b}[o]
                else:
                    a, target = s32(r[i[1]]), i[2]
                    cond = {"bnez": a != 0, "beqz": a == 0, "bltz": a < 0, "bgez": a >= 0,
                            "bgtz": a > 0, "blez": a <= 0}[o]
                if cond:
                    pc = self.labels[target]
                    self.cycles += 2
            elif o == "j":
                pc = self.labels[i[1]]
                self.cycles += 1
            elif o == "ret":
                self.cycles += 1
                return
            else:
                raise ValueError("unknown instruction %r" % (i,))


def uses_defs(i):
    """(registers read, registers written, memory read, memory written);
    memory is identified by (base register, offset)."""
    o = i[0]
    if o in LOADS:
        return [i[2]], [i[1]], (i[2], i[3]), None
    if o in STORES:
        return [i[1], i[2]], [], None, (i[2], i[3])
    if o in RRR:
        return [i[2], i[3]], [i[1]], None, None
    if o in RRI or o in ("mv", "not", "neg"):
        return [i[2]], [i[1]], None, None
    if o in ("li", "la"):
        return [], [i[1]], None, None
    if o in BRANCH2:
        return [i[1], i[2]], [], None, None
    if o in BRANCH1:
        return [i[1]], [], None, None
    if o in ("j", "ret", "label"):
        return [], [], None, None
    raise ValueError(o)


def _schedule_run(ins, readonly_bases):
    n = len(ins)
    preds = [set() for _ in range(n)]
    last_def, readers = {}, {}
    for j, i in enumerate(ins):
        ru, rd, mr, mw = uses_defs(i)
        res_r = [x for x in ru if x != "zero"]
        res_w = list(rd)
        if mr and mr[0] not in readonly_bases:
            res_r.append(("m",) + mr)
        if mw:
            res_w.append(("m",) + mw)
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
            if not isinstance(r, tuple):
                # a changed base register invalidates the memory cells named with it
                for m in [m for m in last_def if isinstance(m, tuple) and m[1] == r]:
                    preds[j].add(last_def[m])
                    for k in readers.get(m, []):
                        preds[j].add(k)
                    del last_def[m]
                    readers.pop(m, None)
                for m in [m for m in readers if isinstance(m, tuple) and m[1] == r]:
                    for k in readers[m]:
                        if k != j:
                            preds[j].add(k)
                    readers[m] = []
    succs = [[] for _ in range(n)]
    npred = [len(p) for p in preds]
    for j in range(n):
        for p in preds[j]:
            succs[p].append(j)
    ready = sorted(j for j in range(n) if npred[j] == 0)
    out, prev = [], None
    while ready:
        pick = next((j for j in ready if prev is None or prev not in uses_defs(ins[j])[0]), ready[0])
        ready.remove(pick)
        out.append(ins[pick])
        prev = ins[pick][1] if ins[pick][0] in LOADS else None
        for s in succs[pick]:
            npred[s] -= 1
            if npred[s] == 0:
                ready.append(s)
        ready.sort()
    assert len(out) == n
    return out


def schedule(prog, readonly_bases=()):
    """Reorder instructions inside straight-line runs (between labels and
    branches). Loads through `readonly_bases` read constant tables."""
    out, run = [], []

    def flush():
        if run:
            out.extend(_schedule_run(run, set(readonly_bases)))
            run.clear()

    for i in prog:
        if i[0] == "label" or i[0] in BRANCH1 or i[0] in BRANCH2 or i[0] in ("j", "ret"):
            flush()
            out.append(i)
        else:
            run.append(i)
    flush()
    return out


def fmt(i):
    o = i[0]
    if o == "label":
        return "%s:" % i[1]
    if o in LOADS or o in STORES:
        return "    %s %s, %d(%s)" % (o, i[1], i[3], i[2])
    if o in RRR:
        return "    %s %s, %s, %s" % (o, i[1], i[2], i[3])
    if o in RRI:
        return "    %s %s, %s, %d" % (o, i[1], i[2], i[3])
    if o in ("mv", "not", "neg"):
        return "    %s %s, %s" % (o, i[1], i[2])
    if o == "li":
        return "    li %s, %d" % (i[1], s32(i[2]))
    if o == "la":
        return "    la %s, %s" % (i[1], i[2])
    if o in BRANCH2:
        return "    %s %s, %s, %s" % (o, i[1], i[2], i[3])
    if o in BRANCH1:
        return "    %s %s, %s" % (o, i[1], i[2])
    if o == "j":
        return "    j %s" % i[1]
    if o == "ret":
        return "    ret"
    raise ValueError(o)


def emit(prog):
    return "\n".join(fmt(i) for i in prog)


CALLEE_SAVED = ["s%d" % k for k in range(12)] + ["ra"]


def function(name, body, used_regs, frame_extra=0):
    """Wrap a body (list of items) into a global function that saves the
    callee-saved registers it uses. Local labels must be unique per file."""
    saved = [r for r in CALLEE_SAVED if r in used_regs]
    frame = (4 * len(saved) + frame_extra + 15) & ~15
    prog = [("label", name)]
    if frame:
        prog.append(("addi", "sp", "sp", -frame))
        for k, r in enumerate(saved):
            prog.append(("sw", r, "sp", frame_extra + 4 * k))
    prog += body
    if frame:
        for k, r in enumerate(saved):
            prog.append(("lw", r, "sp", frame_extra + 4 * k))
        prog.append(("addi", "sp", "sp", frame))
    prog.append(("ret",))
    return prog


def regs_of(prog):
    s = set()
    for i in prog:
        if i[0] == "label":
            continue
        ru, rd, _, _ = uses_defs(i)
        s.update(ru)
        s.update(rd)
    return s
