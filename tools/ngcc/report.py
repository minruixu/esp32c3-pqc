#!/usr/bin/env python3
"""Write the NGCC round-1 inventory and status tables.

    python3 tools/ngcc/report.py inventory   results/ngcc/inventory.json, docs/ngcc/inventory.md
    python3 tools/ngcc/report.py status      results/ngcc/status.json,    docs/ngcc/status.md

The inventory describes the submission packages as downloaded. The status
table says, for every candidate, how far it got on the ESP32-C3 and why.
Parameter-set names, claimed levels and sizes come from the independent
evaluation data of SYF19256/NGCC-Round1-Cryptographic-Algorithms-Evaluation
(CC BY 4.0), which this tool reads from downloads/ngcc/_eval.
"""
import collections
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ngcc  # noqa: E402

ROOT = ngcc.ROOT
DOCS = os.path.join(ROOT, "docs", "ngcc")

STD_HEADERS = set("""assert.h complex.h ctype.h errno.h fenv.h float.h inttypes.h iso646.h limits.h locale.h math.h
setjmp.h signal.h stdalign.h stdarg.h stdatomic.h stdbool.h stddef.h stdint.h stdio.h stdlib.h stdnoreturn.h string.h
tgmath.h time.h wchar.h strings.h memory.h malloc.h unistd.h sys/types.h sys/stat.h sys/time.h fcntl.h endian.h
byteswap.h alloca.h""".split())


# Headers that only the packages' test programs and platform shims include.
NOISE_HEADERS = set("direct.h io.h windows.h wincrypt.h bcrypt.h sys/syscall.h sys/random.h machine/endian.h "
                    "sys/endian.h sys/isa_defs.h gperftools/profiler.h sys/resource.h sys/utsname.h mach/mach_time.h "
                    "dlfcn.h libkern/OSByteOrder.h intrin.h".split())


def num(text):
    """Integer from a CSV field that may be empty or 'n/a'."""
    m = re.match(r"\s*(\d+)", text or "")
    return int(m.group(1)) if m else 0


def kb(n):
    return "%.1f" % (n / 1024.0)


def ms(us):
    if us is None:
        return ""
    if us < 1000:
        return "%d µs" % us
    if us < 10000:
        return "%.2f ms" % (us / 1e3)
    if us < 1e6:
        return "%.0f ms" % (us / 1e3)
    return "%.1f s" % (us / 1e6)


def classify_headers(headers):
    out = []
    rest = sorted(h for h in headers if h not in STD_HEADERS and h not in NOISE_HEADERS)
    if any(re.search(r"(immintrin|emmintrin|xmmintrin|smmintrin|tmmintrin|wmmintrin|x86intrin|avx\w*intrin)\.h", h)
           for h in rest):
        out.append("x86 intrinsics")
    if any(h.startswith("openssl/") for h in rest):
        out.append("OpenSSL")
    if any(h in ("gmp.h", "mpfr.h") for h in rest):
        out.append("GMP")
    if any(h in ("pthread.h", "omp.h") for h in rest):
        out.append("threads")
    if any(h == "arm_neon.h" for h in rest):
        out.append("NEON intrinsics")
    other = [h for h in rest if not re.search(r"intrin\.h|^openssl/|^gmp\.h|^mpfr\.h|^pthread\.h|^omp\.h|arm_neon", h)]
    if other:
        out.append("other headers: " + ", ".join(other[:6]))
    return out


def tree_headers(top):
    hs = set()
    for dp, dns, fns in os.walk(top):
        dns[:] = [d for d in dns if not ngcc.bad_component(d)]
        for fn in fns:
            if fn.endswith((".c", ".h")):
                hs |= set(re.findall(r"#\s*include\s*<([^>]+)>", ngcc.read_text(os.path.join(dp, fn), 400000)))
    return hs


