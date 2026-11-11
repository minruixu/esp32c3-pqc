#!/usr/bin/env python3
"""Run several suites one after another on the board and summarize.

    python3 tools/runall.py [--idle S] [--timeout S] [--tag NAME] suite [suite ...]

For each suite this calls `tools/pqc run <suite> --check`: build, load into
RAM, run, run the host build, compare fingerprints. A failing suite does not
stop the others. The summary goes to results/logs/runall_<tag>.md.
"""
import argparse
import json
import os
import subprocess
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def counts(suite):
    path = os.path.join(ROOT, "results", suite + ".json")
    hpath = os.path.join(ROOT, "results", suite + ".host.json")
    if not os.path.exists(path):
        return None
    d = json.load(open(path))
    r, c, dg = d.get("results", {}), d.get("checks", {}), d.get("digests", {})
    hd = json.load(open(hpath)).get("digests", {}) if os.path.exists(hpath) else {}
    return {
        "jobs": len(r),
        "jobs_failed": sum(1 for v in r.values() if v.get("rc")),
        "checks": len(c),
        "checks_failed": sum(1 for v in c.values() if v != "PASS"),
        "fp_equal": sum(1 for k, v in dg.items() if hd.get(k) == v),
        "fp_diff": sum(1 for k, v in dg.items() if k in hd and hd[k] != v),
        "fp_nohost": sum(1 for k in dg if k not in hd),
        "complete": bool(d.get("complete")),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("suites", nargs="+")
    ap.add_argument("--idle", type=int, default=600)
    ap.add_argument("--timeout", type=int, default=3600)
    ap.add_argument("--tag", default=time.strftime("%Y%m%d_%H%M%S"))
    args = ap.parse_args()

    logdir = os.path.join(ROOT, "results", "logs")
    os.makedirs(logdir, exist_ok=True)
    out = os.path.join(logdir, "runall_%s.md" % args.tag)
    rows = []
    for s in args.suites:
        t0 = time.time()
        p = subprocess.run([os.path.join(ROOT, "tools", "pqc"), "run", s, "--check",
                            "--idle", str(args.idle), "--timeout", str(args.timeout)],
                           cwd=ROOT, capture_output=True, text=True)
        tail = [l for l in (p.stdout + p.stderr).splitlines() if l.startswith("[") or "RUN DID NOT" in l
                or "MISMATCH" in l or "failed" in l or "no host digest" in l][-6:]
        c = counts(s) or {}
        ok = (p.returncode == 0 and c.get("complete") and not c.get("jobs_failed") and not c.get("checks_failed")
              and not c.get("fp_diff") and not c.get("fp_nohost") and c.get("fp_equal", 0) > 0)
        rows.append((s, "PASS" if ok else "FAIL", p.returncode, time.time() - t0, c, tail))
        line = "%-28s %s rc=%d %5.0fs jobs %s/%s failed, checks %s/%s failed, fingerprints %s equal %s different %s without host" % (
            s, rows[-1][1], p.returncode, rows[-1][3], c.get("jobs_failed"), c.get("jobs"), c.get("checks_failed"),
            c.get("checks"), c.get("fp_equal"), c.get("fp_diff"), c.get("fp_nohost"))
        print(line, flush=True)
        with open(out, "w") as f:
            f.write("# Batch run %s\n\n" % args.tag)
            f.write("| Suite | Outcome | Seconds | Jobs (failed) | Checks (failed) | Fingerprints equal / different / no host |\n")
            f.write("|---|---|---:|---:|---:|---|\n")
            for (su, st, rc, dt, cc, _t) in rows:
                f.write("| %s | %s | %.0f | %s (%s) | %s (%s) | %s / %s / %s |\n" % (
                    su, st, dt, cc.get("jobs"), cc.get("jobs_failed"), cc.get("checks"), cc.get("checks_failed"),
                    cc.get("fp_equal"), cc.get("fp_diff"), cc.get("fp_nohost")))
            f.write("\n")
            for (su, st, rc, dt, cc, tl) in rows:
                if st != "PASS":
                    f.write("## %s (exit code %d)\n\n```\n%s\n```\n\n" % (su, rc, "\n".join(tl)))
    bad = [r[0] for r in rows if r[1] != "PASS"]
    print("DONE: %d suites, %d passed, failed: %s; summary %s" % (len(rows), len(rows) - len(bad), bad or "none",
                                                               os.path.relpath(out, ROOT)), flush=True)


if __name__ == "__main__":
    main()
