#!/usr/bin/env python3
"""Port NGCC round-1 candidates to the ESP32-C3 in bulk.

Run through the wrapper `tools/ngcc/ngcc`, which sets up ESP-IDF:

    tools/ngcc/ngcc units [candidate]            list the parameter sets found in the packages
    tools/ngcc/ngcc gen <candidate> [param]      choose sources, write build/ngcc_gen/<unit>/
    tools/ngcc/ngcc host <candidate> [param]     gen + host build + sandboxed host run
    tools/ngcc/ngcc dev <candidate> [param]      device build + run from RAM + comparison with the host
    tools/ngcc/ngcc hostall [-j N]               `host` for every candidate and parameter set
    tools/ngcc/ngcc devall [--level 128]         `dev` for every unit that works on the host
    tools/ngcc/ngcc status                       write docs/ngcc/status.md and results/ngcc/status.json

A "unit" is one parameter set of one candidate, named <candidate>__<param>.

The packages are third-party code that is not redistributed here. They are
read from downloads/ngcc/src/ (see fetch.py and unpack.py); nothing derived
from their text is written outside build/. Their build scripts are never run:
this tool picks the C files itself and compiles them through this project's
CMake files. Host runs happen in a sandbox without network access, without
file writes and without the right to start processes.
"""
import argparse
import concurrent.futures
import csv
import fnmatch
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import pqc  # noqa: E402  (parse_lines, host_env)

DL = os.path.join(ROOT, "downloads", "ngcc")
SRC = os.path.join(DL, "src")
GEN = os.path.join(ROOT, "build", "ngcc_gen")
HOSTB = os.path.join(ROOT, "build", "ngcc_host")
RES = os.path.join(ROOT, "results", "ngcc")
CANDS = os.path.join(HERE, "cands")
SHIMS = os.path.join(HERE, "shims")
MANIFEST = os.path.join(RES, "packages.json")

# Ported from other sources by another part of the project.
ELSEWHERE = {"kem_Aigis-Enc+": "PQMagic", "sig_Aigis-Sig+": "PQMagic", "kem_NEV": "PQMagic",
             "kem_QIMEN-PIKE": "PQMagic", "sig_ReSolveD-ɑ": "PQMagic", "kex_NEV-AKE": "PQMagic",
             "kem_Scloud+": "scloudplus repository"}

KINDS = {"kem": "KEM", "sig": "SIG", "kex": "KEX"}

# The uniform ICCS interface: these names are never renamed.
API_KEEP = set("""
kem_get_pk_len_bytes kem_get_sk_len_bytes kem_get_ss_len_bytes kem_get_ct_len_bytes kem_keygen kem_enc kem_dec
sig_get_pk_len_bytes sig_get_sk_len_bytes sig_get_sn_len_bytes sig_keygen sig_sign sig_verify
kex_get_passes_num kex_get_pk_len_bytes kex_get_sk_len_bytes kex_get_sta_len_bytes kex_get_stb_len_bytes
kex_get_ss_len_bytes kex_get_total_msg_len_bytes kex_init_a kex_init_b kex_generate_pass1_msg_a
kex_generate_pass2_msg_b kex_generate_pass3_msg_a kex_generate_pass4_msg_b kex_generate_pass5_msg_a
kex_derive_ss_a kex_derive_ss_b drng_algorithm
""".split())
# Handled by fixed renames in the generated headers.
SPECIAL = {"randombytes", "get_random_number", "init_random_number", "main"}

BAD_COMP = re.compile(r"(?:^|[^a-z0-9])(opt|optimi[sz]ed|optimi[sz]ation|additional|avx2?|avx512|neon|aarch64|"
                      r"arm(?:v[78])?|cortex(?:-?m\d)?|m4|x86(?:_64)?|x64|sse\d*|simd|asm|riscv|windows|win32|"
                      r"msvc)(?:[^a-z0-9]|$)", re.I)
# Files that are never part of an algorithm: operating-system randomness,
# cycle counters and test scaffolding.
# Directories that hold extra copies or test doubles, never the reference code.
SIDE_COPY = re.compile(r"^(others?|api_pkc|中文资料|self[-_ ]?(assess?ment|evaluation).*|ngcc_bench|mock|"
                       r"resource_implementation|optimzed_implementation)$", re.I)
NAME_EXCLUDE = re.compile(r"^(randombytes.*|randomness_os|rng|cpucycles.*|cycles|osfreq|speed_print|ticks|"
                          r"test_support|generate_json_report|print_banner)\.c$", re.I)
MAIN_RE = re.compile(r"^[ \t]*(?:int|void)?[ \t\n]*main[ \t]*\(", re.M)

SCAN = {
    "process": r"\b(system|popen|execl|execlp|execle|execv|execvp|execve|fork|vfork|posix_spawn|dlopen|ptrace)\s*\(",
    "network": r"\b(socket|gethostbyname|getaddrinfo|recvfrom|sendto)\s*\(|<sys/socket\.h>|<netinet/|<arpa/|<netdb\.h>",
    "file": r"\b(fopen|freopen|creat|unlink|rmdir|chmod|chown|opendir)\s*\(|\bopen\s*\(\s*\"",
    "env": r"\b(getenv|setenv|putenv)\s*\(",
    "syscall": r"\b(mmap|mprotect|syscall)\s*\(",
}

COMMON_FLAGS = ["-w", "-fcommon", "-Wno-error=implicit-function-declaration", "-Wno-error=implicit-int",
                "-Wno-error=incompatible-pointer-types", "-Wno-error=int-conversion", "-Wno-error=return-type"]


def log(msg):
    print(msg, flush=True)


def read_text(path, limit=3 << 20):
    try:
        with open(path, "rb") as f:
            return f.read(limit).decode("utf-8", "replace")
    except OSError:
        return ""


def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def safe(s):
    return re.sub(r"[^A-Za-z0-9+._-]", "_", s)


def manifest():
    return json.load(open(MANIFEST))["packages"]


def bad_component(comp):
    return bool(BAD_COMP.search(comp)) and not re.search(r"ref", comp, re.I)


def bad_rel(rel):
    return any(bad_component(c) for c in rel.split(os.sep)[:-1])


def walk_c(top, base=None):
    """All .c files under top whose path (relative to base) has no excluded component."""
    base = base or top
    out = []
    for dp, dns, fns in os.walk(top):
        dns[:] = sorted(d for d in dns if not bad_component(d) and not d.startswith("."))
        for fn in sorted(fns):
            if fn.endswith(".c") and not fn.startswith("."):
                out.append(os.path.join(dp, fn))
    return out


# ---------------------------------------------------------------- discovery