def license_info(pkg):
    files, spdx, phrases = [], collections.Counter(), collections.Counter()
    for dp, dns, fns in os.walk(pkg):
        for fn in fns:
            if re.match(r"(?i)^(licen[sc]e|copying|notice)([._-].*)?$", fn):
                files.append(os.path.relpath(os.path.join(dp, fn), pkg))
            elif fn.endswith((".c", ".h")) and len(spdx) + len(phrases) < 4000:
                t = ngcc.read_text(os.path.join(dp, fn), 6000)
                for m in re.findall(r"SPDX-License-Identifier:\s*([^\n*]+)", t):
                    spdx[m.strip()[:40]] += 1
                for name, pat in (("public domain", r"public domain"), ("CC0", r"\bCC0\b"),
                                  ("MIT", r"MIT License|Permission is hereby granted, free of charge"),
                                  ("Apache", r"Apache License"), ("GPL", r"GNU (Lesser )?General Public")):
                    if re.search(pat, t, re.I):
                        phrases[name] += 1
    parts = []
    if files:
        parts.append("license files: " + ", ".join(sorted(files, key=len)[:3]) + (" ..." if len(files) > 3 else ""))
    if spdx:
        parts.append("SPDX tags in sources: " + ", ".join("%s (%d files)" % kv for kv in spdx.most_common(3)))
    if phrases:
        parts.append("notices in sources: " + ", ".join("%s (%d files)" % kv for kv in phrases.most_common(4)))
    return "; ".join(parts) if parts else "none found in the code; the package has intellectual-property statements as PDF"


def level_units(pid, units):
    """Units of a candidate with their evaluation row, lowest level first."""
    out = []
    for u in units:
        host = ngcc.host_result(u["unit"])
        row = ngcc.eval_match(pid, u["param"], ngcc.sizes_of(host))
        out.append((num(row["classical_security_level"]) or 9999 if row else 9999, u, row))
    out.sort(key=lambda t: (t[0], ngcc.pqc_natural(t[1]["param"])))
    return out


