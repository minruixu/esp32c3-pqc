#!/usr/bin/env python3
"""Print the saved device results of one or more suites as Markdown tables:
one row per algorithm and operation, reference against optimized variant.

    suites/pqmagic_common/results_table.py aigis_enc aigis_sig ...
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def ms(us):
    return "%.1f" % (us / 1000.0) if us < 10_000_000 else "%.0f" % (us / 1000.0)


def table(suite):
    path = os.path.join(ROOT, "results", suite + ".json")
    if not os.path.exists(path):
        print("### %s\n\n(no device results)\n" % suite)
        return
    d = json.load(open(path))
    res = d["results"]
    host = {}
    hp = os.path.join(ROOT, "results", suite + ".host.json")
    if os.path.exists(hp):
        host = json.load(open(hp)).get("digests", {})
    same = sum(1 for k, v in d["digests"].items() if host.get(k) == v)
    print("### %s\n" % suite)
    print("Device/host fingerprints equal: %d of %d. Failed checks: %s. Free heap at start: %s B.\n"
          % (same, len(d["digests"]), sum(1 for v in d["checks"].values() if v != "PASS"),
             d.get("heap_free_start", "?")))
    print("| Algorithm | Operation | ref ms | opt ms | speed-up | ref stack B | opt stack B | heap B |")
    print("|---|---|---:|---:|---:|---:|---:|---:|")
    for name, r in res.items():
        alg, var, op = name.split("/")
        if var != "ref" or op in ("tamper",):
            continue
        o = res.get("%s/opt/%s" % (alg, op))
        if r["rc"] != 0:
            print("| %s | %s | failed (rc=%d) | | | | | |" % (alg, op, r["rc"]))
            continue
        if o and o["rc"] == 0:
            sp = "%.2fx" % (r["avg_us"] / o["avg_us"]) if o["avg_us"] else ""
            print("| %s | %s | %s | %s | %s | %d | %d | %d |" % (alg, op, ms(r["avg_us"]), ms(o["avg_us"]), sp,
                                                           r["stack"], o["stack"], max(r["heap"], o["heap"])))
        else:
            print("| %s | %s | %s | | | %d | | %d |" % (alg, op, ms(r["avg_us"]), r["stack"], r["heap"]))
    info = {k: v for k, v in d["info"].items() if "/" not in k and k != "heap"}
    if info:
        print("\nSizes: " + "; ".join("%s: %s" % (k, v) for k, v in info.items()))
    print()


for s in sys.argv[1:]:
    table(s)
