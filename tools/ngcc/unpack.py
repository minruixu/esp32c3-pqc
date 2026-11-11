#!/usr/bin/env python3
"""Unpack the downloaded packages, each into its own directory.

    python3 tools/ngcc/unpack.py [id ...]

The archives are untrusted: member names are checked so nothing is written
outside downloads/ngcc/src/<id>/, symbolic links are not created, and nothing
from an archive is ever executed. Large known-answer files are unpacked too;
they are data.
"""
import json
import os
import stat
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ZIPS = os.path.join(ROOT, "downloads", "ngcc", "zips")
SRC = os.path.join(ROOT, "downloads", "ngcc", "src")


def decode_name(info):
    """Zip names without the UTF-8 flag are often GBK in these packages."""
    if info.flag_bits & 0x800:
        return info.filename
    raw = info.filename.encode("cp437", "replace")
    for enc in ("utf-8", "gbk"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            pass
    return info.filename


def unpack(pid):
    zpath = os.path.join(ZIPS, pid + ".zip")
    dest = os.path.join(SRC, pid)
    marker = os.path.join(dest, ".unpacked")
    if os.path.exists(marker):
        return "already unpacked"
    os.makedirs(dest, exist_ok=True)
    real_dest = os.path.realpath(dest)
    n = skipped = 0
    with zipfile.ZipFile(zpath) as z:
        for info in z.infolist():
            name = decode_name(info).replace("\\", "/")
            target = os.path.realpath(os.path.join(dest, name))
            if not (target == real_dest or target.startswith(real_dest + os.sep)):
                skipped += 1
                continue
            mode = info.external_attr >> 16
            if stat.S_ISLNK(mode):
                skipped += 1
                continue
            if name.endswith("/"):
                os.makedirs(target, exist_ok=True)
                continue
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with z.open(info) as src, open(target, "wb") as out:
                while True:
                    chunk = src.read(1 << 20)
                    if not chunk:
                        break
                    out.write(chunk)
            os.chmod(target, 0o644)  # never executable
            n += 1
    nested = unpack_nested(dest) if not has_c_files(dest) else 0
    open(marker, "w").write("%d files\n" % n)
    return "%d files%s%s" % (n, (", %d entries skipped" % skipped) if skipped else "",
                             (", %d nested archives unpacked" % nested) if nested else "")


def has_c_files(top):
    return any(fn.endswith(".c") for _, _, fns in os.walk(top) for fn in fns)


def unpack_nested(dest):
    """A package without any C file keeps its code in an inner archive (RAR
    or zip). Unpack those with bsdtar, which refuses paths that leave the
    target directory, then drop symbolic links and execute permissions."""
    import subprocess
    count = 0
    for dp, _, fns in list(os.walk(dest)):
        for fn in fns:
            if not fn.lower().endswith((".rar", ".zip", ".7z", ".tar.gz", ".tgz")):
                continue
            target = os.path.join(dp, "_nested", os.path.splitext(fn)[0])
            os.makedirs(target, exist_ok=True)
            r = subprocess.run(["bsdtar", "-xf", os.path.join(dp, fn), "-C", target, "--no-same-owner",
                                "--no-same-permissions"], capture_output=True)
            if r.returncode != 0:
                continue
            count += 1
            for d2, dns, f2 in os.walk(target):
                for name in f2 + dns:
                    path = os.path.join(d2, name)
                    if os.path.islink(path):
                        os.remove(path)
                    elif os.path.isfile(path):
                        os.chmod(path, 0o644)
    return count


def main():
    ids = sys.argv[1:]
    if not ids:
        man = json.load(open(os.path.join(ROOT, "results", "ngcc", "packages.json")))
        ids = [p["id"] for p in man["packages"]
               if os.path.exists(os.path.join(ZIPS, p["id"] + ".zip")) and p.get("sha256")]
    for pid in ids:
        try:
            print("%s: %s" % (pid, unpack(pid)), flush=True)
        except Exception as e:
            print("%s: FAILED %s" % (pid, e), flush=True)


if __name__ == "__main__":
    main()