def cmd_inventory():
    inv = []
    for p in ngcc.manifest():
        pid = p["id"]
        pkg = os.path.join(ngcc.SRC, pid)
        rows = ngcc.eval_rows(pid)
        units = ngcc.discover(pid) if os.path.exists(os.path.join(pkg, ".unpacked")) else []
        e = {"id": pid, "name": p["name"], "type": p["type"], "family": rows[0]["family"] if rows else "",
             "package_url": p.get("url"), "package_bytes": p.get("size"), "package_sha256": p.get("sha256"),
             "ported_elsewhere": ngcc.ELSEWHERE.get(pid)}
        sets = []
        for r in rows:
            s = {"name": r["param_set"], "level": num(r["classical_security_level"]), "pk": num(r.get("pk_bytes")),
                 "sk": num(r.get("sk_bytes"))}
            if p["type"] == "SIG":
                s["sig"] = num(r.get("sn_bytes"))
            elif p["type"] == "KEM":
                s["ct"] = num(r.get("ct_bytes"))
            else:
                s["messages"] = num(r.get("total_msg_bytes"))
            sets.append(s)
        e["parameter_sets"] = sets
        e["units_found"] = [u["param"] for u in units]
        if units:
            u0 = units[0]
            e["reference_dir"] = os.path.relpath(u0["ref_root"], pkg)
            e["api"] = "ICCS uniform interface (%s_* functions) in %s" % (pid.split("_")[0],
                                                                         os.path.basename(u0["inst"]))
            infos = [json.load(open(f)) for f in
                     [os.path.join(ngcc.GEN, u["unit"], "info.json") for u in units] if os.path.exists(f)]
            if infos:  # what the selected sources include, plus what made files unusable
                i0 = infos[0]
                deps = classify_headers(set(i0.get("system_headers", [])))
                x86 = sum(1 for v in i0.get("dropped", {}).values() if re.search(r"intrin\.h|constraint .* in asm", v))
                if x86 and "x86 intrinsics" not in deps:
                    deps.insert(0, "x86 intrinsics or assembly in %d source files" % x86)
                if any("__int128" in v or "__uint128" in v for v in i0.get("riscv_errors", {}).values()):
                    deps.append("128-bit integers (__int128)")
                e["dependencies"] = deps
            else:
                e["dependencies"] = classify_headers(tree_headers(u0["ref_root"]) - NOISE_HEADERS)
            # auxfunc.c and drng.c are ICCS's own files and use malloc everywhere
            e["uses_malloc"] = any(n not in ("auxfunc.c", "drng.c") for i in infos
                                   for n in i.get("uses_malloc", [])) if infos else None
        else:
            nested = [os.path.relpath(f, pkg) for f in glob.glob(os.path.join(pkg, "**", "*"), recursive=True)
                      if f.lower().endswith((".rar", ".7z", ".tar.gz", ".tgz"))
                      and re.search(r"(?i)implement", f)]
            e["reference_dir"] = None
            e["api"] = "no implementation of the uniform interface found" + \
                (" (code is inside %s)" % nested[0] if nested else "")
            e["dependencies"], e["uses_malloc"] = [], None
        e["license"] = license_info(pkg) if os.path.isdir(pkg) else "package not unpacked"
        low = min(sets, key=lambda s: s["level"]) if sets else None
        if low:
            need = low["pk"] + low["sk"] + low.get("ct", low.get("sig", low.get("messages", 0)))
            e["ram_estimate"] = ("no: keys and messages of the smallest set alone need %s KB" % kb(need)) \
                if need > 300 * 1024 else ("doubtful: keys and messages of the smallest set need %s KB" % kb(need)
                                           if need > 100 * 1024 else "plausible from the sizes (%s KB of keys and "
                                           "messages for the smallest set)" % kb(need))
        else:
            e["ram_estimate"] = "unknown"
        inv.append(e)
    os.makedirs(ngcc.RES, exist_ok=True)
    with open(os.path.join(ngcc.RES, "inventory.json"), "w") as f:
        json.dump({"source": "https://www.niccs.org.cn/niccs/Proposal/pc/list.html", "candidates": inv}, f,
                  indent=1, ensure_ascii=False)
        f.write("\n")
    os.makedirs(DOCS, exist_ok=True)
    out = ["# NGCC round-1 public-key candidates: inventory of the submission packages", "",
           "Packages as published by the Institute of Commercial Cryptography Standards at",
           "<https://www.niccs.org.cn/niccs/Proposal/pc/list.html> and downloaded by `tools/ngcc/fetch.py`.",
           "URL, size and SHA-256 of every package are in `results/ngcc/packages.json`; the packages",
           "themselves are not part of this repository.", "",
           "Parameter-set names, claimed classical security levels and sizes are taken from the independent",
           "evaluation data in <https://github.com/SYF19256/NGCC-Round1-Cryptographic-Algorithms-Evaluation>",
           "(CC BY 4.0). Everything else in the table was read from the packages by `tools/ngcc/report.py`.", "",
           "Columns: *smallest set* is the parameter set with the lowest claimed level (public key / secret key /",
           "ciphertext, signature or total message bytes). *RAM* is a first estimate from those sizes; what",
           "really happened on the chip is in [status.md](status.md).", ""]
    for kind, title in (("KEM", "Key encapsulation"), ("SIG", "Digital signatures"), ("KEX", "Key exchange")):
        es = [e for e in inv if e["type"] == kind]
        out += ["## %s (%d)" % (title, len(es)), "",
                "| Candidate | Family | Sets | Smallest set | Reference code | Dependencies | malloc | License notes | RAM |",
                "|---|---|---:|---|---|---|---|---|---|"]
        for e in es:
            low = min(e["parameter_sets"], key=lambda s: s["level"]) if e["parameter_sets"] else None
            small = "%s (%d): %d / %d / %d" % (low["name"], low["level"], low["pk"], low["sk"],
                                               low.get("ct", low.get("sig", low.get("messages", 0)))) if low else ""
            ref = ("`%s`, %d sets found" % (e["reference_dir"], len(e["units_found"]))) if e["reference_dir"] \
                else e["api"]
            mal = {True: "yes", False: "no", None: "?"}[e["uses_malloc"]]
            name = e["name"] + (" (ported from %s instead)" % e["ported_elsewhere"] if e["ported_elsewhere"] else "")
            out.append("| %s | %s | %d | %s | %s | %s | %s | %s | %s |" % (
                name, e["family"], len(e["parameter_sets"]), small, ref,
                "; ".join(e["dependencies"]) or "none beyond the C library", mal, e["license"],
                e["ram_estimate"]))
        out.append("")
    out += ["All candidates with a reference directory implement the uniform interface that ICCS prescribes for",
            "its known-answer-test programs (`kem_*`, `sig_*` or `kex_*` functions and the ICCS deterministic",
            "random number generator). The generic suite `suites/ngcc` drives them through that interface.", ""]
    with open(os.path.join(DOCS, "inventory.md"), "w") as f:
        f.write("\n".join(out))
    print("wrote results/ngcc/inventory.json and docs/ngcc/inventory.md (%d candidates)" % len(inv))


