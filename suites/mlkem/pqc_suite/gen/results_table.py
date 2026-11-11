#!/usr/bin/env python3
"""Print a Markdown table of reference against optimized results.
usage: results_table.py results/<suite>.json [more.json ...] [--base ref] [--var opt]"""
import json
import sys


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    base, var = "ref", "opt"
    if "--base" in sys.argv:
        base = sys.argv[sys.argv.index("--base") + 1]
        args.remove(base)
    if "--var" in sys.argv:
        var = sys.argv[sys.argv.index("--var") + 1]
        args.remove(var)
    res = {}
    for path in args:
        res.update(json.load(open(path))["results"])
    print("| Operation | %s ms | %s ms | Speedup | %s stack | %s stack |" % (base, var, base, var))
    print("|---|---:|---:|---:|---:|---:|")
    for name in res:
        alg, v, op = name.split("/", 2)
        if v != base or op == "kat":
            continue
        other = res.get("%s/%s/%s" % (alg, var, op))
        if not other:
            continue
        r = res[name]
        print("| %s %s | %.2f | %.2f | %.2f | %d | %d |" % (alg, op, r["avg_us"] / 1000, other["avg_us"] / 1000,
                                                       r["avg_us"] / max(other["avg_us"], 1), r["stack"],
                                                       other["stack"]))


if __name__ == "__main__":
    main()