def discover(pid):
    """Find the parameter sets of a package: one per file that implements the
    uniform interface (the definition of <kind>_get_pk_len_bytes)."""
    pkg = os.path.join(SRC, pid)
    kind = pid.split("_", 1)[0]
    pat = re.compile(r"\b%s_get_pk_len_bytes\s*\([^;{)]*\)\s*\{" % kind)
    files = walk_c(pkg)
    insts = [f for f in files if pat.search(read_text(f))]
    if not insts:  # e.g. a reference tree below a directory called "x86"
        insts = [os.path.join(dp, fn) for dp, _, fns in os.walk(pkg) for fn in sorted(fns)
                 if fn.endswith(".c") and not re.search(r"(?i)optimi|additional|avx|neon", dp)
                 and pat.search(read_text(os.path.join(dp, fn)))]
    # Some packages carry several copies of their code (self-assessment
    # bundles, translations). Two instance files are the same parameter set
    # when their paths below the reference root are equal: keep the copy
    # closest to the top-level Implementations directory. A directory with
    # several instance files keeps the one that is not a test.
    best = {}
    for f in insts:
        rel = os.path.relpath(f, pkg).split(os.sep)
        ref = next((i for i, c in enumerate(rel[:-1]) if re.search(r"ref", c, re.I)), None)
        tail = tuple(rel[ref + 1:-1]) if ref is not None else tuple(rel[-2:-1])
        rank = (any(SIDE_COPY.match(c) for c in rel[:ref if ref is not None else -1]),
                not rel[0].lower().startswith("implementation"), len(rel), "test" in rel[-1].lower(), rel)
        if tail not in best or rank < best[tail][0]:
            best[tail] = (rank, f)
    insts = sorted(f for _, f in best.values()
                   if len(best) == 1 or os.path.basename(os.path.dirname(f)) != "AlgorithmInstance")
    primary = [f for f in insts if not any(SIDE_COPY.match(c) for c in os.path.relpath(f, pkg).split(os.sep)[:-1])]
    if primary:
        insts = primary
    units = []
    for inst in insts:
        rel = os.path.relpath(inst, pkg).split(os.sep)
        ref_idx = next((i for i, c in enumerate(rel[:-1]) if re.search(r"ref", c, re.I)), None)
        floor = ref_idx + 1 if ref_idx is not None else 1  # do not climb above the reference root
        d = os.path.dirname(inst)
        while True:
            parent = os.path.dirname(d)
            depth = len(os.path.relpath(parent, pkg).split(os.sep)) if parent != pkg else 0
            if parent == pkg or depth < floor:
                break
            if sum(1 for o in insts if o.startswith(parent + os.sep)) != 1:
                break
            d = parent
        ref_root = os.path.join(pkg, *rel[:floor]) if ref_idx is not None else os.path.dirname(d)
        units.append({"pid": pid, "kind": KINDS[kind], "inst": inst, "dir": d, "ref_root": ref_root})
    # Name each unit after its directory; fall back to the instance directory.
    names = {}
    for u in units:
        name = os.path.basename(u["dir"])
        if u["dir"] == u["ref_root"] or sum(1 for o in units if os.path.basename(o["dir"]) == name) > 1:
            name = os.path.basename(os.path.dirname(u["inst"]))
            if sum(1 for o in units if os.path.basename(os.path.dirname(o["inst"])) == name) > 1:
                name = os.path.relpath(os.path.dirname(u["inst"]), u["ref_root"]).replace(os.sep, "_")
        u["param"] = safe(name)
        names[u["param"]] = names.get(u["param"], 0) + 1
    for u in units:
        u["unit"] = "%s__%s" % (safe(pid), u["param"])
    unit_dirs = [u["dir"] for u in units]
    for u in units:
        u["others"] = [d for d in unit_dirs if d != u["dir"]]
    # One instance file but several parameter sets that differ in -D flags:
    # one unit per parameter set of the evaluation data.
    rows = eval_rows(pid)
    if len(units) == 1 and len(rows) > 1 and len({" ".join(eval_defs(r)) for r in rows}) == len(rows):
        base, units = units[0], []
        for r in rows:
            u = dict(base, param=safe(r["param_set"]), eval_row=r)
            u["unit"] = "%s__%s" % (safe(pid), u["param"])
            units.append(u)
    return sorted(units, key=lambda u: pqc_natural(u["param"]))


def pqc_natural(text):
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", text)]


EVAL = os.path.join(DL, "_eval")
EVAL_ALIASES = {"kem_Hybrid_Equivalent_Punctured_and_Quasi-Cyclic": "HEP-QC",
                "kem_Quasi-Cyclic_Twisted_McEliece_Key_Encapsulation_Mechanism": "QCTM",
                "kem_UVW_Key_Encapsulation_Mechanism": "UVW-KEM"}
_EVAL_ROWS = {}


def eval_rows(pid):
    """Rows of the independent x86 evaluation (SYF19256, CC BY 4.0) for a
    package: parameter-set names, claimed levels, sizes and compiler flags."""
    if not _EVAL_ROWS:
        names = {(p["id"].split("_", 1)[0], norm(EVAL_ALIASES.get(p["id"], p["name"]))): p["id"] for p in manifest()}
        for fname, kind in (("KEM", "kem"), ("DSA", "sig"), ("KEX", "kex")):
            perf = os.path.join(EVAL, "performance", "pkc", "ref", fname + ".csv")
            corr = os.path.join(EVAL, "correctness", "pkc", "ref", fname + ".csv")
            if not os.path.exists(perf):
                continue
            verdicts = {}
            if os.path.exists(corr):
                for r in csv.DictReader(open(corr, encoding="utf-8-sig")):
                    verdicts[(r["package"], r["param_set"])] = r.get("verdict", "")
            for r in csv.DictReader(open(perf, encoding="utf-8-sig")):
                n = norm(r["package"])
                key = (kind, n)
                if key not in names:
                    cands = [k for k in names if k[0] == kind and (k[1].startswith(n) or n.startswith(k[1]))]
                    key = cands[0] if len(cands) == 1 else None
                if key:
                    r["x86_verdict"] = verdicts.get((r["package"], r["param_set"]), "")
                    _EVAL_ROWS.setdefault(names[key], []).append(r)
    return _EVAL_ROWS.get(pid, [])


LEVELS = ("128", "160", "192", "256", "384", "512")


def name_sig(name):
    """Numbers in a parameter-set name and a trailing variant letter (128f)."""
    m = re.search(r"\d([a-z])$", name.lower())
    return tuple(re.findall(r"\d+", name)), (m.group(1) if m else "")


def eval_match(pid, param, sizes=None):
    """The evaluation row of a parameter set: by name, by the numbers in the
    name, by claimed level, else by sizes."""
    rows = eval_rows(pid)
    n = norm(param)
    tokens = set(re.findall(r"[a-z]+|\d+", param.lower()))
    nums, suffix = name_sig(param)
    tries = [
        lambda r: norm(r["param_set"]) == n,
        lambda r: set(re.findall(r"[a-z]+|\d+", r["param_set"].lower())) == tokens,
        lambda r: bool(n) and (n in norm(r["param_set"]) or norm(r["param_set"]) in n)
        and (not nums or nums[-1] not in LEVELS or r["classical_security_level"] == nums[-1]
             or nums[-1] in name_sig(r["param_set"])[0]),
        lambda r: bool(nums) and name_sig(r["param_set"]) == (nums, suffix),
        lambda r: bool(nums) and name_sig(r["param_set"])[0][-1:] == nums[-1:] and name_sig(r["param_set"])[1] == suffix,
        lambda r: bool(nums) and nums[-1] in LEVELS and r["classical_security_level"] == nums[-1]
        and name_sig(r["param_set"])[1] == suffix,
    ]
    for t in tries:
        hit = [r for r in rows if t(r)]
        if len(hit) == 1:
            return hit[0]
        if len(hit) > 1:
            hit.sort(key=lambda r: -len(norm(r["param_set"])))
            if len(norm(hit[0]["param_set"])) > len(norm(hit[1]["param_set"])) and norm(hit[0]["param_set"]) in n:
                return hit[0]
    if sizes:
        by = [r for r in rows if str(sizes.get("pk")) == r.get("pk_bytes")
              and str(sizes.get("ct", sizes.get("sig"))) in (r.get("ct_bytes"), r.get("sn_bytes"))]
        if by and len({r["classical_security_level"] for r in by}) == 1:
            return by[0]
    return None


def eval_defs(row):
    """-D flags the evaluators used for a parameter set, without the ones that
    only describe their own build."""
    out = []
    for d in re.findall(r"(?:^|\s)-D\s*(\S+)", row.get("compile_flags", "")):
        if re.search(r"[\\\"'<>]", d) or re.match(r"(ALGORITHM_INSTANCE|PARAMNAME|\w*BENCH\w*|GMP\w*|\w*_LOOP_ROUNDS|"
                                                r"\w*AVX\w*|\w*X86\w*|TARGET_AMD64|VERBOSE|DEBUG)(=|$)", d):
            continue
        if d not in out:
            out.append(d)
    return out