OPS = {"KEM": ("keygen", "encaps", "decaps"), "SIG": ("keygen", "sign", "verify"), "KEX": ("init_a", "init_b", "full")}


def unit_status(u):
    host = ngcc.host_result(u["unit"])
    dp = ngcc.result_path(u["unit"])
    dev = json.load(open(dp)) if os.path.exists(dp) else None
    st = {"unit": u["unit"], "param": u["param"], "host": host.get("verdict") if host else "not attempted",
          "host_reason": (host or {}).get("reason", ""), "host_kat": ((host or {}).get("kat") or {}).get("result"),
          "device": dev.get("verdict") if dev else "not attempted", "device_reason": (dev or {}).get("reason", "")}
    if dev:
        st["device_kat"] = (dev.get("kat") or {}).get("result")
        st["host_check"] = dev.get("host_check")
        st["image_bytes"] = dev.get("image_bytes")
        res = {k.split("/")[-1]: v for k, v in dev.get("results", {}).items() if "/ref/" in k}
        opt = {k.split("/")[-1]: v for k, v in dev.get("results", {}).items() if "/opt/" in k}
        st["ops"] = {op: res[op]["avg_us"] for op in OPS[u["kind"]] if op in res and res[op]["rc"] == 0}
        st["ops_opt"] = {op: opt[op]["avg_us"] for op in OPS[u["kind"]] if op in opt and opt[op]["rc"] == 0}
        ratios = [st["ops"][op] / st["ops_opt"][op] for op in st["ops_opt"] if op in st["ops"] and st["ops_opt"][op]]
        if ratios:
            prod = 1.0
            for r in ratios:
                prod *= r
            st["opt_speedup"] = round(prod ** (1.0 / len(ratios)), 2)
        okr = [v for v in res.values() if v["rc"] == 0]
        st["stack"] = max([v["stack"] for v in okr] or [0])
        st["heap"] = max([v["heap"] for v in okr] or [0])
        st["sizes"] = ngcc.sizes_of(dev)
    elif host:
        st["sizes"] = ngcc.sizes_of(host)
    return st


def explain_host(unit, reason):
    """A readable reason for a host build failure, from what the generator
    recorded about the unit's source files."""
    ip = os.path.join(ngcc.GEN, unit, "info.json")
    if not os.path.exists(ip):
        return reason
    cats = collections.Counter()
    for v in json.load(open(ip)).get("dropped", {}).values():
        if "main()" in v or "helper" in v or "randombytes" in v or "description file" in v:
            continue
        m = re.search(r"'([^']+)' file not found", v)
        if re.search(r"intrin\.h", v):
            cats["need x86 intrinsics"] += 1
        elif "constraint" in v and "asm" in v:
            cats["use x86 inline assembly"] += 1
        elif m:
            cats["include %s, which is not in the reference tree" % m.group(1)] += 1
        elif "__declspec" in v:
            cats["need a Windows build of GMP"] += 1
        else:
            cats["do not compile on the host (%s)" % v.split("error:")[-1].strip()[:70]] += 1
    parts = ["%d source files %s" % (n, c) for c, n in cats.most_common(2)]
    short = reason if len(reason) < 110 else reason[:107] + "..."
    return "; ".join(parts + ([short] if short else []))


