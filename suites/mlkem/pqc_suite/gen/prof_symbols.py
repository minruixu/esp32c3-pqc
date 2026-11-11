#!/usr/bin/env python3
"""Turn the profile printed by a -D PQC_PROFILE=1 image into a per-function
table.   usage: prof_symbols.py <device log> <elf> [nm program]"""
import bisect
import collections
import subprocess
import sys


def main():
    log, elf = sys.argv[1], sys.argv[2]
    nm = sys.argv[3] if len(sys.argv) > 3 else "riscv32-esp-elf-nm"
    syms = []
    for line in subprocess.run([nm, "-n", elf], capture_output=True, text=True).stdout.splitlines():
        f = line.split()
        if len(f) == 3 and f[1] in "tTwW":
            syms.append((int(f[0], 16), f[2]))
    addrs = [a for a, _ in syms]
    prof = collections.defaultdict(collections.Counter)
    totals = {}
    for line in open(log, errors="replace"):
        if not line.startswith("@INFO|prof/"):
            continue
        _, key, val = line.rstrip().split("|", 2)
        _, label, rank = key.split("/", 2) if key.count("/") == 2 else (None, key.split("/")[1], key.split("/")[-1])
        label = "/".join(key.split("/")[1:-1])
        if key.endswith("/total"):
            totals[label] = int(val.split()[0])
            continue
        addr, count = val.split()
        i = bisect.bisect_right(addrs, int(addr, 16) + 32) - 1
        prof[label][syms[i][1] if i >= 0 else addr] += int(count)
    for label, c in prof.items():
        tot = totals.get(label, sum(c.values()))
        print("%s: %d samples" % (label, tot))
        shown = 0
        for name, n in c.most_common(25):
            shown += n
            print("  %5.1f%%  %s" % (100.0 * n / tot, name))
        print("  %5.1f%%  (not in the listed buckets)" % (100.0 * (tot - sum(c.values())) / tot))


if __name__ == "__main__":
    main()