def overrides(pid, param):
    path = os.path.join(CANDS, safe(pid) + ".json")
    o = {}
    if os.path.exists(path):
        d = json.load(open(path))
        o.update(d.get("all", {}))
        for k, v in d.get("units", {}).get(param, {}).items():
            if isinstance(v, list) and isinstance(o.get(k), list):
                o[k] = o[k] + v
            else:
                o[k] = v
    return o


def select_sources(u, o):
    """Candidate source files of a unit: its own directory first, then files
    of the reference tree that belong to no parameter set (shared code)."""
    own = walk_c(u["dir"])
    shared = []
    if u["ref_root"] != u["dir"]:
        for f in walk_c(u["ref_root"]):
            if not f.startswith(u["dir"] + os.sep) and not any(f.startswith(d + os.sep) for d in u["others"]):
                shared.append(f)
    # Code next to the reference tree in a directory with a "shared" name.
    side = []
    parent = os.path.dirname(u["ref_root"])
    if os.path.isdir(parent) and parent != os.path.join(SRC, u["pid"]):
        for name in sorted(os.listdir(parent)):
            if re.match(r"(?i)^_?(shared|common|commons|lib|libs|core)$", name) and os.path.isdir(os.path.join(parent, name)):
                side.append(os.path.join(parent, name))
    for extra in o.get("extra_dirs", []):
        side.append(os.path.join(SRC, u["pid"], extra))
    for d in side:
        shared += walk_c(d)
    files, dropped = [], {}
    for f in own + shared:
        rel = os.path.relpath(f, os.path.join(SRC, u["pid"]))
        text = read_text(f)
        if any(fnmatch.fnmatch(rel, pat) or fnmatch.fnmatch(os.path.basename(f), pat) for pat in o.get("exclude", [])):
            dropped[rel] = "excluded by the description file"
        elif NAME_EXCLUDE.match(os.path.basename(f)):
            dropped[rel] = "test or platform helper"
        elif MAIN_RE.search(text):
            dropped[rel] = "has main()"
        else:
            files.append(f)
    for extra in o.get("extra_srcs", []):
        files.append(os.path.join(SRC, u["pid"], extra))
    inc = []
    for top in [u["dir"]] + ([u["ref_root"]] if u["ref_root"] != u["dir"] else []):
        for dp, dns, fns in os.walk(top):
            dns[:] = sorted(d for d in dns if not bad_component(d) and not d.startswith("."))
            if top == u["ref_root"] and any(dp == d or dp.startswith(d + os.sep) for d in u["others"]):
                continue
            if any(fn.endswith((".h", ".inc", ".macros")) for fn in fns) and dp not in inc:
                inc.append(dp)
    for top in side:
        for dp, dns, fns in os.walk(top):
            dns[:] = sorted(d for d in dns if not bad_component(d) and not d.startswith("."))
            if any(fn.endswith((".h", ".inc", ".macros")) for fn in fns) and dp not in inc:
                inc.append(dp)
    inst_dir = os.path.dirname(u["inst"])
    inc.sort(key=lambda d: (d != inst_dir, not d.startswith(u["dir"]), d.count(os.sep), d))
    inc = [os.path.join(SRC, u["pid"], i) for i in o.get("incs", [])] + inc
    return files, inc, dropped


def scan(files):
    hits = {}
    for f in files:
        text = re.sub(r"/\*.*?\*/|//[^\n]*", " ", read_text(f), flags=re.S)  # comments do not count
        for cat, pat in SCAN.items():
            m = re.search(pat, text)
            if m:
                hits.setdefault(cat, []).append("%s: %s" % (os.path.basename(f), m.group(0).strip()[:40]))
    return hits


def rb_len(files, incdirs):
    """Width of the length argument of the candidate's randombytes()."""
    votes = {32: 0, 64: 0}
    seen = list(files)
    for d in incdirs:
        seen += glob.glob(os.path.join(d, "*.h"))
    for f in seen:
        for m in re.finditer(r"\brandombytes\s*\(([^;{)]*)\)\s*[;{]", read_text(f)):
            args = m.group(1).split(",")
            if len(args) != 2 or not re.search(r"[a-z_]", args[1]):
                continue
            a = args[1]
            if re.search(r"long\s+long|uint64_t|u64", a):
                votes[64] += 1
            elif re.search(r"size_t|unsigned|uint32_t|int\b|long", a):
                votes[32] += 1
    return (32 if votes[32] > votes[64] else 64), votes


# ---------------------------------------------------------------- generation

def riscv_gcc():
    return shutil.which("riscv32-esp-elf-gcc")


def compile_cmd(cc, src, out, incs, defs, extra, hdr, syntax_only=False):
    cmd = [cc] + COMMON_FLAGS + list(extra)
    for i in incs:
        cmd += ["-I", i]
    for d in defs:
        cmd.append("-D" + d)
    cmd += ["-include", hdr]
    cmd += ["-fsyntax-only", src] if syntax_only else ["-c", src, "-o", out]
    return cmd


RB_FUNC = re.compile(r"\b(?:void|int|size_t)\s+(\w+)\s*\(\s*(?:const\s+)?(?:unsigned\s+char|uint8_t|void|u8|uchar|"
                     r"BYTE|byte)\s*\*\s*\w+\s*,\s*([\w\s]+?)\s+\w+\s*\)\s*\{")


def rng_wrappers(pid, dropped):
    """Random-byte functions defined in the files that were left out because
    they read the operating system's generator: name -> our entry point."""
    out = {}
    for rel, why in dropped.items():
        if "test or platform helper" not in why:
            continue
        for name, ltype in RB_FUNC.findall(read_text(os.path.join(SRC, pid, rel))):
            if name in SPECIAL or name.startswith("ngcc_"):
                continue
            out[name] = "ngcc_rb_ull" if re.search(r"long\s+long|uint64_t|u64", ltype) else "ngcc_rb_sz"
    return out


def write_rename(path, symbols, drng, extra=None):
    lines = ["/* Generated by tools/ngcc/ngcc.py: keeps the candidate's global names apart from",
             " * this project's and from the C library's, and wraps its random source. */",
             "#ifndef NGCC_RENAME_H", "#define NGCC_RENAME_H", "#include <stddef.h>"]
    if drng:
        lines += ["#define get_random_number iccs_get_random_number",
                  "#define init_random_number iccs_init_random_number"]
    else:
        lines += ["#define get_random_number ngcc_get_random_number",
                  "#define init_random_number ngcc_init_random_number",
                  "#define randombytes ngcc_randombytes"]
        lines += ["#define %s %s" % kv for kv in sorted((extra or {}).items())]
        lines += ["#define %s ngc_%s" % (s, s) for s in sorted(symbols) if s not in (extra or {})]
    lines += ["#endif", ""]
    with open(path, "w") as f:
        f.write("\n".join(lines))


