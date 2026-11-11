#!/usr/bin/env python3
"""Prepare the fetched LAC sources (LAC-v3b, 32-bit C version) for this project.

LAC is fetched into downloads/lac-intel32 and never committed. Its
lac_param.h hard-codes one security level, so the sources cannot be built for
several levels where they are. This script copies the files the KEM needs to
downloads/lac_port/ with two changes: the hard-coded level in lac_param.h is
only applied when the build has not chosen one, and the headers get include
guards.

rand.c and rng.c (OpenSSL-based) are not copied; suites/lac provides the same
functions on top of this project's primitives.
"""
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "downloads", "lac-intel32", "LAC-v3b", "opt32")
PORT = os.path.join(ROOT, "downloads", "lac_port")
FILES = ["bch.c", "bin-lwe.c", "ecc.c", "encrypt.c", "kem.c", "api.h", "bch.h", "bch-light.h",
         "bch128.h", "bch192.h", "bch256.h", "bin-lwe.h", "ecc.h", "rand.h"]

if not os.path.isdir(SRC):
    sys.exit("LAC sources missing: run tools/fetch/lac.sh")
os.makedirs(PORT, exist_ok=True)
for f in FILES:
    if f.endswith(".h"):
        # The headers have no include guards; in a single compilation unit
        # each must still be read only once.
        guard = "LAC_PORT_" + f.upper().replace(".", "_").replace("-", "_")
        body = open(os.path.join(SRC, f), newline="").read()
        open(os.path.join(PORT, f), "w", newline="").write(
            "#ifndef %s\n#define %s\n%s\n#endif\n" % (guard, guard, body))
    else:
        shutil.copyfile(os.path.join(SRC, f), os.path.join(PORT, f))
s = open(os.path.join(SRC, "lac_param.h"), newline="").read()
old = "#define LAC_LIGHT\r\n"
if s.count(old) != 1:
    sys.exit("unexpected lac_param.h, cannot port")
new = ("#if !defined(LAC_LIGHT) && !defined(LAC128) && !defined(LAC192) && !defined(LAC256)\r\n"
       "#define LAC_LIGHT\r\n#endif\r\n")
open(os.path.join(PORT, "lac_param.h"), "w", newline="").write(s.replace(old, new))
print("LAC port files written to", os.path.relpath(PORT, ROOT))
