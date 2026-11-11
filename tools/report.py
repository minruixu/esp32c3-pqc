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


# ---------------------------------------------------------------------------
# One row per algorithm, for the README and docs/SUMMARY.md.

SECTIONS = [
    ("NIST standards and selections", [
        ("ML-KEM (FIPS 203)", ["mlkem"]),
        ("ML-DSA (FIPS 204)", ["mldsa", "mldsa87"]),
        ("ML-DSA, low-RAM build of mldsa-native", ["mldsa_lowram"]),
        ("SLH-DSA (FIPS 205)", ["slhdsa"]),
        ("FN-DSA / Falcon (FIPS 206 draft)", ["fndsa"]),
        ("HQC (selected 2025, reference v5.1.0)", ["hqc", "hqc3ref"]),
        ("LMS and HSS (SP 800-208)", ["lms"]),
        ("XMSS and XMSS^MT (SP 800-208)", ["xmss"]),
    ]),
    ("Chinese algorithms and SM3 instantiations", [
        ("Aigis-enc (PQMagic)", ["aigis_enc"]),
        ("Aigis-sig (PQMagic)", ["aigis_sig"]),
        ("Aigis-sig, low-stack build", ["aigis_sig_lowram"]),
        ("ML-KEM with SM3 (PQMagic)", ["pqmagic_mlkem"]),
        ("ML-DSA with SM3 (PQMagic)", ["pqmagic_mldsa", "pqmagic_mldsa87"]),
        ("SLH-DSA with SM3 (PQMagic)", ["pqmagic_slhdsa_sm3"]),
        ("SPHINCS-alpha (PQMagic)", ["pqmagic_sphincsa", "pqmagic_sphincsa_192f"]),
        ("GmSSL: Kyber and SM3-based hash signatures", ["gmssl_kyber", "gmssl_lms", "gmssl_xmss", "gmssl_sphincs"]),
        ("LAC", ["lac"]),
    ]),
    ("NGCC round-1 candidates with open implementations", [
        ("Aigis-Enc+ (PQMagic)", ["pqmagic_aigis_enc_plus"]),
        ("Aigis-Sig+ (PQMagic)", ["pqmagic_aigis_sig_plus"]),
        ("NEV (PQMagic)", ["pqmagic_nev", "pqmagic_nev_sm3"]),
        ("NEV-AKE (PQMagic)", ["pqmagic_nev_ake"]),
        ("SCloud+", ["scloudplus", "scloudplus_256", "scloudplus_256_sm3"]),
    ]),
]
COLS = [("Key generation", ["keygen"]),
        ("Encapsulation / signing", ["encaps", "sign", "init"]),
        ("Decapsulation / verification", ["decaps", "verify", "response"])]


def cell(table, alg, ops):
    for o in ops:
        ref, opt = table.get((alg, "ref", o)), table.get((alg, "opt", o))
        ref = ref if ref and ref["rc"] == 0 else None
        opt = opt if opt and opt["rc"] == 0 else None
        if ref and opt:
            x = ref["avg_us"] / opt["avg_us"] if opt["avg_us"] else 0
            if 0.95 < x < 1.05:
                return fmt_time(opt["avg_us"])
            return "%s → %s (%.1f×)" % (fmt_time(ref["avg_us"]), fmt_time(opt["avg_us"]), x)
        only = opt or ref
        if only:
            return fmt_time(only["avg_us"]) + ("" if opt else " (ref only)")
        other = [r for (a, v, oo), r in table.items() if a == alg and oo == o and r["rc"] == 0]
        if other:
            return fmt_time(other[0]["avg_us"])
    return ""


def summary():
    out = []
    for title, groups in SECTIONS:
        out += ["### %s" % title, ""]
        for gtitle, suites in groups:
            table, algs = {}, []
            for s in suites:
                path = os.path.join(RESULTS, s + ".json")
                if not os.path.exists(path):
                    continue
                for job, r in json.load(open(path)).get("results", {}).items():
                    a, v, o = split(job)
                    table.setdefault((a, v, o), r)
                    if a not in algs:
                        algs.append(a)
            main_ops = [o for _c, ops in COLS for o in ops]
            # test-vector jobs ("kat-...") are in RESULTS.md, not in the per-algorithm summary
            algs = [a for a in algs if not a.startswith("kat-")
                    and any((a2 == a and o in main_ops) for (a2, _v, o) in table)]
            if not algs:
                continue
            algs.sort(key=natural)
            out += ["**%s**" % gtitle, "",
                    "| Algorithm | " + " | ".join(c for c, _ in COLS) + " | Peak stack + heap |",
                    "|---|---:|---:|---:|---:|"]
            for a in algs:
                cells = [cell(table, a, ops) for _c, ops in COLS]
                pref = [r for (a2, v, o), r in table.items() if a2 == a and o in main_ops and r["rc"] == 0
                        and v in ("opt", "lowram")]
                anyv = pref or [r for (a2, v, o), r in table.items() if a2 == a and o in main_ops and r["rc"] == 0]
                stack = max([r["stack"] + r.get("heap", 0) for r in anyv] or [0])
                out.append("| %s | %s | %s | %s | %s |" % (a, cells[0], cells[1], cells[2], fmt_kb(stack)))
            out.append("")
    return out