def gen(u, verbose=False):
    """Choose and check the sources of a unit and write its build description."""
    o = overrides(u["pid"], u["param"])
    gdir = os.path.join(GEN, u["unit"])
    shutil.rmtree(gdir, ignore_errors=True)
    os.makedirs(os.path.join(gdir, "obj"))
    files, incs, dropped = select_sources(u, o)
    defs = list(o.get("defs", []))
    row = u.get("eval_row") or eval_match(u["pid"], u["param"])
    if row and not o.get("no_eval_defs"):
        for d in eval_defs(row):
            if d.split("=")[0] not in [x.split("=")[0] for x in defs]:
                defs.append(d)
    copts = list(o.get("copts", []))
    incs = incs + [os.path.join(HERE, "compat", "host")]
    drng_pat = re.compile(r"\bint\s+get_random_number\s*\([^;{)]*\)\s*\{")
    drng = [f for f in files if drng_pat.search(read_text(f))]
    drng_src = drng[0] if drng else None
    files = [f for f in files if f not in drng]

    hdr0 = os.path.join(gdir, "ngcc_rename.h")
    hdr_drng = os.path.join(gdir, "ngcc_rename_drng.h")
    wrappers = rng_wrappers(u["pid"], dropped)
    wrappers.update(o.get("rename", {}))
    write_rename(hdr0, [], False, wrappers)
    write_rename(hdr_drng, [], True)
    env = pqc.host_env()

    def host_compile(i_f):
        i, f = i_f
        obj = os.path.join(gdir, "obj", "%04d.o" % i)
        r = subprocess.run(compile_cmd("cc", f, obj, incs, defs, copts + ["-O0"], hdr_drng if f == drng_src else hdr0),
                           capture_output=True, text=True, env=env)
        if r.returncode != 0:
            err = [l for l in r.stderr.splitlines() if "error" in l]
            return f, None, (err[0] if err else r.stderr.strip().splitlines()[-1] if r.stderr.strip() else "?")[-200:]
        nm = subprocess.run(["nm", "-g", "-U", obj], capture_output=True, text=True)
        syms = set()
        for ln in nm.stdout.splitlines():
            p = ln.split()
            if len(p) >= 3 and p[-2] in "TDBSC":
                s = p[-1][1:] if p[-1].startswith("_") else p[-1]
                if re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", s):
                    syms.add(s)
        return f, syms, None

    allf = files + ([drng_src] if drng_src else [])
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        res = list(ex.map(host_compile, enumerate(allf)))
    kept, symbols, defined_by, own_rb = [], set(), {}, []
    pkg = os.path.join(SRC, u["pid"])
    for f, syms, err in res:
        rel = os.path.relpath(f, pkg)
        if syms is None:
            dropped[rel] = "does not compile on the host: " + err
            if f == drng_src:
                drng_src = None
            continue
        if f == drng_src:
            continue
        if "ngcc_randombytes" in syms:
            if "get_random_number" in read_text(f) or o.get("keep_randombytes"):
                own_rb.append(f)  # a wrapper around the ICCS generator: keep it, under a private name
                syms = (syms - {"ngcc_randombytes"}) | {"ngc_own_randombytes"}
            else:
                dropped[rel] = "defines its own randombytes()"
                continue
        kept.append(f)
        for s in syms:
            defined_by.setdefault(s, []).append(rel)
        symbols |= syms
    has_drng_alg = "drng_algorithm" in symbols
    rename = {s for s in symbols if s not in API_KEEP and s not in SPECIAL and not s.startswith("ngcc_")
              and not s.startswith("iccs_") and not s.startswith("ngc_")}
    with open(os.path.join(gdir, "ngcc_rename_ownrb.h"), "w") as f:
        f.write("/* Generated: this file has its own randombytes() on top of the ICCS generator. */\n"
                "#undef randombytes\n#define randombytes ngc_own_randombytes\n")
    write_rename(hdr0, rename, False, wrappers)

    # Second check with the RISC-V compiler and the final rename header.
    rv = riscv_gcc()
    rv_dropped = {}
    if rv:
        def rv_check(f):
            r = subprocess.run(compile_cmd(rv, f, None, incs[:-1] + [os.path.join(HERE, "compat", "device")], defs,
                                           copts + ["-march=rv32imc_zicsr_zifencei"],
                                           hdr_drng if f == drng_src else hdr0, syntax_only=True),
                               capture_output=True, text=True)
            if r.returncode != 0:
                err = [l for l in r.stderr.splitlines() if "error" in l]
                return f, (err[0] if err else "?")[-200:]
            return f, None
        with concurrent.futures.ThreadPoolExecutor(8) as ex:
            for f, err in ex.map(rv_check, kept + ([drng_src] if drng_src else [])):
                if err:
                    rv_dropped[os.path.relpath(f, pkg)] = err
    shutil.rmtree(os.path.join(gdir, "obj"), ignore_errors=True)

    # Most candidates hash with the SM3-based helper functions that ICCS
    # supplies (auxfunc.c: sm3hash, pseudohash, pseudoXOF). A copy of that
    # file with one added line sends its SM3 compression to components/prims
    # when the optimized backend is selected, so the suite can time "opt".
    shim_srcs = []
    aux = [f for f in kept if os.path.basename(f).lower() == "auxfunc.c"]
    sig = re.compile(r"static\s+void\s+sm3_bit_compress\s*\(\s*unsigned\s+int\s+dgst\[8\]\s*,\s*const\s+unsigned\s+char\s*\*"
                     r"\s*msg\s*,\s*unsigned\s+long\s+long\s+blocks\s*\)\s*\{")
    if aux and not o.get("no_route"):
        text = read_text(aux[0])
        m = sig.search(text)
        if m:
            routed = (text[:m.end()] +
                      "\n\tif (pqc_prims_selected() == PQC_PRIMS_OPT) { pqc_sm3_compress((uint32_t *)dgst, msg, "
                      "(size_t)blocks); return; }\n" + text[m.end():])
            hdr = os.path.join(os.path.dirname(aux[0]), "auxfunc.h")
            if os.path.exists(hdr):  # otherwise it is found through the candidate's include paths
                routed = re.sub(r'#\s*include\s*"auxfunc\.h"', '#include "%s"' % hdr, routed)
            routed = "#include <stdint.h>\n#include <stddef.h>\n#include \"pqc_prims.h\"\n" + routed
            rp = os.path.join(gdir, "auxfunc_routed.c")
            with open(rp, "w") as f:
                f.write(routed)
            shim_srcs.append(rp)
            kept.remove(aux[0])

    width, votes = rb_len(kept, incs)
    width = o.get("rb_len", width)
    suite_defs = ["NGCC_CAND_HAS_DRNG_ALGORITHM=1"] if has_drng_alg else []
    if shim_srcs:
        suite_defs.append("NGCC_HAS_OPT=1")
    host_link = []
    if u["kind"] == "KEX" and "kex_generate_pass3_msg_a" not in symbols:
        host_link.append("-Wl,-U,_kex_generate_pass3_msg_a")
    alg = safe(o.get("alg", u["param"])).lower()[:40]

    def cm_list(name, items):
        return "set(%s\n%s)\n" % (name, "".join('    "%s"\n' % i for i in items))

    with open(os.path.join(gdir, "cand.cmake"), "w") as f:
        f.write("# Generated by tools/ngcc/ngcc.py for %s. Not committed.\n" % u["unit"])
        f.write("set(NGCC_KIND %s)\nset(NGCC_ALG \"%s\")\nset(NGCC_RB_LEN %d)\n" % (u["kind"], alg, width))
        f.write(cm_list("NGCC_SRCS", kept))
        f.write(cm_list("NGCC_RV_EXCLUDE", [os.path.join(pkg, r) for r in rv_dropped]))
        f.write(cm_list("NGCC_DRNG_SRC", [drng_src] if drng_src else []))
        f.write(cm_list("NGCC_INCS", incs[:-1]))
        f.write(cm_list("NGCC_DEFS", defs))
        f.write(cm_list("NGCC_COPTS", copts))
        f.write(cm_list("NGCC_SUITE_DEFS", suite_defs))
        f.write(cm_list("NGCC_HOST_LINK_OPTS", host_link))
        f.write(cm_list("NGCC_SHIM_SRCS", shim_srcs))
        f.write(cm_list("NGCC_OWN_RB_SRCS", own_rb))
    info = {"unit": u["unit"], "pid": u["pid"], "param": u["param"], "kind": u["kind"], "alg": alg,
            "eval": ({k: row[k] for k in ("param_set", "classical_security_level", "pk_bytes", "sk_bytes",
                                          "x86_verdict") if k in row} if row else None),
            "dir": os.path.relpath(u["dir"], pkg), "inst": os.path.relpath(u["inst"], pkg),
            "sources": [os.path.relpath(f, pkg) for f in kept],
            "drng": os.path.relpath(drng_src, pkg) if drng_src else None,
            "dropped": dropped, "riscv_errors": rv_dropped, "defs": defs,
            "rb_len": width, "rb_votes": votes, "rng_wrappers": wrappers, "scan": scan(kept),
            "sm3_routed": bool(shim_srcs),
            "uses_malloc": sorted({os.path.basename(f) for f in kept
                                   if re.search(r"\b(malloc|calloc|realloc|aligned_alloc)\s*\(", read_text(f))}),
            "system_headers": sorted({h for f in kept for h in re.findall(r"#\s*include\s*<([^>]+)>", read_text(f))}),
            "override": o}
    with open(os.path.join(gdir, "info.json"), "w") as f:
        json.dump(info, f, indent=1)
    if verbose:
        log("%s: %d sources, %d dropped, %d fail on RISC-V, drng=%s, randombytes length %d bits"
            % (u["unit"], len(kept), len(dropped), len(rv_dropped), bool(drng_src), width))
        for k, v in list(dropped.items()) + list(rv_dropped.items()):
            if "main()" not in v and "helper" not in v:
                log("   dropped %s: %s" % (k, v))
        if info["scan"]:
            log("   scan: %s" % json.dumps(info["scan"]))
    return info


