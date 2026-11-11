#!/usr/bin/env python3
"""Print the tables of suites/hashes/NOTES.md from results/hashes.json."""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
d = json.load(open(os.path.join(ROOT, "results", "hashes.json")))
ROWS = [
    ("SHA-256, one block per call", "sha256/ref/1block-x1000", "sha256/opt/1block-x1000", 1000),
    ("SHA-256, 16 blocks per call", "sha256/ref/16blocks-x64", "sha256/opt/16blocks-x64", 1024),
    ("SHA-256 of a 55-byte message", "sha256/ref/55-byte-message-x1000", "sha256/opt/55-byte-message-x1000", 1000),
    ("SM3, one block", "sm3/ref/1block-x1000", "sm3/opt/1block-x1000", 1000),
    ("SM3 of a 55-byte message", "sm3/ref/55-byte-message-x1000", "sm3/opt/55-byte-message-x1000", 1000),
    ("SHA-512, one block", "sha512/ref/1block-x200", "sha512/opt/1block-x200", 200),
    ("SHA-512, one block given as bytes", "sha512/ref/1block-x200", "sha512/opt/1block-from-bytes-x200", 200),
    ("Keccak-f[1600]", "keccak/ref/x200", "keccak/opt/x200", 200),
]
OTHER = [
    ("SHA-256, slhdsa-c's portable C, one block", "sha256/up/1block-x1000", 1000),
    ("SHA-512, slhdsa-c's portable C, one block", "sha512/up/1block-x200", 200),
    ("Keccak-f[1600], slhdsa-c's portable C", "keccak/up/x200", 200),
    ("SHA-256 of a 55-byte message with pqc_sha256_hw_short()", "sha256/raw/55-byte-message-x1000", 1000),
    ("SHA-256 accelerator used directly: load state, one block, read 32 bytes", "sha256/raw/1block-read32-x1000", 1000),
    ("SHA-256 accelerator used directly: load state, one block, read 16 bytes", "sha256/raw/1block-read16-x1000", 1000),
]
R = d["results"]


def us(name, n):
    r = R.get(name)
    return r["min_us"] / n if r else None


print("| Primitive | ref us | ref cycles | opt us | opt cycles | Speedup |")
print("|---|---:|---:|---:|---:|---:|")
for title, a, b, n in ROWS:
    x, y = us(a, n), us(b, n)
    if x is None or y is None:
        continue
    print("| %s | %.2f | %d | %.2f | %d | %.1fx |" % (title, x, round(x * 160), y, round(y * 160), x / y))
print()
print("| Other measurements | us | cycles |")
print("|---|---:|---:|")
for title, a, n in OTHER:
    x = us(a, n)
    if x is not None:
        print("| %s | %.2f | %d |" % (title, x, round(x * 160)))
print()
for k, v in d["info"].items():
    print(k, ":", v)
bad = [k for k, v in d["checks"].items() if v != "PASS"]
print("checks: %d, failed: %s" % (len(d["checks"]), bad or "none"))