# ---------------------------------------------------------------------------
# The table in the READMEs: one representative parameter set per family.

HIGHLIGHTS = [
    ("ML-KEM-768", ["mlkem"], "mlkem768"),
    ("ML-DSA-65", ["mldsa"], "mldsa65"),
    ("SLH-DSA-SHA2-128f", ["slhdsa"], "slhdsa-sha2-128f"),
    ("SLH-DSA-SHA2-128s", ["slhdsa"], "slhdsa-sha2-128s"),
    ("FN-DSA-512 (Falcon)", ["fndsa"], "fndsa512"),
    ("HQC-1", ["hqc"], "hqc1"),
    ("LMS-SHA256 H10/W4", ["lms"], "lms-sha256-m32-h10-w4"),
    ("XMSS-SHA2_10_256", ["xmss"], "xmss-sha2_10_256"),
    ("Aigis-enc-2 (SM3)", ["aigis_enc"], "aigis-enc-2-sm3"),
    ("Aigis-sig-2 (SM3)", ["aigis_sig"], "aigis-sig-2-sm3"),
    ("ML-KEM-768 with SM3", ["pqmagic_mlkem"], "mlkem768-sm3-pqmagic"),
    ("ML-DSA-65 with SM3", ["pqmagic_mldsa"], "mldsa65-sm3-pqmagic"),
    ("LAC-128", ["lac"], "lac-128"),
    ("SCloud+-128 (AES)", ["scloudplus"], "scloudplus-128-aes"),
    ("NEV-512", ["pqmagic_nev"], "nev-512-769-c"),
    ("Aigis-Enc+-512", ["pqmagic_aigis_enc_plus"], "aigis-enc-plus-512"),
]
README_HEAD = {
    "README.md": ["Algorithm", "Key generation", "Encapsulation / signing", "Decapsulation / verification",
                  "Stack + heap"],
    "README.zh-CN.md": ["算法", "密钥生成", "封装 / 签名", "解封装 / 验签", "栈 + 堆"],
}


def readme_rows():
    rows = []
    main_ops = [o for _c, ops in COLS for o in ops]
    for label, suites, alg in HIGHLIGHTS:
        table = {}
        for s in suites:
            path = os.path.join(RESULTS, s + ".json")
            if os.path.exists(path):
                for job, r in json.load(open(path)).get("results", {}).items():
                    table.setdefault(split(job), r)
        if not any(a == alg for (a, _v, _o) in table):
            continue
        cells = [cell(table, alg, ops) for _c, ops in COLS]
        pref = [r for (a, v, o), r in table.items() if a == alg and o in main_ops and r["rc"] == 0 and v == "opt"]
        anyv = pref or [r for (a, v, o), r in table.items() if a == alg and o in main_ops and r["rc"] == 0]
        ram = max([r["stack"] + r.get("heap", 0) for r in anyv] or [0])
        rows.append([label] + cells + [fmt_kb(ram)])
    return rows


def fill_readmes():
    rows = readme_rows()
    for name, head in README_HEAD.items():
        path = os.path.join(ROOT, name)
        if not os.path.exists(path):
            continue
        text = open(path).read()
        a, b = "<!-- results:begin -->", "<!-- results:end -->"
        if a not in text or b not in text:
            continue
        block = ["| " + " | ".join(head) + " |", "|---|---:|---:|---:|---:|"]
        block += ["| " + " | ".join(r) + " |" for r in rows]
        text = text[:text.index(a) + len(a)] + "\n" + "\n".join(block) + "\n" + text[text.index(b):]
        with open(path, "w") as fh:
            fh.write(text)
    return len(rows)


def main():
    files = sorted(f for f in glob.glob(os.path.join(RESULTS, "*.json")) if not f.endswith(".host.json")
                   and os.path.isdir(os.path.join(ROOT, "suites", os.path.basename(f)[:-5])))
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
    stext = "\n".join(["# Summary", "",
                       "One row per algorithm: `reference → optimized (speedup)`, or a single time where the two",
                       "do not differ. ESP32-C3 at 160 MHz, images loaded into RAM. The last column is the largest",
                       "stack plus heap any of the three operations needs in the optimized variant; keys, ciphertexts",
                       "and signatures held by the caller come on top. Full tables: [RESULTS.md](RESULTS.md).", ""] + summary()) + "\n"
    if "--summary" in sys.argv:
        sys.stdout.write(stext)
    elif "--stdout" in sys.argv:
        sys.stdout.write(text)
    else:
        with open(os.path.join(ROOT, "docs", "SUMMARY.md"), "w") as fh:
            fh.write(stext)
        print("README tables: %d rows" % fill_readmes())
        with open(os.path.join(ROOT, "docs", "RESULTS.md"), "w") as fh:
            fh.write(text)
        print("wrote docs/RESULTS.md (%d suites)" % len(files))


if __name__ == "__main__":
    main()