# ---------------------------------------------------------------- host

SANDBOX = ('(version 1)(allow default)(deny network*)(deny file-write*)'
           '(allow file-write* (literal "/dev/null"))(deny process-fork)')


def result_path(unit, host=False):
    return os.path.join(RES, "%s%s.json" % (unit, ".host" if host else ""))


def clean(obj):
    """Remove machine-specific path prefixes from everything that is saved."""
    if isinstance(obj, str):
        obj = obj.replace(os.path.join(SRC, ""), "").replace(ROOT + os.sep, "").replace(ROOT, ".")
        return obj.replace(os.path.expanduser("~"), "~")
    if isinstance(obj, list):
        return [clean(x) for x in obj]
    if isinstance(obj, dict):
        return {k: clean(v) for k, v in obj.items()}
    return obj


def save(unit, host, parsed, extra):
    os.makedirs(RES, exist_ok=True)
    parsed = dict(parsed)
    parsed.update(extra)
    parsed = clean(parsed)
    with open(result_path(unit, host), "w") as f:
        json.dump(parsed, f, indent=1)
        f.write("\n")


def host_run(u, timeout=300, iters=None, regen=True):
    """Generate, build for the host and run in the sandbox. Returns a verdict."""
    t0 = time.time()
    info = gen(u) if regen or not os.path.exists(os.path.join(GEN, u["unit"], "info.json")) else \
        json.load(open(os.path.join(GEN, u["unit"], "info.json")))
    o = info["override"]
    base = {"unit": u["unit"], "pid": u["pid"], "param": u["param"], "kind": u["kind"]}
    blocked = [c for c in ("process", "network", "syscall") if c in info["scan"]]
    if blocked and not o.get("scan_reviewed"):
        save(u["unit"], True, pqc.parse_lines([]), dict(base, verdict="not run",
             reason="source scan found %s calls (%s); needs a review" % ("/".join(blocked),
                                                                         "; ".join(info["scan"][blocked[0]][:3]))))
        return "not run"
    bdir = os.path.join(HOSTB, u["unit"])
    env = pqc.host_env()
    cfg = ["cmake", "-S", os.path.join(ROOT, "host"), "-B", bdir, "-G", "Ninja", "-DPQC_SUITE=ngcc",
           "-DNGCC_CAND=" + safe(u["pid"]), "-DNGCC_PARAM=" + u["param"],
           "-DNGCC_ITERS=%d" % (iters or o.get("host_iters", 3))]
    r = subprocess.run(cfg, capture_output=True, text=True, env=env)
    if r.returncode == 0:
        r = subprocess.run(["cmake", "--build", bdir], capture_output=True, text=True, env=env)
    if r.returncode != 0:
        out = (r.stdout + r.stderr).splitlines()
        errs = [l for l in out if re.search(r"error:|Undefined symbols|duplicate symbol|\"_.*\", referenced from|"
                                            r"ld: |No such file", l)]
        und = sorted(set(re.findall(r'"_(\w+)", referenced from', "\n".join(out))))
        reason = ("undefined symbols: " + ", ".join(und[:12])) if und else \
            (errs[0].strip()[-240:] if errs else "build failed")
        os.makedirs(os.path.join(RES, "logs"), exist_ok=True)
        with open(os.path.join(RES, "logs", u["unit"] + ".hostbuild.log"), "w") as f:
            f.write("\n".join(out[-200:]) + "\n")
        save(u["unit"], True, pqc.parse_lines([]), dict(base, verdict="does not build", reason=reason,
                                                        build_errors=[e.strip()[-300:] for e in errs[:12]]))
        return "does not build"
    exe = os.path.join(bdir, "suite_host")
    try:
        r = subprocess.run(["sandbox-exec", "-p", SANDBOX, exe], capture_output=True, timeout=timeout)
        out = r.stdout.decode("utf-8", "replace").splitlines()
        rc = r.returncode
        crash = None if rc in (0, 1) else "exit status %d" % rc
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode("utf-8", "replace").splitlines()
        crash = "host run exceeded %d s" % timeout
    parsed = pqc.parse_lines(out)
    extra = dict(base, seconds=round(time.time() - t0, 1))
    kat = kat_verdict(u, parsed)
    extra["kat"] = kat
    bad = [k for k, v in parsed["checks"].items() if v != "PASS"]
    failed = [k for k, v in parsed["results"].items() if v["rc"] != 0]
    if crash or not parsed["complete"]:
        extra.update(verdict="crashes", reason=crash or "run did not finish")
    elif bad or failed:
        extra.update(verdict="self-test fails", reason="failed: " + ", ".join(sorted(set(bad + failed)))[:200])
    else:
        extra.update(verdict="works", reason="")
    save(u["unit"], True, parsed, extra)
    return extra["verdict"]


# ---------------------------------------------------------------- known-answer files

def kat_files(pid):
    out = []
    for dp, dns, fns in os.walk(os.path.join(SRC, pid)):
        for fn in fns:
            if re.match(r"(?i)^(KAT_|PQCkemKAT|PQCsignKAT).*\.(txt|rsp)$", fn):
                out.append(os.path.join(dp, fn))
    return out


def kat_first_vector(path):
    """SHA-256 fingerprints (as bench_digest prints them) of the fields of
    'Count = 0' in a KAT file, without loading the whole file."""
    fields = {}
    started = False
    with open(path, "rb") as f:
        for raw in f:
            ln = raw.decode("ascii", "replace").strip()
            if ln.startswith("Count"):
                if started:
                    break
                started = True
                continue
            m = re.match(r"^([A-Za-z0-9_]+)\s*=\s*([0-9A-Fa-f]*)$", ln)
            if started and m and not m.group(1).endswith("_Len") and len(m.group(2)) % 2 == 0:
                data = bytes.fromhex(m.group(2))
                fields[m.group(1).lower()] = [hashlib.sha256(data).hexdigest()[:32], len(data)]
    return fields


_KAT_CACHE = {}
_KAT_LOCK = __import__("threading").Lock()


def kat_index(pid):
    with _KAT_LOCK:
        if pid not in _KAT_CACHE:
            cache = os.path.join(DL, "_kat", safe(pid) + ".json")
            idx = None
            if os.path.exists(cache):
                try:
                    idx = json.load(open(cache))
                except ValueError:
                    idx = None
            if idx is None:
                idx = {os.path.relpath(p, os.path.join(SRC, pid)): kat_first_vector(p) for p in kat_files(pid)}
                os.makedirs(os.path.dirname(cache), exist_ok=True)
                tmp = "%s.%d.tmp" % (cache, os.getpid())
                with open(tmp, "w") as f:
                    json.dump(idx, f)
                os.replace(tmp, cache)
            _KAT_CACHE[pid] = idx
        return _KAT_CACHE[pid]


KAT_MAP = {"KEM": {"kat0-pk": "pk", "kat0-sk": "sk", "kat0-ct": "ct", "kat0-ss": "ss"},
           "SIG": {"kat0-pk": "pk", "kat0-sk": "sk", "kat0-sig": "sn"},
           "KEX": {"kat0-pka": "pka", "kat0-pkb": "pkb", "kat0-m1": "m1", "kat0-m2": "m2", "kat0-ss": "ss"}}