def tidy(reason):
    """Device failure reasons in plain words."""
    if "Stack protection fault" in reason or "stack overflow" in reason or re.search(r"(Load|Store) access fault", reason):
        return "does not fit in RAM: stack overflow or memory fault with every stack/heap split that was tried"
    if "failed: buffers" in reason:
        return "keys or signatures are too large to allocate on the chip"
    m = re.search(r"no output for (\d+) s", reason)
    if m:
        return "no output for %s s on the chip: hangs or is far too slow" % m.group(1)
    if reason.startswith("fingerprints differ"):
        return "runs on the chip, but its outputs differ from the host run (%s)" % reason.split(": ", 1)[-1]
    if "rc=99" in reason:
        return "runs on the chip, but decapsulation returns a different shared secret than encapsulation"
    if reason.startswith("failed: "):
        return "operations return an error on the chip (%s)" % reason[8:]
    return reason


def outcome(st):
    if st["device"] == "works":
        return "works on device"
    if st["device"] in ("crashes", "self-test fails", "differs from host", "does not build"):
        return "host only" if st["host"] == "works" else "does not work"
    if st["host"] == "works":
        return "host only (device not run)"
    if st["host"] in ("does not build", "crashes", "self-test fails", "not run"):
        return "does not build" if st["host"] == "does not build" else "does not work"
    return "not attempted"


