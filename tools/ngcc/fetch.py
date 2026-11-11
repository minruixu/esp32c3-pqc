#!/usr/bin/env python3
"""Download the NGCC round-1 public-key submission packages.

The packages are published by the Institute of Commercial Cryptography
Standards at https://www.niccs.org.cn/niccs/Proposal/pc/list.html. This
script asks the site's own listing API for the three public-key categories,
downloads every zip file into downloads/ngcc/zips/ (one request at a time,
with a pause between requests, resuming partial files), and writes
results/ngcc/packages.json with the URL, size and SHA-256 of each package.

The packages themselves are never committed to this repository.

    python3 tools/ngcc/fetch.py            list and download
    python3 tools/ngcc/fetch.py --list     only refresh the list
"""
import hashlib
import json
import os
import sys
import time
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SITE = "https://www.niccs.org.cn"
ZIPS = os.path.join(ROOT, "downloads", "ngcc", "zips")
MANIFEST = os.path.join(ROOT, "results", "ngcc", "packages.json")
CHANNELS = [("R1KeyEncapsulation", "KEM"), ("R1DigitalSignature", "SIG"), ("R1KeyExchange", "KEX")]
UA = {"User-Agent": "Mozilla/5.0"}
PAUSE = 2.0


def api_list(channel):
    body = urllib.parse.urlencode([("current", "1"), ("pageSize", "100"), ("webSiteCode[]", "niccs"),
                                   ("channelCode[]", channel), ("isContent", "0")]).encode()
    req = urllib.request.Request(SITE + "/queryList", data=body, headers=UA)
    with urllib.request.urlopen(req, timeout=60) as r:
        data = json.load(r)["data"]
    out = []
    for item in data["results"]:
        src = item["source"]
        files = [f for f in json.loads(src.get("articleFiles") or "[]") if f.get("filePath")]
        out.append({"title": src["title"].strip(), "published": src.get("pubDate", ""),
                    "page": json.loads(src.get("urls") or "{}").get("pc", ""),
                    "files": [f["filePath"] for f in files]})
    if len(out) != data["total"]:
        sys.exit("listing of %s is incomplete: %d of %d" % (channel, len(out), data["total"]))
    return out


def safe_name(title):
    keep = "".join(c if (c.isalnum() or c in "+-_.") else "_" for c in title)
    return keep.strip("_") or "unnamed"


def download(url, dest):
    """Download with resume. Returns the size on disk."""
    part = dest + ".part"
    have = os.path.getsize(part) if os.path.exists(part) else 0
    headers = dict(UA)
    if have:
        headers["Range"] = "bytes=%d-" % have
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=120) as r:
        mode = "ab" if (have and r.status == 206) else "wb"
        with open(part, mode) as f:
            while True:
                chunk = r.read(1 << 16)
                if not chunk:
                    break
                f.write(chunk)
    os.replace(part, dest)
    return os.path.getsize(dest)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    os.makedirs(ZIPS, exist_ok=True)
    os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
    old = {}
    if os.path.exists(MANIFEST):
        old = {p["id"]: p for p in json.load(open(MANIFEST))["packages"]}
    packages = []
    for channel, kind in CHANNELS:
        items = api_list(channel)
        print("%s: %d candidates" % (kind, len(items)), flush=True)
        for it in items:
            pid = kind.lower() + "_" + safe_name(it["title"])
            if not it["files"]:
                packages.append({"id": pid, "name": it["title"], "type": kind, "url": None})
                continue
            path = it["files"][0]
            entry = {"id": pid, "name": it["title"], "type": kind, "published": it["published"],
                     "url": SITE + urllib.parse.quote(path), "file": os.path.basename(path)}
            prev = old.get(pid, {})
            for k in ("size", "sha256"):
                if k in prev and prev.get("url") == entry["url"]:
                    entry[k] = prev[k]
            packages.append(entry)
        time.sleep(PAUSE)
    packages.sort(key=lambda p: (p["type"], p["name"].lower()))

    def save():
        with open(MANIFEST, "w") as f:
            json.dump({"source": SITE + "/niccs/Proposal/pc/list.html", "packages": packages}, f,
                      indent=1, ensure_ascii=False)
            f.write("\n")

    save()
    if "--list" in sys.argv:
        return
    for i, p in enumerate(packages):
        if not p.get("url"):
            print("[%d/%d] %s: no zip file listed" % (i + 1, len(packages), p["id"]), flush=True)
            continue
        dest = os.path.join(ZIPS, p["id"] + ".zip")
        if os.path.exists(dest) and p.get("size") == os.path.getsize(dest):
            continue
        for attempt in range(4):
            try:
                size = download(p["url"], dest)
                break
            except Exception as e:  # network errors: wait and resume
                print("  retry %s: %s" % (p["id"], e), flush=True)
                time.sleep(10 * (attempt + 1))
        else:
            print("[%d/%d] %s: FAILED" % (i + 1, len(packages), p["id"]), flush=True)
            continue
        p["size"] = size
        p["sha256"] = sha256(dest)
        save()
        print("[%d/%d] %s: %.1f MB" % (i + 1, len(packages), p["id"], size / 1e6), flush=True)
        time.sleep(PAUSE)
    done = sum(1 for p in packages if p.get("sha256"))
    print("%d of %d packages downloaded, %.0f MB in total"
          % (done, len(packages), sum(p.get("size", 0) for p in packages) / 1e6))


if __name__ == "__main__":
    main()