def kat_verdict(u, parsed):
    """Compare the fingerprints of the run seeded like test vector 0 with the
    package's own KAT files."""
    got = {}
    for label, d in parsed["digests"].items():
        what = label.split("/")[-1]
        if "/ref/" in label and what in KAT_MAP[u["kind"]]:
            got[KAT_MAP[u["kind"]][what]] = [d["sha256_16"], d["len"]]
    if not got:
        return {"result": "no output"}
    idx = kat_index(u["pid"])
    if not idx:
        return {"result": "no KAT file in the package"}
    first = "pk" if "pk" in got else "pka"
    best = None
    for path, fields in idx.items():
        common = [k for k in got if k in fields]
        if not common:
            continue
        eq = [k for k in common if fields[k] == got[k]]
        if first in eq and (best is None or len(eq) > len(best[1])):
            best = (path, eq, common)
    if best is None:
        return {"result": "differs", "detail": "no KAT file starts with this public key"}
    path, eq, common = best
    if len(eq) == len(common):
        return {"result": "match", "file": path, "fields": sorted(eq)}
    return {"result": "differs", "file": path, "equal": sorted(eq), "different": sorted(set(common) - set(eq))}


# ---------------------------------------------------------------- device

def dev_run(u, worker="0", stack=0, iters=None, timeout=420, idle=240):
    """Build for the ESP32-C3, run from RAM and compare with the host run."""
    hp = result_path(u["unit"], True)
    host = json.load(open(hp)) if os.path.exists(hp) else None
    name = "ngcc_w" + worker
    o = overrides(u["pid"], u["param"])
    stack = stack or o.get("stack", 0)
    iters = iters or o.get("iters", 3)
    cmd = [sys.executable, os.path.join(ROOT, "tools", "pqc.py"), "run", "ngcc", "--build-name", name,
           "-D", "NGCC_CAND=" + safe(u["pid"]), "-D", "NGCC_PARAM=" + u["param"], "-D", "NGCC_STACK=%d" % stack,
           "-D", "NGCC_ITERS=%d" % iters, "--timeout", str(timeout), "--idle", str(idle)]
    t0 = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    out = r.stdout + r.stderr
    base = {"unit": u["unit"], "pid": u["pid"], "param": u["param"], "kind": u["kind"],
            "stack_given": stack, "iters": iters, "seconds": round(time.time() - t0, 1)}
    tmp = os.path.join(ROOT, "results", name + ".json")
    if "build failed" in out:
        m = re.search(r"region `?(\w+)'? overflowed by (\d+) bytes", out)
        errs = [l.strip()[-260:] for l in out.splitlines() if re.search(r"error:|undefined reference|overflowed|"
                                                                      r"will not fit|multiple definition", l)]
        reason = ("image does not fit in RAM: %s overflows by %d KB" % (m.group(1), int(m.group(2)) // 1024 + 1)) \
            if m else (errs[0] if errs else "build failed")
        und = sorted(set(re.findall(r"undefined reference to `(\w+)'", out)))
        if und and not m:
            reason = "undefined on the device: " + ", ".join(und[:10])
        save(u["unit"], False, pqc.parse_lines([]), dict(base, verdict="does not build", reason=reason,
                                                         build_errors=errs[:10]))
        return "does not build"
    parsed = json.load(open(tmp)) if os.path.exists(tmp) else pqc.parse_lines([])
    if os.path.exists(tmp):
        os.remove(tmp)
    logp = os.path.join(ROOT, "results", "logs", name + ".device.log")
    if os.path.exists(logp):
        os.makedirs(os.path.join(RES, "logs"), exist_ok=True)
        shutil.copy(logp, os.path.join(RES, "logs", u["unit"] + ".device.log"))
    m = re.search(r"\[built ngcc in \d+ s, image (\d+) bytes\]", out)
    if m:
        base["image_bytes"] = int(m.group(1))
    crash = re.search(r"RUN DID NOT FINISH: (.*)", out)
    extra = dict(base, kat=kat_verdict(u, parsed))
    bad = [k for k, v in parsed["checks"].items() if v != "PASS"]
    failed = ["%s rc=%d" % (k.split("/")[-1], v["rc"]) for k, v in parsed["results"].items() if v["rc"] != 0]
    if crash:
        extra.update(verdict="crashes", reason=crash.group(1)[:200])
    elif not parsed.get("complete"):
        extra.update(verdict="crashes", reason="run did not finish")
    elif bad or failed:
        why = "failed: " + ", ".join(sorted(set([b.split("/")[-1] for b in bad] + failed)))[:200]
        if any("rc=-100" in f for f in failed):
            why = "not enough free RAM for the job stack (%s)" % why
        extra.update(verdict="self-test fails", reason=why)
    else:
        extra.update(verdict="works", reason="")
    if host is not None:
        dd, hd = parsed["digests"], host.get("digests", {})
        same = [k for k in dd if hd.get(k) == dd[k]]
        diff = [k for k in dd if k in hd and hd[k] != dd[k]]
        extra["host_check"] = {"equal": len(same), "different": len(diff), "missing": len(dd) - len(same) - len(diff),
                               "different_labels": [d.split("/")[-1] for d in diff][:8]}
        if extra["verdict"] == "works" and (diff or not same):
            extra.update(verdict="differs from host", reason="fingerprints differ: " +
                         ", ".join(extra["host_check"]["different_labels"]))
    save(u["unit"], False, parsed, extra)
    return extra["verdict"]


# ---------------------------------------------------------------- commands

def all_units(only=None, param=None):
    out = []
    for p in manifest():
        pid = p["id"]
        if only and pid != only and safe(pid) != only:
            continue
        if pid in ELSEWHERE and not only:
            continue
        if not os.path.exists(os.path.join(SRC, pid, ".unpacked")):
            continue
        us = discover(pid)
        if param:
            us = [u for u in us if u["param"] == param]
        out += us
    return out


def cmd_units(args):
    for p in manifest():
        if args.cand and p["id"] != args.cand:
            continue
        if not os.path.exists(os.path.join(SRC, p["id"], ".unpacked")):
            log("%s: not unpacked" % p["id"])
            continue
        us = discover(p["id"])
        log("%s: %d units%s" % (p["id"], len(us), " (ported elsewhere)" if p["id"] in ELSEWHERE else ""))
        for u in us:
            log("    %-40s %s" % (u["param"], os.path.relpath(u["dir"], os.path.join(SRC, p["id"]))))


def cmd_gen(args):
    for u in all_units(args.cand, args.param):
        gen(u, verbose=True)


def cmd_host(args):
    for u in all_units(args.cand, args.param):
        v = host_run(u, timeout=args.timeout)
        r = json.load(open(result_path(u["unit"], True)))
        log("%-58s host: %-16s kat: %-8s %s" % (u["unit"], v, r.get("kat", {}).get("result", "-"), r.get("reason", "")[:110]))


def cmd_hostall(args):
    units = all_units(args.cand)
    if args.missing:
        units = [u for u in units if not os.path.exists(result_path(u["unit"], True))]
    if args.failed:
        units = [u for u in units if os.path.exists(result_path(u["unit"], True))
                 and json.load(open(result_path(u["unit"], True))).get("verdict") != "works"]
    log("%d units" % len(units))

    def one(u):
        try:
            v = host_run(u, timeout=args.timeout)
        except Exception as e:  # keep the batch going
            v = "tool error: %s" % e
        r = json.load(open(result_path(u["unit"], True))) if os.path.exists(result_path(u["unit"], True)) else {}
        log("%-58s host: %-16s kat: %-8s %s" % (u["unit"], v, r.get("kat", {}).get("result", "-"),
                                               r.get("reason", "")[:110]))
        return v
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        vs = list(ex.map(one, units))
    for v in sorted(set(vs)):
        log("  %-20s %d" % (v, vs.count(v)))


def cmd_dev(args):
    for u in all_units(args.cand, args.param):
        v = dev_run(u, worker=args.worker, stack=args.stack, iters=args.iters, timeout=args.timeout, idle=args.idle)
        r = json.load(open(result_path(u["unit"])))
        hc = r.get("host_check", {})
        log("%-58s device: %-18s kat: %-8s host-equal: %s/%s %s" % (
            u["unit"], v, r.get("kat", {}).get("result", "-"), hc.get("equal", "-"),
            hc.get("equal", 0) + hc.get("different", 0) + hc.get("missing", 0), r.get("reason", "")[:120]))


# ---------------------------------------------------------------- device, in batches

BINS = os.path.join(ROOT, "build", "ngcc_bins")


def host_result(unit):
    hp = result_path(unit, True)
    return json.load(open(hp)) if os.path.exists(hp) else None


def plan_iters(host):
    """Iterations for the device run, from the host timings. The ESP32-C3 is
    several hundred times slower than the build machine."""
    slow = max([r["avg_us"] for r in host.get("results", {}).values()] or [0]) * 600 / 1e6
    return (3 if slow < 4 else 1), slow


def dev_build(u, worker, stack=0, stack_pct=60, iters=None):
    """Build the RAM image of a unit and keep it in build/ngcc_bins/."""
    host = host_result(u["unit"])
    o = overrides(u["pid"], u["param"])
    if iters is None:
        iters = o.get("iters") or (plan_iters(host)[0] if host else 3)
    name = "ngcc_w%s" % worker
    cmd = [sys.executable, os.path.join(ROOT, "tools", "pqc.py"), "build", "ngcc", "--build-name", name,
           "-D", "NGCC_CAND=" + safe(u["pid"]), "-D", "NGCC_PARAM=" + u["param"],
           "-D", "NGCC_STACK=%d" % (stack or o.get("stack", 0)),
           "-D", "NGCC_STACK_PCT=%d" % stack_pct, "-D", "NGCC_ITERS=%d" % iters]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    out = r.stdout + r.stderr
    base = {"unit": u["unit"], "pid": u["pid"], "param": u["param"], "kind": u["kind"], "iters": iters,
            "stack_pct": stack_pct, "stack_given": stack or o.get("stack", 0)}
    os.makedirs(BINS, exist_ok=True)
    binp = os.path.join(BINS, u["unit"] + ".bin")
    meta = os.path.join(BINS, u["unit"] + ".json")
    if r.returncode != 0:
        m = re.search(r"region `?(\w+)'? overflowed by (\d+) bytes", out)
        und = sorted(set(re.findall(r"undefined reference to `(\w+)'", out)))
        errs = [l.strip()[-260:] for l in out.splitlines()
                if re.search(r"error:|undefined reference|overflowed|will not fit|multiple definition", l)]
        if m:
            reason = "image does not fit in RAM (%s over by %d KB)" % (m.group(1), int(m.group(2)) // 1024 + 1)
        elif re.search(r"__u?int128", out):
            reason = "needs 128-bit integers (__int128), which the 32-bit RISC-V compiler does not have"
        elif re.search(r"immintrin|emmintrin|x86intrin", out):
            reason = "needs x86 intrinsics"
        elif und:
            reason = "undefined on the device: " + ", ".join(und[:10])
            ip = os.path.join(GEN, u["unit"], "info.json")
            rv = json.load(open(ip)).get("riscv_errors", {}) if os.path.exists(ip) else {}
            if any("int128" in v for v in rv.values()):
                reason = ("needs code that uses 128-bit integers (__int128), which the 32-bit RISC-V compiler does "
                          "not have (%s)" % ", ".join(sorted(os.path.basename(k) for k in rv)[:4]))
            elif rv:
                reason = "needs code that does not compile for RV32 (%s: %s)" % (
                    os.path.basename(sorted(rv)[0]), sorted(rv.values())[0].split("error:")[-1].strip()[:90])
        else:
            reason = errs[0] if errs else "build failed"
        save(u["unit"], False, pqc.parse_lines([]), dict(base, verdict="does not build", reason=reason,
                                                         build_errors=errs[:10]))
        for f in (binp, meta):
            if os.path.exists(f):
                os.remove(f)
        return "does not build: " + reason
    shutil.copy(os.path.join(ROOT, "build", name, "pqc_bench.bin"), binp)
    base["image_bytes"] = os.path.getsize(binp)
    rp = result_path(u["unit"])
    if os.path.exists(rp):
        old = json.load(open(rp))
        base["attempts"] = old.get("attempts", []) + [{k: old.get(k) for k in ("verdict", "reason", "stack_pct")}]
        os.remove(rp)
    json.dump(base, open(meta, "w"))
    return "built, %d KB" % (base["image_bytes"] // 1024)


def dev_capture(binp, timeout, idle):
    """Load one image into RAM and collect its output. The caller holds the
    device lock. Same procedure as tools/pqc.py."""
    port = pqc.find_port()
    r = subprocess.run([sys.executable, "-m", "esptool", "-p", port, "--no-stub", "load_ram", binp],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return [], "load_ram failed: " + (r.stdout + r.stderr).strip().splitlines()[-1][:160]
    ser = pqc.open_serial_no_reset(port)
    lines, raw, crash = [], b"", None
    t0 = last = time.time()
    started = done = False
    try:
        while not done:
            now = time.time()
            if now - t0 > timeout:
                crash = crash or "timeout after %d s" % timeout
                break
            if now - last > idle:
                crash = crash or "no output for %d s" % idle
                break
            chunk = ser.read(4096)
            if not chunk:
                continue
            last = now
            raw += chunk
            while b"\n" in raw:
                b, raw = raw.split(b"\n", 1)
                ln = b.decode("utf-8", "replace").rstrip("\r")
                if ln.startswith("@READY"):
                    if not started:
                        ser.write(b"GO \n")
                        ser.flush()
                    continue
                lines.append(ln)
                if ln.startswith("@START"):
                    started = True
                if pqc.PANIC.search(ln) and crash is None:
                    crash = ln.strip()
                    t0 = now - timeout + 2
                if ln.startswith("@DONE"):
                    done = True
    finally:
        ser.close()
    return lines, crash


def dev_finish(u, lines, crash, meta):
    parsed = pqc.parse_lines(lines)
    os.makedirs(os.path.join(RES, "logs"), exist_ok=True)
    with open(os.path.join(RES, "logs", u["unit"] + ".device.log"), "w") as f:
        f.write("\n".join(lines) + "\n")
    extra = dict(meta, kat=kat_verdict(u, parsed))
    bad = [k.split("/")[-1] for k, v in parsed["checks"].items() if v != "PASS"]
    failed = ["%s rc=%d" % (k.split("/")[-1], v["rc"]) for k, v in parsed["results"].items() if v["rc"] != 0]
    if crash:
        extra.update(verdict="crashes", reason=crash[:200])
    elif not parsed.get("complete"):
        extra.update(verdict="crashes", reason="run did not finish")
    elif bad or failed:
        why = "failed: " + ", ".join(sorted(set(bad + failed)))[:200]
        if any("rc=-100" in f for f in failed):
            why = "not enough free RAM for the job stack (%s)" % why
        extra.update(verdict="self-test fails", reason=why)
    else:
        extra.update(verdict="works", reason="")
    host = host_result(u["unit"])
    if host is not None:
        dd, hd = parsed["digests"], host.get("digests", {})
        same = [k for k in dd if hd.get(k) == dd[k]]
        diff = [k for k in dd if k in hd and hd[k] != dd[k]]
        extra["host_check"] = {"equal": len(same), "different": len(diff), "missing": len(dd) - len(same) - len(diff),
                               "different_labels": [d.split("/")[-1] for d in diff][:8]}
        if extra["verdict"] == "works" and (diff or not same):
            extra.update(verdict="differs from host", reason="fingerprints differ: " +
                         ", ".join(extra["host_check"]["different_labels"]))
    save(u["unit"], False, parsed, extra)
    return extra


def select_units(args):
    units = all_units(args.cand, getattr(args, "param", None))
    if args.level:
        keep = []
        for u in units:
            row = eval_match(u["pid"], u["param"], sizes_of(host_result(u["unit"])))
            if row and row["classical_security_level"] == str(args.level):
                keep.append(u)
        units = keep
    if getattr(args, "lowest", False):  # per candidate: the host-working unit with the lowest claimed level
        best = {}
        for u in units:
            host = host_result(u["unit"])
            if (host or {}).get("verdict") != "works":
                continue
            row = u.get("eval_row") or eval_match(u["pid"], u["param"], sizes_of(host))
            lvl = int(re.match(r"\d+", row["classical_security_level"]).group(0)) if row and re.match(
                r"\d+", row["classical_security_level"]) else 9999
            key = (lvl, pqc_natural(u["param"]))
            if u["pid"] not in best or key < best[u["pid"]][0]:
                best[u["pid"]] = (key, u)
        units = [u for _, u in best.values()]
    if getattr(args, "more", False):  # further sets of the candidates that already work on the device
        good = set()
        for u in units:
            rp = result_path(u["unit"])
            if os.path.exists(rp) and json.load(open(rp)).get("verdict") == "works":
                good.add(u["pid"])
        units = [u for u in units if u["pid"] in good and (host_result(u["unit"]) or {}).get("verdict") == "works"
                 and not os.path.exists(result_path(u["unit"]))]
    if args.first:  # the first unit of each candidate, preferring one that works on the host
        units.sort(key=lambda u: (u["pid"], (host_result(u["unit"]) or {}).get("verdict") != "works"))
        seen, keep = set(), []
        for u in units:
            if u["pid"] not in seen:
                seen.add(u["pid"])
                keep.append(u)
        units = keep
    return units


def sizes_of(host):
    if not host:
        return None
    for v in host.get("info", {}).values():
        m = dict(re.findall(r"(\w+)=(\w+)", v))
        if "kind" in m:
            return m
    return None


def retry_pct(unit):
    """After a failed device run: the next stack share to try, or None."""
    rp = result_path(unit)
    if not os.path.exists(rp):
        return None
    r = json.load(open(rp))
    tried = [a.get("stack_pct", 60) for a in r.get("attempts", [])] + [r.get("stack_pct", 60)]
    if r.get("verdict") == "works" or r.get("verdict") in ("does not build", "differs from host"):
        return None
    reason = r.get("reason", "")
    if re.search(r"no output|timeout|buffers|rc=99", reason):
        return None  # too slow or too large; another split does not help
    order = [85, 92] if re.search(r"(?i)stack", reason) else [30, 15, 85]
    for pct in order:
        if pct not in tried:
            return pct
    return None


def cmd_devbuild(args):
    units = [u for u in select_units(args)
             if (host_result(u["unit"]) or {}).get("verdict") == "works" or args.any_host]
    pcts = {}
    if args.retry:
        pcts = {u["unit"]: retry_pct(u["unit"]) for u in units}
        units = [u for u in units if pcts[u["unit"]]]
    if args.missing:
        units = [u for u in units if not os.path.exists(result_path(u["unit"]))
                 and not os.path.exists(os.path.join(BINS, u["unit"] + ".bin"))]
    log("%d units to build" % len(units))
    import queue
    workers = queue.Queue()
    for w in range(args.jobs):
        workers.put(str(w))

    def one(u):
        w = workers.get()
        try:
            v = dev_build(u, w, stack=args.stack, stack_pct=pcts.get(u["unit"]) or args.stack_pct,
                          iters=args.iters or None)
        except Exception as e:
            v = "tool error: %s" % e
        finally:
            workers.put(w)
        log("%-58s %s" % (u["unit"], v))
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        list(ex.map(one, units))


def cmd_devrun(args):
    """Run the images in build/ngcc_bins/, several per hold of the device lock."""
    units = [u for u in select_units(args) if os.path.exists(os.path.join(BINS, u["unit"] + ".bin"))]
    if args.missing:
        units = [u for u in units if not os.path.exists(result_path(u["unit"]))]
    if args.max_est:  # leave out units whose slowest operation would take too long on the chip
        units = [u for u in units if plan_iters(host_result(u["unit"]) or {})[1] <= args.max_est]
    log("%d images to run" % len(units))
    todo = list(units)
    while todo:
        with pqc.DeviceLock():
            t0 = time.time()
            while todo and time.time() - t0 < args.hold:
                u = todo.pop(0)
                meta = json.load(open(os.path.join(BINS, u["unit"] + ".json")))
                t1 = time.time()
                lines, crash = dev_capture(os.path.join(BINS, u["unit"] + ".bin"), args.timeout, args.idle)
                meta["seconds"] = round(time.time() - t1, 1)
                r = dev_finish(u, lines, crash, meta)
                hc = r.get("host_check", {})
                log("%-58s device: %-18s kat: %-8s host-equal: %s/%s  %ss %s" % (
                    u["unit"], r["verdict"], r["kat"].get("result", "-"), hc.get("equal", "-"),
                    hc.get("equal", 0) + hc.get("different", 0) + hc.get("missing", 0), meta["seconds"],
                    r.get("reason", "")[:110]))
        if todo:
            time.sleep(3)  # let the others in



def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("units"); p.add_argument("cand", nargs="?"); p.set_defaults(fn=cmd_units)
    p = sub.add_parser("gen"); p.add_argument("cand"); p.add_argument("param", nargs="?"); p.set_defaults(fn=cmd_gen)
    p = sub.add_parser("host"); p.add_argument("cand"); p.add_argument("param", nargs="?")
    p.add_argument("--timeout", type=int, default=300); p.set_defaults(fn=cmd_host)
    p = sub.add_parser("hostall"); p.add_argument("cand", nargs="?"); p.add_argument("-j", "--jobs", type=int, default=4)
    p.add_argument("--timeout", type=int, default=300); p.add_argument("--missing", action="store_true")
    p.add_argument("--failed", action="store_true"); p.set_defaults(fn=cmd_hostall)
    p = sub.add_parser("dev"); p.add_argument("cand"); p.add_argument("param", nargs="?")
    p.add_argument("--worker", default="0"); p.add_argument("--stack", type=int, default=0)
    p.add_argument("--iters", type=int, default=0); p.add_argument("--timeout", type=int, default=420)
    p.add_argument("--idle", type=int, default=240); p.set_defaults(fn=cmd_dev)

    def sel(p):
        p.add_argument("cand", nargs="?")
        p.add_argument("--level", type=int, help="only parameter sets of this claimed classical level")
        p.add_argument("--first", action="store_true", help="only the first selected unit of each candidate")
        p.add_argument("--more", action="store_true",
                       help="the other host-working units of candidates that already work on the device")
        p.add_argument("--lowest", action="store_true",
                       help="per candidate, the unit with the lowest claimed level that works on the host")
        p.add_argument("--missing", action="store_true", help="skip units that already have a device result")
    p = sub.add_parser("devbuild"); sel(p); p.add_argument("-j", "--jobs", type=int, default=3)
    p.add_argument("--stack", type=int, default=0); p.add_argument("--stack-pct", type=int, default=60)
    p.add_argument("--iters", type=int, default=0); p.add_argument("--any-host", action="store_true")
    p.add_argument("--retry", action="store_true",
                   help="rebuild units whose device run failed, with another split between stack and heap")
    p.set_defaults(fn=cmd_devbuild)
    p = sub.add_parser("devrun"); sel(p); p.add_argument("--hold", type=int, default=150,
                                                         help="seconds to keep the device before letting others in")
    p.add_argument("--timeout", type=int, default=150); p.add_argument("--idle", type=int, default=90)
    p.add_argument("--max-est", type=float, default=0,
                   help="skip units whose slowest operation is estimated above this many seconds on the chip")
    p.set_defaults(fn=cmd_devrun)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