def cmd_status():
    table = []
    for p in ngcc.manifest():
        pid = p["id"]
        rows = ngcc.eval_rows(pid)
        e = {"id": pid, "name": p["name"], "type": p["type"], "family": rows[0]["family"] if rows else ""}
        if pid in ngcc.ELSEWHERE:
            e.update(outcome="not attempted here", reason="ported from %s by another suite of this project"
                     % ngcc.ELSEWHERE[pid], units=[])
            table.append(e)
            continue
        units = ngcc.discover(pid)
        if not units:
            nested = [os.path.basename(f) for f in glob.glob(os.path.join(ngcc.SRC, pid, "**", "*.rar"), recursive=True)]
            e.update(outcome="not attempted", units=[],
                     reason=("reference code is packed as RAR inside the package (%s), which this tool does not unpack"
                             % nested[0]) if nested else "no implementation of the uniform interface found")
            table.append(e)
            continue
        lu = level_units(pid, units)
        sts = []
        for lvl, u, row in lu:
            st = unit_status(u)
            st["level"] = lvl if lvl != 9999 else None
            st["set"] = row["param_set"] if row else u["param"]
            sts.append(st)
        e["units"] = sts
        # The headline row: the lowest-level unit that got furthest.
        order = ["works on device", "host only", "host only (device not run)", "does not work", "does not build",
                 "not attempted"]
        low = min(s["level"] or 9999 for s in sts)
        cands = [s for s in sts if (s["level"] or 9999) == low] or sts
        head = min(cands, key=lambda s: order.index(outcome(s)))
        if outcome(head) != "works on device":
            better = [s for s in sts if outcome(s) == "works on device"]
            if better:
                head = better[0]
        e["headline"] = head["unit"]
        e["outcome"] = outcome(head)
        e["reason"] = tidy(head["device_reason"]) if head["device"] not in ("works", "not attempted") else \
            (explain_host(head["unit"], head["host_reason"]) if head["host"] != "works" else "")
        table.append(e)
    with open(os.path.join(ngcc.RES, "status.json"), "w") as f:
        json.dump({"candidates": table}, f, indent=1, ensure_ascii=False)
        f.write("\n")

    def head_of(e):
        return next(s for s in e["units"] if s["unit"] == e["headline"])

    counts = collections.Counter(e["outcome"] for e in table)
    out = ["# NGCC round-1 public-key candidates on the ESP32-C3: status", "",
           "One row per candidate, for its parameter set with the lowest claimed classical security level",
           "(usually 128 bits; another set is shown when only that one ran). The reference implementations",
           "are built unmodified from the official submission packages (see [inventory.md](inventory.md)) and",
           "driven through the ICCS uniform interface by `suites/ngcc`. No candidate was optimized individually.", "",
           "What the columns mean:", "",
           "- **Outcome**: *works on device* = keygen, encapsulation/signing, decapsulation/verification and the",
           "  tamper test pass on the ESP32-C3 from RAM. *host only* = the same test passes on the build machine but",
           "  not on the chip (reason given). *does not build* / *does not work* = the generic port failed already on",
           "  the build machine (reason given).",
           "- **KAT**: the run seeded like test vector 0 of the ICCS KAT program reproduces the first vector of the",
           "  package's own KAT file (*match*), does not (*differs*), or the package has no usable file.",
           "- **= host**: output fingerprints of the device run that equal those of the host run of the same code.",
           "- Times are averages on the ESP32-C3 at 160 MHz (3 runs, or 1 run for slow operations), with the",
           "  harness random generator; stack and heap are the peaks of one operation; image is the RAM image size.",
           "- **opt**: most candidates hash with the SM3-based helper functions that ICCS supplies. For those, a",
           "  second run sends the SM3 compression function to this project's optimized backend",
           "  (`components/prims`); the column is the speed-up over the unmodified package (geometric mean of the",
           "  three operations). The outputs of both runs are identical.",
           "", "Summary: " + ", ".join("%d %s" % (n, k) for k, n in counts.most_common()) + ".", ""]
    for kind, title in (("KEM", "Key encapsulation"), ("SIG", "Digital signatures"), ("KEX", "Key exchange")):
        ops = OPS[kind]
        out += ["## %s" % title, "",
                "| Candidate | Family | Set (level) | Outcome | KAT | = host | %s | %s | %s | opt | Stack KB | Heap KB | "
                "Image KB | Note |" % ops, "|---|---|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|---|"]
        for e in [x for x in table if x["type"] == kind]:
            if not e.get("units"):
                out.append("| %s | %s | | %s | | | | | | | | | | %s |" % (e["name"], e["family"], e["outcome"],
                                                                        e["reason"]))
                continue
            h = head_of(e)
            hc = h.get("host_check") or {}
            eq = "%d/%d" % (hc.get("equal", 0), hc.get("equal", 0) + hc.get("different", 0) + hc.get("missing", 0)) \
                if hc else ""
            o = h.get("ops", {})
            dev_ok = h["device"] == "works"
            note = e["reason"].replace("|", "/")[:230]
            out.append("| %s | %s | %s (%s) | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
                e["name"], e["family"], h["set"], h["level"] or "?", e["outcome"],
                h.get("device_kat") or h.get("host_kat") or "", eq,
                ms(o.get(ops[0])), ms(o.get(ops[1])), ms(o.get(ops[2])),
                ("%.2f×" % h["opt_speedup"]) if h.get("opt_speedup") else "",
                kb(h["stack"]) if dev_ok else "", kb(h["heap"]) if dev_ok else "",
                kb(h["image_bytes"]) if h.get("image_bytes") else "", note))
        out.append("")
    # Further parameter sets that ran on the device.
    extra = []
    for e in table:
        for s in e.get("units", []):
            if s["unit"] != e.get("headline") and s["device"] == "works":
                o = s.get("ops", {})
                ops = OPS[e["type"]]
                extra.append("| %s | %s (%s) | %s | %s | %s | %s | %s | %s | %s |" % (
                    e["name"], s["set"], s["level"] or "?", s.get("device_kat") or "",
                    ms(o.get(ops[0])), ms(o.get(ops[1])), ms(o.get(ops[2])), kb(s["stack"]), kb(s["heap"]),
                    kb(s["image_bytes"]) if s.get("image_bytes") else ""))
    if extra:
        out += ["## Further parameter sets that ran on the device", "",
                "Operations are keygen / encaps or sign / decaps or verify (init_a / init_b / full exchange for key "
                "exchange).", "",
                "| Candidate | Set (level) | KAT | Op 1 | Op 2 | Op 3 | Stack KB | Heap KB | Image KB |",
                "|---|---|---|---:|---:|---:|---:|---:|---:|"] + extra + [""]
    with open(os.path.join(DOCS, "status.md"), "w") as f:
        f.write("\n".join(out))
    print("wrote results/ngcc/status.json and docs/ngcc/status.md: " + ", ".join("%d %s" % (n, k) for k, n in
                                                                                 counts.most_common()))


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "inventory":
        cmd_inventory()
    elif cmd == "status":
        cmd_status()
    else:
        sys.exit(__doc__)
