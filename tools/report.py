#!/usr/bin/env python3
"""Turn results/*.json into docs/RESULTS.md (tables of time, stack and heap).

    python3 tools/report.py            write docs/RESULTS.md
    python3 tools/report.py --stdout   print instead

Job names are <algorithm>/<variant>/<operation>. For each suite the table has
one row per algorithm and operation and one group of columns per variant.
"""
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTS = os.path.join(ROOT, "results")


def fmt_time(us):
    if us is None:
        return ""
    if us < 1000:
        return "%d µs" % us
    ms = us / 1000.0
    if ms < 10:
        return "%.2f ms" % ms
    if ms < 100:
        return "%.1f ms" % ms
    if ms < 10000:
        return "%.0f ms" % ms
    return "%.1f s" % (ms / 1000.0)


def fmt_kb(n):
    if n is None:
        return ""
    if n == 0:
        return "0"
    return "%.1f KB" % (n / 1024.0)


OP_ORDER = ["kat", "keygen", "encaps", "decaps", "sign", "verify"]


def natural(text):
    """Sort key that orders mlkem512 < mlkem768 < mlkem1024."""
    import re
    return [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", text)]


def split(name):
    parts = name.split("/")
    if len(parts) >= 3:
        return parts[0], parts[1], "/".join(parts[2:])
    return name, "", ""


def suite_section(name, dev, host):
    out = ["## %s" % name, ""]
    rows = dev.get("results", {})
    if not rows:
        return out + ["No results.", ""]
    algs, variants, ops = [], [], {}
    table = {}
    for job, r in rows.items():
        a, v, o = split(job)
        if a not in algs:
            algs.append(a)
            ops[a] = []
        if v not in variants:
            variants.append(v)
        if o not in ops[a]:
            ops[a].append(o)
        table[(a, v, o)] = r
    variants.sort(key=lambda v: {"ref": 0, "opt": 1}.get(v, 2))
    algs.sort(key=natural)
    for a in algs:
        ops[a].sort(key=lambda o: (OP_ORDER.index(o) if o in OP_ORDER else len(OP_ORDER), o))
    has_heap = any(r.get("heap") for r in rows.values())

    head = ["Algorithm", "Operation"] + ["%s time" % v for v in variants]
    if "ref" in variants and "opt" in variants:
        head.append("speedup")
    head += ["%s stack" % v for v in variants]
    if has_heap:
        head += ["%s heap" % v for v in variants]
    out.append("| " + " | ".join(head) + " |")
    out.append("|" + "|".join(["---", "---"] + ["---:"] * (len(head) - 2)) + "|")
    for a in algs:
        for o in ops[a]:
            cells = [a, o]
            rs = [table.get((a, v, o)) for v in variants]
            for r in rs:
                if r is None:
                    cells.append("")
                elif r["rc"] != 0:
                    cells.append("FAIL (%d)" % r["rc"])
                else:
                    cells.append(fmt_time(r["avg_us"]))
            if "ref" in variants and "opt" in variants:
                r0, r1 = table.get((a, "ref", o)), table.get((a, "opt", o))
                ok = r0 and r1 and r0["rc"] == 0 and r1["rc"] == 0 and r1["avg_us"] > 0
                cells.append("%.2f×" % (r0["avg_us"] / r1["avg_us"]) if ok else "")
            cells += [fmt_kb(r["stack"]) if r else "" for r in rs]
            if has_heap:
                cells += [fmt_kb(r["heap"]) if r else "" for r in rs]
            out.append("| " + " | ".join(cells) + " |")
    out.append("")

    checks = dev.get("checks", {})
    bad = [k for k, v in checks.items() if v != "PASS"]
    line = "Self-tests on the device: %d passed, %d failed" % (len(checks) - len(bad), len(bad))
    if bad:
        line += " (" + ", ".join(sorted(bad)) + ")"
    out.append(line + ".")
    if host is not None:
        dd, hd = dev.get("digests", {}), host.get("digests", {})
        same = sum(1 for k, v in dd.items() if hd.get(k) == v)
        diff = sum(1 for k, v in dd.items() if k in hd and hd[k] != v)
        miss = sum(1 for k in dd if k not in hd)
        out.append("Output fingerprints, device against host build: %d equal, %d different, %d without a host value."
                   % (same, diff, miss))
    else:
        out.append("No host run recorded for this suite.")
    if not dev.get("complete", False):
        out.append("**The last device run of this suite did not finish.**")
    info = dev.get("info", {})
    if info:
        out += ["", "<details><summary>Parameters and sizes</summary>", ""]
        out += ["- `%s`: %s" % (k, v) for k, v in info.items()]
        out += ["", "</details>"]
    out.append("")
    return out


def main():
    files = sorted(f for f in glob.glob(os.path.join(RESULTS, "*.json")) if not f.endswith(".host.json"))
    out = ["# Results", "",
           "Measured on an ESP32-C3 (RV32IMC, 160 MHz) with every suite loaded into RAM.",
           "`ref` is the upstream portable code; `opt` adds this project's ESP32-C3 optimizations.",
           "Times are averages over the runs of a job with a fixed random seed; stack and heap are peaks.",
           "Generated by `tools/report.py` from `results/*.json`.", ""]
    for f in files:
        name = os.path.basename(f)[:-5]
        dev = json.load(open(f))
        hp = os.path.join(RESULTS, name + ".host.json")
        host = json.load(open(hp)) if os.path.exists(hp) else None
        out += suite_section(name, dev, host)
    text = "\n".join(out) + "\n"
    if "--stdout" in sys.argv:
        sys.stdout.write(text)
    else:
        with open(os.path.join(ROOT, "docs", "RESULTS.md"), "w") as fh:
            fh.write(text)
        print("wrote docs/RESULTS.md (%d suites)" % len(files))


if __name__ == "__main__":
    main()
