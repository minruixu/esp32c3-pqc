#!/usr/bin/env python3
"""Print the result tables of suites/slhdsa/NOTES.md from results/slhdsa.json."""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
d = json.load(open(os.path.join(ROOT, "results", sys.argv[1] if len(sys.argv) > 1 else "slhdsa.json")))
R = d["results"]
SETS = ["sha2-128s", "sha2-128f", "sha2-192s", "sha2-192f", "sha2-256s", "sha2-256f",
        "shake-128s", "shake-128f", "shake-192s", "shake-192f", "shake-256s", "shake-256f"]


def ms(us):
    if us is None:
        return "n/a"
    v = us / 1000.0
    return "%.0f" % v if v >= 100 else ("%.1f" % v if v >= 10 else "%.2f" % v)


def get(s, v, op, key="avg_us"):
    r = R.get("slhdsa-%s/%s/%s" % (s, v, op))
    return r[key] if r and r["rc"] == 0 else None


print("| Parameter set | pk / sk / sig bytes | Operation | ref ms | opt ms | Speedup | ref stack | opt stack |")
print("|---|---|---|---:|---:|---:|---:|---:|")
for s in SETS:
    info = d["info"].get("slhdsa-" + s, "")
    sizes = " / ".join(x.split("=")[1] for x in info.split()[:3]) if info else ""
    for op in ("keygen", "sign", "verify"):
        a, b = get(s, "ref", op), get(s, "opt", op)
        sp = "%.1fx" % (a / b) if a and b else "n/a"
        print("| %s | %s | %s | %s | %s | %s | %s | %s |" % (
            ("SLH-DSA-" + s.upper().replace("SHA2", "SHA2").replace("S", "S") if op == "keygen" else "").replace("128S", "128s").replace("192S", "192s").replace("256S", "256s").replace("128F", "128f").replace("192F", "192f").replace("256F", "256f"),
            sizes if op == "keygen" else "", op, ms(a), ms(b), sp,
            get(s, "ref", op, "stack") or "n/a", get(s, "opt", op, "stack") or "n/a"))
bad = [k for k, v in d["checks"].items() if v != "PASS"]
print("\nchecks: %d, failed: %s" % (len(d["checks"]), bad or "none"))
