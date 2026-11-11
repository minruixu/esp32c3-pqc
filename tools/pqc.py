#!/usr/bin/env python3
"""Build a suite, run it on the ESP32-C3 from RAM, and collect its results.

Run through the wrapper, which sets up ESP-IDF:

    tools/pqc run mlkem                 build, load into RAM, run, save results
    tools/pqc run mlkem -f mlkem768/    run only jobs whose name contains the text
    tools/pqc host mlkem                build and run the suite on this machine
    tools/pqc check mlkem               compare device and host outputs
    tools/pqc size mlkem                code and data size per library
    tools/pqc md5                       checksum of the whole flash (read-only)
    tools/pqc reset                     restart the board into its own firmware

Nothing here writes to the board's flash: the program is loaded into RAM with
`esptool load_ram`, and a reset brings the board's own firmware back.
"""
import argparse
import atexit
import fcntl
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTS = os.path.join(ROOT, "results")
LOGS = os.path.join(RESULTS, "logs")
LOCK_PATH = "/tmp/pqc_esp32c3_device.lock"
PANIC = re.compile(r"Guru Meditation|abort\(\) was called|stack overflow|Stack protection fault|"
                   r"assert failed|Rebooting\.\.\.|CORRUPT HEAP|Core +0 register dump")


def log(msg):
    print(msg, flush=True)


def find_port():
    port = os.environ.get("PQC_PORT")
    if port:
        return port
    ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
    if not ports:
        sys.exit("no serial port found (set PQC_PORT)")
    return ports[0]


class DeviceLock:
    """One board, many users: serialize everything that talks to it."""

    def __enter__(self):
        self.f = open(LOCK_PATH, "w")
        try:
            fcntl.flock(self.f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            log("[device busy, waiting for the lock]")
            fcntl.flock(self.f, fcntl.LOCK_EX)
        return self

    def __exit__(self, *exc):
        fcntl.flock(self.f, fcntl.LOCK_UN)
        self.f.close()


def open_serial_no_reset(port):
    import serial
    ser = serial.serial_for_url(port, 115200, do_not_open=True)
    ser.timeout = 0.2
    ser.rts = True   # the states the OS asserts on open, so nothing toggles
    ser.dtr = True
    ser.open()
    ser.rts = False  # release RTS first to avoid a reset pulse
    ser.dtr = False
    return ser


def build_dir(args):
    return os.path.join(ROOT, "build", args.build_name or args.suite)


def do_build(args):
    bdir = build_dir(args)
    os.makedirs(LOGS, exist_ok=True)
    logp = os.path.join(LOGS, (args.build_name or args.suite) + ".build.log")
    cmd = ["idf.py", "-B", bdir, "-D", "PQC_SUITE=" + args.suite,
           "-D", "SDKCONFIG=" + os.path.join(bdir, "sdkconfig"),
           "-D", "SDKCONFIG_DEFAULTS=" + os.path.join(ROOT, args.sdkconfig)]
    for d in args.define or []:
        cmd += ["-D", d]
    cmd.append("build")
    t0 = time.time()
    with open(logp, "w") as lf:
        rc = subprocess.call(cmd, cwd=ROOT, stdout=lf, stderr=subprocess.STDOUT)
    if rc != 0:
        lines = open(logp, errors="replace").read().splitlines()
        errs = [l[:400] for l in lines if re.search(r"error:|undefined reference|will not fit|overflowed|"
                                                    r"multiple definition|No such file", l)]
        log("\n".join((errs or [l[:400] for l in lines])[-40:]))
        sys.exit("build failed, full log: " + logp)
    binp = os.path.join(bdir, "pqc_bench.bin")
    log("[built %s in %.0f s, image %d bytes]" % (args.suite, time.time() - t0, os.path.getsize(binp)))
    return binp


def parse_lines(lines):
    out = {"results": {}, "digests": {}, "checks": {}, "info": {}, "complete": False, "fails": None}
    for ln in lines:
        if not ln.startswith("@"):
            continue
        f = ln.rstrip().split("|")
        tag = f[0]
        try:
            if tag == "@RESULT":
                out["results"][f[1]] = {"iters": int(f[2]), "avg_us": int(f[3]), "min_us": int(f[4]),
                                        "max_us": int(f[5]), "stack": int(f[6]), "heap": int(f[7]),
                                        "rc": int(f[8])}
            elif tag == "@DIGEST":
                out["digests"][f[1]] = {"sha256_16": f[2], "len": int(f[3])}
            elif tag == "@CHECK":
                out["checks"][f[1]] = f[2]
            elif tag == "@INFO":
                out["info"][f[1]] = "|".join(f[2:])
            elif tag == "@START":
                out["cpu_mhz"] = int(f[3])
                out["heap_free_start"] = int(f[4])
            elif tag == "@DONE":
                out["complete"] = True
                out["fails"] = int(f[1])
                out["heap_free_min"] = int(f[3])
        except (IndexError, ValueError):
            pass  # a garbled line; the summary will show what is missing
    return out


def device_run(binp, args):
    port = find_port()
    lines, raw = [], b""
    crash = None
    # Load a private copy: while this run waits for the board, somebody else
    # may rebuild the same build directory.
    fd, image = tempfile.mkstemp(prefix="pqc_", suffix=".bin")
    os.close(fd)
    shutil.copyfile(binp, image)
    atexit.register(lambda: os.path.exists(image) and os.unlink(image))
    with DeviceLock():
        r = subprocess.run([sys.executable, "-m", "esptool", "-p", port, "--no-stub", "load_ram", image],
                           capture_output=True, text=True)
        if r.returncode != 0:
            log(r.stdout[-1500:] + r.stderr[-1500:])
            sys.exit("load_ram failed")
        ser = open_serial_no_reset(port)
        t0 = last = time.time()
        started = done = False
        go = ("GO " + (args.filter or "") + "\n").encode()
        try:
            while True:
                now = time.time()
                if now - t0 > args.timeout:
                    crash = "timeout after %d s" % args.timeout
                    break
                if now - last > args.idle:
                    crash = "no output for %d s" % args.idle
                    break
                chunk = ser.read(4096)
                if not chunk:
                    continue
                last = now
                raw += chunk
                while b"\n" in raw:
                    b, raw = raw.split(b"\n", 1)
                    ln = b.decode("utf-8", "replace").rstrip("\r")
                    if ln.startswith("@READY"):
                        if not started:
                            ser.write(go)
                            ser.flush()
                        continue
                    lines.append(ln)
                    if ln.startswith("@START"):
                        started = True
                    if args.verbose or not ln.startswith("@"):
                        log("  " + ln)
                    if PANIC.search(ln) and crash is None:
                        crash = ln.strip()
                        t0 = now - args.timeout + 2  # collect two more seconds of the dump
                    if ln.startswith("@DONE"):
                        done = True
                if done:
                    break
        finally:
            ser.close()
    if crash and crash.startswith("timeout") and PANIC.search("\n".join(lines)):
        crash = next(l.strip() for l in lines if PANIC.search(l))
    return lines, crash


def save_results(name, kind, parsed, lines, merge):
    os.makedirs(LOGS, exist_ok=True)
    with open(os.path.join(LOGS, "%s.%s.log" % (name, kind)), "w") as f:
        f.write("\n".join(lines) + "\n")
    path = os.path.join(RESULTS, "%s%s.json" % (name, "" if kind == "device" else ".host"))
    if merge and os.path.exists(path):
        old = json.load(open(path))
        for k in ("results", "digests", "checks", "info"):
            old.setdefault(k, {}).update(parsed[k])
        for k in ("cpu_mhz",):
            if k in parsed:
                old[k] = parsed[k]
        # Recount for the merged set, so an old failure that a later run fixed
        # does not linger.
        old["fails"] = (sum(1 for r in old["results"].values() if r.get("rc"))
                        + sum(1 for v in old["checks"].values() if v != "PASS"))
        old["complete"] = bool(parsed.get("complete"))
        parsed = old
    with open(path, "w") as f:
        json.dump(parsed, f, indent=1)
        f.write("\n")
    return path


def show(parsed):
    rows = parsed["results"]
    if rows:
        w = max(len(n) for n in rows)
        log("%-*s %5s %11s %11s %11s %8s %8s  %s" % (w, "job", "runs", "avg us", "min us", "max us",
                                                    "stack", "heap", "rc"))
        for n, r in rows.items():
            log("%-*s %5d %11d %11d %11d %8d %8d  %s" % (w, n, r["iters"], r["avg_us"], r["min_us"],
                                                        r["max_us"], r["stack"], r["heap"],
                                                        "ok" if r["rc"] == 0 else "FAIL rc=%d" % r["rc"]))
    bad = [k for k, v in parsed["checks"].items() if v != "PASS"]
    log("checks: %d passed, %d failed%s" % (len(parsed["checks"]) - len(bad), len(bad),
                                           (" (" + ", ".join(bad) + ")") if bad else ""))


def cmd_run(args):
    binp = os.path.join(build_dir(args), "pqc_bench.bin")
    if not args.no_build:
        binp = do_build(args)
    lines, crash = device_run(binp, args)
    parsed = parse_lines(lines)
    name = args.build_name or args.suite
    path = save_results(name, "device", parsed, lines, merge=bool(args.filter) or args.merge)
    show(parsed)
    if crash:
        log("RUN DID NOT FINISH: " + crash)
        log("device log: " + os.path.join(LOGS, name + ".device.log"))
        sys.exit(1)
    log("[%s: %d results, %s failures, saved %s]" % (name, len(parsed["results"]), parsed["fails"],
                                                   os.path.relpath(path, ROOT)))
    if args.check:
        cmd_host(args)
        cmd_check(args)
    sys.exit(1 if parsed["fails"] else 0)


def host_env():
    """Environment for the host compiler. On macOS the default SDK is not
    always usable from the command line, so pick the first one that links."""
    env = dict(os.environ)
    if sys.platform != "darwin" or "SDKROOT" in env:
        return env
    cache = os.path.join(ROOT, "build", "host", ".sdkroot")
    if os.path.exists(cache):
        sdk = open(cache).read().strip()
        if sdk:
            env["SDKROOT"] = sdk
        return env
    os.makedirs(os.path.dirname(cache), exist_ok=True)
    probe = os.path.join(os.path.dirname(cache), "probe.c")
    with open(probe, "w") as f:
        f.write("#include <stdio.h>\nint main(void){puts(\"ok\");return 0;}\n")
    sdks = [""] + sorted(glob.glob("/Library/Developer/CommandLineTools/SDKs/MacOSX[0-9]*.sdk"), reverse=True)
    for sdk in sdks:
        e = dict(env)
        if sdk:
            e["SDKROOT"] = sdk
        if subprocess.call(["cc", probe, "-o", probe + ".out"], env=e,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
            with open(cache, "w") as f:
                f.write(sdk)
            return e
    sys.exit("the host C compiler cannot link a program; set SDKROOT")


def cmd_host(args):
    bdir = os.path.join(ROOT, "build", "host", args.suite)
    os.makedirs(LOGS, exist_ok=True)
    logp = os.path.join(LOGS, args.suite + ".hostbuild.log")
    env = host_env()
    with open(logp, "w") as lf:
        rc = subprocess.call(["cmake", "-S", os.path.join(ROOT, "host"), "-B", bdir, "-G", "Ninja",
                              "-D", "PQC_SUITE=" + args.suite] + ["-D" + d for d in (args.define or [])],
                             stdout=lf, stderr=subprocess.STDOUT, env=env)
        rc = rc or subprocess.call(["cmake", "--build", bdir], stdout=lf, stderr=subprocess.STDOUT, env=env)
    if rc != 0:
        lines = open(logp, errors="replace").read().splitlines()
        errs = [l[:400] for l in lines if re.search(r"error:|undefined|No such file", l)]
        log("\n".join((errs or [l[:400] for l in lines])[-40:]))
        sys.exit("host build failed, full log: " + logp)
    cmd = [os.path.join(bdir, "suite_host")] + ([args.filter] if args.filter else [])
    r = subprocess.run(cmd, capture_output=True, text=True)
    lines = r.stdout.splitlines()
    parsed = parse_lines(lines)
    path = save_results(args.suite, "host", parsed, lines, merge=bool(args.filter))
    if getattr(args, "cmd", "") == "host":
        show(parsed)
    log("[host %s: %d results, %s failures, saved %s]" % (args.suite, len(parsed["results"]), parsed["fails"],
                                                        os.path.relpath(path, ROOT)))
    if r.returncode not in (0, 1) or not parsed["complete"]:
        log(r.stderr[-2000:])
        sys.exit("host run crashed")


def cmd_check(args):
    name = getattr(args, "build_name", None) or args.suite
    dev = json.load(open(os.path.join(RESULTS, name + ".json")))
    host = json.load(open(os.path.join(RESULTS, args.suite + ".host.json")))
    same = diff = missing = 0
    for label, d in sorted(dev["digests"].items()):
        h = host["digests"].get(label)
        if h is None:
            missing += 1
            log("  no host digest for " + label)
        elif h == d:
            same += 1
        else:
            diff += 1
            log("  MISMATCH %s device=%s host=%s" % (label, d["sha256_16"], h["sha256_16"]))
    devbad = [k for k, v in dev["checks"].items() if v != "PASS"]
    hostbad = [k for k, v in host["checks"].items() if v != "PASS"]
    log("[check %s: %d digests equal, %d different, %d without host value; failed checks: device %d, host %d]"
        % (name, same, diff, missing, len(devbad), len(hostbad)))
    if diff or missing or devbad or hostbad or same == 0:
        sys.exit(1)


def cmd_size(args):
    bdir = build_dir(args)
    r = subprocess.run(["idf.py", "-B", bdir, "size-components"], cwd=ROOT, capture_output=True, text=True)
    keep = [l for l in r.stdout.splitlines() if re.search(r"Archive File|libpqc_suite|libprims|libbench|Total", l)]
    log("\n".join(keep) if keep else r.stdout[-3000:])


def cmd_md5(args):
    from esptool.cmds import detect_chip
    with DeviceLock():
        esp = detect_chip(port=find_port(), baud=115200, connect_mode="default_reset")
        log("chip: " + esp.get_chip_description())
        esp = esp.run_stub()
        size = int(args.size, 0)
        esp.flash_set_parameters(size)
        log("flash_md5: " + esp.flash_md5sum(0, size))
        esp.hard_reset()
        esp._port.close()


def cmd_reset(args):
    with DeviceLock():
        r = subprocess.run([sys.executable, "-m", "esptool", "-p", find_port(), "--after", "hard_reset", "chip_id"],
                           capture_output=True, text=True)
        log(r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr[-500:])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("suite")
        p.add_argument("-f", "--filter", default="", help="comma-separated substrings of job names to run")
        p.add_argument("-D", "--define", action="append", help="extra CMake definition NAME=VALUE")
        p.add_argument("--build-name", help="build directory and result name (default: the suite name)")
        p.add_argument("--sdkconfig", default="sdkconfig.ram")

    p = sub.add_parser("run")
    common(p)
    p.add_argument("--no-build", action="store_true")
    p.add_argument("--timeout", type=int, default=900, help="give up after this many seconds")
    p.add_argument("--idle", type=int, default=180, help="give up after this many seconds without output")
    p.add_argument("--merge", action="store_true", help="add to the saved results instead of replacing them")
    p.add_argument("--check", action="store_true", help="also run on the host and compare outputs")
    p.add_argument("-v", "--verbose", action="store_true")
    p.set_defaults(fn=cmd_run)

    p = sub.add_parser("build")
    common(p)
    p.set_defaults(fn=do_build)

    p = sub.add_parser("host")
    common(p)
    p.set_defaults(fn=cmd_host)

    p = sub.add_parser("check")
    common(p)
    p.set_defaults(fn=cmd_check)

    p = sub.add_parser("size")
    common(p)
    p.set_defaults(fn=cmd_size)

    p = sub.add_parser("md5")
    p.add_argument("--size", default="0x800000")
    p.set_defaults(fn=cmd_md5)

    p = sub.add_parser("reset")
    p.set_defaults(fn=cmd_reset)

    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
