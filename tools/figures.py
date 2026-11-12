#!/usr/bin/env python3
"""Draw the figures of the READMEs from the measured results.

    python3 tools/figures.py        write docs/assets/*.svg

Four figures, each in a light and a dark variant and in English and Chinese:

    hero      title, the chip, and the headline counts (from results/*.json)
    speedup   time per operation, reference -> optimized, on a log axis
    pipeline  how a suite is built, run and checked
    ngcc      how far the NGCC round-1 candidates got (from docs/ngcc/status.md)

Everything is computed from the result files, so the figures cannot drift
away from the tables. The SVGs use system fonts only and no scripts; GitHub
shows the light or the dark file through <picture>.
"""
import json
import math
import os
import re

import report

ROOT = report.ROOT
OUT = os.path.join(ROOT, "docs", "assets")

SANS = ("-apple-system, BlinkMacSystemFont, 'Segoe UI', 'Noto Sans', Helvetica, Arial, "
        "'PingFang SC', 'Hiragino Sans GB', 'Microsoft YaHei', 'Noto Sans CJK SC', sans-serif")
MONO = "ui-monospace, SFMono-Regular, 'SF Mono', Menlo, Consolas, 'Liberation Mono', monospace"

# Surfaces and inks are GitHub's own, so the figures sit in the page instead
# of on it. The blues are one ramp: the darker the step in the light theme
# (the lighter in the dark theme), the more it matters. Checked for step
# contrast and for contrast against both surfaces.
THEMES = {
    "light": dict(surface="#ffffff", tile="#f6f8fa", border="#d1d9e0", ink="#1f2328", ink2="#59636e",
                  muted="#818b98", grid="#eaeef2", axis="#c8d1da",
                  opt="#1c5cab", ref="#86b6ef", link="#cde2fb", accent="#2a78d6",
                  ramp=["#104281", "#256abf", "#5598e7", "#86b6ef"]),
    "dark": dict(surface="#0d1117", tile="#151b23", border="#3d444d", ink="#f0f6fc", ink2="#9198a1",
                 muted="#7d8590", grid="#1c2128", axis="#3d444d",
                 opt="#6da7ec", ref="#1c5cab", link="#0d366b", accent="#3987e5",
                 ramp=["#9ec5f4", "#5598e7", "#256abf", "#184f95"]),
}

TEXT = {
    "en": {
        "hero_kicker": "RV32IMC  ·  160 MHz  ·  385 KB RAM  ·  NO FPU",
        "hero_title": ["Post-quantum cryptography", "on an ESP32-C3"],
        "hero_sub": ["The NIST standards and the Chinese algorithms: ported, checked on the chip",
                     "against known answers, measured, and made faster for this particular core."],
        "tiles": ["suites, one RAM image each", "measured jobs, all passing", "output fingerprints equal to the host",
                  "bytes written to the board's flash"],
        "sp_title": "Time per operation: reference → optimized",
        "sp_sub": "Measured on the chip at 160 MHz. Log scale: one gridline is a factor of ten.",
        "sp_ref": "Reference (upstream portable C)",
        "sp_opt": "Optimized for the ESP32-C3",
        "sp_cols": ["Key generation", "Encapsulation / signing", "Decapsulation / verification"],
        "sp_speed": "Speedup",
        "sp_groups": ["NIST", "CHINA"],
        "pl_title": "How every number is checked",
        "pl_lane1": "ON THE CHIP",
        "pl_lane2": "ON THE BUILD MACHINE",
        "pl_src": ["One suite", "reference + optimized"],
        "pl_img": ["RAM-only image", "flash is never written"],
        "pl_run": ["Run on the ESP32-C3", "time, stack, heap, KATs"],
        "pl_fp1": ["Fingerprints", "of every output"],
        "pl_host": ["Host build", "reference code only"],
        "pl_hrun": ["Run on the host", "same seeds"],
        "pl_fp2": ["Fingerprints", "of the same outputs"],
        "pl_cmp": ["Equal?", "compared label by label"],
        "pl_note": "A wrong optimization shows up as a mismatch, not as a faster number.",
        "ng_title": "NGCC round 1: how far the %d candidates of the generic port got",
        "ng_sub": "Built unmodified from the official submission packages; no per-candidate work.",
        "ng_parts": ["Runs on the ESP32-C3, fingerprints equal the host", "Runs on the host only",
                     "Builds, fails its self-test", "Does not build"],
        "ng_kinds": ["Key encapsulation", "Signature", "Key exchange"],
    },
    "zh": {
        "hero_kicker": "RV32IMC  ·  160 MHz  ·  385 KB 内存  ·  无浮点单元",
        "hero_title": ["ESP32-C3 上的", "后量子密码"],
        "hero_sub": ["NIST 标准与中国算法：逐个移植，在芯片上用已知答案校验、实测，",
                     "再针对这颗内核做性能优化。"],
        "tiles": ["个套件，各一个内存镜像", "项实测，全部通过", "个输出指纹与主机一致", "字节写入开发板 Flash"],
        "sp_title": "单次操作耗时：参考实现 → 优化后",
        "sp_sub": "芯片上 160 MHz 实测。对数坐标：每条竖线相差十倍。",
        "sp_ref": "参考实现（上游可移植 C 代码）",
        "sp_opt": "针对 ESP32-C3 优化后",
        "sp_cols": ["密钥生成", "封装 / 签名", "解封装 / 验签"],
        "sp_speed": "提速",
        "sp_groups": ["NIST", "中国"],
        "pl_title": "每一个数字是怎么校验的",
        "pl_lane1": "芯片上",
        "pl_lane2": "编译机上",
        "pl_src": ["一个套件", "参考实现 + 优化实现"],
        "pl_img": ["纯内存镜像", "从不写 Flash"],
        "pl_run": ["在 ESP32-C3 上运行", "耗时、栈、堆、已知答案"],
        "pl_fp1": ["输出指纹", "每一个输出"],
        "pl_host": ["主机编译", "只编译参考实现"],
        "pl_hrun": ["在主机上运行", "相同的随机种子"],
        "pl_fp2": ["输出指纹", "同一批输出"],
        "pl_cmp": ["是否一致？", "逐项比对"],
        "pl_note": "优化写错了，表现出来是指纹不一致，而不是一个更快的数字。",
        "ng_title": "商用密码征集第一轮：通用移植的 %d 个候选各走到哪一步",
        "ng_sub": "直接用官方提交包原样编译，没有针对单个候选做任何改动。",
        "ng_parts": ["在 ESP32-C3 上跑通，指纹与主机一致", "只在主机上跑通", "能编译，自检不通过", "无法编译"],
        "ng_kinds": ["密钥封装", "数字签名", "密钥交换"],
    },
}


def esc(s):
    return str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def width(s, size):
    """Rough rendered width of a string in the system sans."""
    w = 0.0
    for ch in str(s):
        if ord(ch) > 0x2E80:
            w += 1.0
        elif ch in "il.,:;·' ":
            w += 0.30
        elif ch in "mwMW→":
            w += 0.85
        elif ch.isupper() or ch.isdigit():
            w += 0.62
        else:
            w += 0.53
    return w * size


def wrap(text, size, room):
    """Break a label into lines that fit; Chinese breaks anywhere, English at spaces."""
    words = text.split(" ") if " " in text else list(text)
    joiner = " " if " " in text else ""
    lines, cur = [], ""
    for w in words:
        trial = (cur + joiner + w) if cur else w
        if cur and width(trial, size) > room:
            lines.append(cur)
            cur = w
        else:
            cur = trial
    return lines + [cur]


class Svg:
    def __init__(self, w, h, t, label):
        self.w, self.h, self.t = w, h, t
        self.out = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" '
                    'role="img" aria-label="%s" font-family="%s">' % (w, h, w, h, esc(label), SANS),
                    '<rect x="0.5" y="0.5" width="%d" height="%d" rx="12" fill="%s" stroke="%s"/>'
                    % (w - 1, h - 1, t["surface"], t["border"])]

    def text(self, x, y, s, size=13, fill=None, weight=None, anchor=None, family=None, spacing=None):
        a = ['x="%.1f"' % x, 'y="%.1f"' % y, 'font-size="%s"' % size, 'fill="%s"' % (fill or self.t["ink"])]
        if weight:
            a.append('font-weight="%s"' % weight)
        if anchor:
            a.append('text-anchor="%s"' % anchor)
        if family:
            a.append('font-family="%s"' % family)
        if spacing:
            a.append('letter-spacing="%s"' % spacing)
        self.out.append("<text %s>%s</text>" % (" ".join(a), esc(s)))

    def line(self, x1, y1, x2, y2, stroke, w=1, cap=None):
        self.out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="%s" stroke-width="%s"%s/>'
                        % (x1, y1, x2, y2, stroke, w, ' stroke-linecap="%s"' % cap if cap else ""))

    def rect(self, x, y, w, h, fill, rx=0, stroke=None):
        self.out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%s" fill="%s"%s/>'
                        % (x, y, w, h, rx, fill, ' stroke="%s"' % stroke if stroke else ""))

    def dot(self, x, y, r, fill, ring=None):
        self.out.append('<circle cx="%.1f" cy="%.1f" r="%s" fill="%s"%s/>'
                        % (x, y, r, fill, ' stroke="%s" stroke-width="2"' % ring if ring else ""))

    def raw(self, s):
        self.out.append(s)

    def save(self, name):
        os.makedirs(OUT, exist_ok=True)
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write("\n".join(self.out) + "\n</svg>\n")


# ---------------------------------------------------------------------------
# data

def totals():
    """Suites, jobs and fingerprints over all result files that belong to a suite."""
    suites = jobs = fps = 0
    for name in sorted(os.listdir(os.path.join(ROOT, "suites"))):
        dev = os.path.join(report.RESULTS, name + ".json")
        host = os.path.join(report.RESULTS, name + ".host.json")
        if not os.path.exists(dev):
            continue
        d = json.load(open(dev))
        hd = json.load(open(host)).get("digests", {}) if os.path.exists(host) else {}
        bad = sum(1 for v in d["results"].values() if v.get("rc")) + sum(1 for v in d["checks"].values() if v != "PASS")
        equal = sum(1 for k, v in d["digests"].items() if hd.get(k) == v)
        if bad or equal != len(d["digests"]):
            raise SystemExit("figures: %s has failures or unequal fingerprints; fix the results first" % name)
        suites, jobs, fps = suites + 1, jobs + len(d["results"]), fps + equal
    return suites, jobs, fps


def highlight_rows():
    """(label, [(ref_us, opt_us) or None for the three operation columns]) per README row."""
    rows = []
    for label, suites, alg in report.HIGHLIGHTS:
        table = {}
        for s in suites:
            path = os.path.join(report.RESULTS, s + ".json")
            if os.path.exists(path):
                for job, r in json.load(open(path)).get("results", {}).items():
                    table.setdefault(report.split(job), r)
        cells = []
        for _c, ops in report.COLS:
            cell = None
            for o in ops:
                ref, opt = table.get((alg, "ref", o)), table.get((alg, "opt", o))
                if ref and opt and ref["rc"] == 0 and opt["rc"] == 0:
                    cell = (ref["avg_us"], opt["avg_us"])
                    break
            cells.append(cell)
        if any(cells):
            rows.append((label, cells))
    return rows


def ngcc_counts():
    """Outcome counts per kind of candidate, from docs/ngcc/status.md."""
    kinds = {"Key encapsulation": 0, "Digital signatures": 1, "Key exchange": 2}
    names = ["works on device", "host only", "does not work", "does not build"]
    counts = [[0] * 4 for _ in range(3)]
    kind = None
    for line in open(os.path.join(ROOT, "docs", "ngcc", "status.md")):
        if line.startswith("## "):
            kind = kinds.get(line[3:].strip())
        elif kind is not None and line.startswith("| ") and not line.startswith("| Candidate") \
                and not line.startswith("|---"):
            cols = [c.strip() for c in line.strip().strip("|").split("|")]
            if len(cols) > 3 and cols[3] in names:
                counts[kind][names.index(cols[3])] += 1
    return counts


# ---------------------------------------------------------------------------
# hero

def chip(svg, cx, cy, size):
    """A QFN32 package seen from above, with a lattice inside."""
    t = svg.t
    half = size / 2.0
    pin_w, pin_l = size / 16.0, size / 9.0
    for i in range(8):
        off = -half + size * (i + 1) / 9.0
        for (x, y, w, h) in ((cx + off - pin_w / 2, cy - half - pin_l, pin_w, pin_l),
                             (cx + off - pin_w / 2, cy + half, pin_w, pin_l),
                             (cx - half - pin_l, cy + off - pin_w / 2, pin_l, pin_w),
                             (cx + half, cy + off - pin_w / 2, pin_l, pin_w)):
            svg.rect(x, y, w, h, t["axis"], rx=1.5)
    svg.rect(cx - half, cy - half, size, size, t["tile"], rx=14, stroke=t["border"])
    svg.dot(cx - half + 16, cy - half + 16, 3.5, t["muted"])
    n, step = 7, size * 0.66 / 6
    x0, y0 = cx - step * 3, cy - step * 3
    basis = {(1, 5), (3, 4), (2, 2)}
    svg.line(x0 + step * 1, y0 + step * 5, x0 + step * 3, y0 + step * 4, t["accent"], 2, cap="round")
    svg.line(x0 + step * 1, y0 + step * 5, x0 + step * 2, y0 + step * 2, t["accent"], 2, cap="round")
    for i in range(n):
        for j in range(n):
            hit = (i, j) in basis
            svg.dot(x0 + step * i, y0 + step * j, 4.5 if hit else 2.6, t["accent"] if hit else t["muted"],
                    ring=t["tile"] if hit else None)


def hero(lang, theme):
    t, s = THEMES[theme], TEXT[lang]
    suites, jobs, fps = totals()
    svg = Svg(960, 356, t, " ".join(s["hero_title"]))
    x = 44
    svg.text(x, 58, s["hero_kicker"], size=12.5, fill=t["ink2"], family=MONO, spacing="0.04em")
    svg.text(x, 108, s["hero_title"][0], size=38, weight=700)
    svg.text(x, 154, s["hero_title"][1], size=38, weight=700)
    svg.rect(x, 170, 56, 4, t["accent"], rx=2)
    svg.text(x, 202, s["hero_sub"][0], size=15, fill=t["ink2"])
    svg.text(x, 224, s["hero_sub"][1], size=15, fill=t["ink2"])
    values = ["%d" % suites, "{:,}".format(jobs), "{:,}".format(fps), "0"]
    gap = 12
    tw = (svg.w - 2 * x - 3 * gap) / 4.0
    for i, (v, label) in enumerate(zip(values, s["tiles"])):
        tx = x + i * (tw + gap)
        svg.rect(tx, 250, tw, 78, t["tile"], rx=8, stroke=t["border"])
        svg.text(tx + 16, 281, v, size=24, weight=650)
        lines = wrap(label, 11.5, tw - 32)
        for k, text in enumerate(lines[:2]):
            svg.text(tx + 16, 300 + 15 * k, text, size=11.5, fill=t["ink2"])
    chip(svg, 800, 132, 132)
    return svg


# ---------------------------------------------------------------------------
# speedup: dumbbells on a log axis, one panel per operation

def fmt_x(a, b):
    def one(v):
        return "%.0f" % v if v >= 9.95 else "%.1f" % v
    lo, hi = one(a), one(b)
    return "%s×" % lo if lo == hi else "%s–%s×" % (lo, hi)


def speedup(lang, theme):
    t, s = THEMES[theme], TEXT[lang]
    rows = highlight_rows()
    split_at = next(i for i, (label, _c) in enumerate(rows) if label.startswith("Aigis"))
    groups = [(s["sp_groups"][0], rows[:split_at]), (s["sp_groups"][1], rows[split_at:])]
    W, pad, row_h, group_h = 960, 28, 26, 30
    x_label, x0, pw, gap = pad, 208, 196, 18
    top = 132
    plot_h = sum(group_h + row_h * len(r) for _g, r in groups)
    H = top + plot_h + 52
    svg = Svg(W, H, t, s["sp_title"])
    svg.text(pad, 40, s["sp_title"], size=17, weight=650)
    svg.text(pad, 62, s["sp_sub"], size=12.5, fill=t["ink2"])
    lx = pad
    for color, label in ((t["ref"], s["sp_ref"]), (t["opt"], s["sp_opt"])):
        svg.dot(lx + 5, 84, 5, color)
        svg.text(lx + 16, 88.5, label, size=12.5, fill=t["ink2"])
        lx += 16 + width(label, 12.5) + 26

    lo, hi = 3.0, 9.0          # log10 of microseconds: 1 ms .. 1000 s

    def px(panel, us):
        v = min(max(math.log10(us), lo), hi)
        return x0 + panel * (pw + gap) + (v - lo) / (hi - lo) * pw

    y_axis = top + plot_h + 6
    for p in range(3):
        left = x0 + p * (pw + gap)
        svg.text(left, top - 14, s["sp_cols"][p], size=12.5, weight=600, fill=t["ink2"])
        for d in range(int(lo), int(hi) + 1):
            gx = left + (d - lo) / (hi - lo) * pw
            svg.line(gx, top - 4, gx, y_axis, t["grid"])
        svg.line(left, y_axis, left + pw, y_axis, t["axis"])
        for d, name in ((3, "1 ms"), (5, "100 ms"), (7, "10 s")):
            gx = left + (d - lo) / (hi - lo) * pw
            svg.line(gx, y_axis, gx, y_axis + 4, t["axis"])
            svg.text(gx, y_axis + 18, name, size=11, fill=t["muted"], anchor="start" if d == 3 else "middle")
    x_speed = x0 + 3 * pw + 2 * gap + 22
    svg.text(W - pad, top - 14, s["sp_speed"], size=12.5, weight=600, fill=t["ink2"], anchor="end")

    y = top
    for gname, grows in groups:
        svg.text(x_label, y + 19, gname, size=11, weight=650, fill=t["muted"], spacing="0.08em")
        svg.line(x_label + width(gname, 11) + 16, y + 15, x_speed - 22, y + 15, t["grid"])
        y += group_h
        for label, cells in grows:
            cy = y + row_h / 2.0
            svg.text(x_label, cy + 4.5, label, size=13)
            ratios = []
            for p, cell in enumerate(cells):
                if not cell:
                    continue
                ref, opt = cell
                ratios.append(ref / float(opt))
                a, b = px(p, ref), px(p, opt)
                if abs(a - b) > 1:
                    svg.line(a, cy, b, cy, t["link"], 4, cap="round")
                svg.dot(a, cy, 5, t["ref"], ring=t["surface"])
                svg.dot(b, cy, 5, t["opt"], ring=t["surface"])
            if ratios:
                svg.text(W - pad, cy + 4.5, fmt_x(min(ratios), max(ratios)), size=13, weight=600, anchor="end")
            y += row_h
    return svg


# ---------------------------------------------------------------------------
# pipeline

def box(svg, x, y, w, h, lines, strong=False):
    t = svg.t
    svg.rect(x, y, w, h, t["tile"], rx=8, stroke=t["accent"] if strong else t["border"])
    svg.text(x + w / 2.0, y + h / 2.0 - 3, lines[0], size=13, weight=650, anchor="middle")
    svg.text(x + w / 2.0, y + h / 2.0 + 14, lines[1], size=11, fill=t["ink2"], anchor="middle")


def arrow(svg, x1, y1, x2, y2):
    t = svg.t
    ang = math.atan2(y2 - y1, x2 - x1)
    ex, ey = x2 - 2 * math.cos(ang), y2 - 2 * math.sin(ang)
    svg.line(x1, y1, ex, ey, t["muted"], 1.5)
    a, b = ang + 2.6, ang - 2.6
    svg.raw('<path d="M%.1f %.1f L%.1f %.1f L%.1f %.1f Z" fill="%s"/>'
            % (x2, y2, x2 + 8 * math.cos(a), y2 + 8 * math.sin(a), x2 + 8 * math.cos(b), y2 + 8 * math.sin(b),
               t["muted"]))


def tall(svg, x, y, w, h, lines, strong=False):
    t = svg.t
    svg.rect(x, y, w, h, t["tile"], rx=8, stroke=t["accent"] if strong else t["border"])
    svg.text(x + w / 2.0, y + h / 2.0 - 3, lines[0], size=13, weight=650, anchor="middle")
    svg.text(x + w / 2.0, y + h / 2.0 + 14, lines[1], size=11, fill=t["ink2"], anchor="middle")


def pipeline(lang, theme):
    t, s = THEMES[theme], TEXT[lang]
    svg = Svg(960, 322, t, s["pl_title"])
    pad, bw, bh, gap = 28, 152, 58, 36
    svg.text(pad, 40, s["pl_title"], size=17, weight=650)
    y1, y2 = 98, 204
    xs = [pad + i * (bw + gap) for i in range(5)]
    for y, name in ((y1, s["pl_lane1"]), (y2, s["pl_lane2"])):
        svg.text(xs[1], y - 10, name, size=10.5, weight=650, fill=t["muted"], spacing="0.08em")
    tall(svg, xs[0], y1, bw, y2 + bh - y1, s["pl_src"])
    tall(svg, xs[4], y1, bw, y2 + bh - y1, s["pl_cmp"], strong=True)
    for y, names in ((y1, (s["pl_img"], s["pl_run"], s["pl_fp1"])), (y2, (s["pl_host"], s["pl_hrun"], s["pl_fp2"]))):
        for k, lines in enumerate(names):
            box(svg, xs[k + 1], y, bw, bh, lines)
        for k in range(4):
            arrow(svg, xs[k] + bw, y + bh / 2.0, xs[k + 1], y + bh / 2.0)
    svg.text(pad, 296, s["pl_note"], size=12.5, fill=t["ink2"])
    return svg


# ---------------------------------------------------------------------------
# NGCC outcomes: one tall segmented bar, and a thin one per kind of candidate

def ink_on(fill):
    r, g, b = (int(fill[i:i + 2], 16) / 255.0 for i in (1, 3, 5))
    lum = 0.2126 * r ** 2.2 + 0.7152 * g ** 2.2 + 0.0722 * b ** 2.2
    return "#0b0b0b" if lum > 0.26 else "#ffffff"


def segments(svg, x, y, w, h, counts, unit, rx, numbers):
    t = svg.t
    for i, c in enumerate(counts):
        if not c:
            continue
        sw = c * unit - 2
        svg.rect(x, y, sw, h, t["ramp"][i], rx=rx)
        if numbers and sw > width(str(c), 13) + 12:
            svg.text(x + 8, y + h / 2.0 + 4.5, str(c), size=13, weight=650, fill=ink_on(t["ramp"][i]))
        x += c * unit


def ngcc(lang, theme):
    t, s = THEMES[theme], TEXT[lang]
    counts = ngcc_counts()
    total = [sum(k[i] for k in counts) for i in range(4)]
    n = sum(total)
    svg = Svg(960, 276, t, s["ng_title"] % n)
    pad = 28
    svg.text(pad, 40, s["ng_title"] % n, size=17, weight=650)
    svg.text(pad, 62, s["ng_sub"], size=12.5, fill=t["ink2"])
    unit = (svg.w - 2 * pad + 2) / float(n)
    segments(svg, pad, 84, svg.w - 2 * pad, 28, total, unit, 4, True)
    lx = pad
    for i, name in enumerate(s["ng_parts"]):
        svg.rect(lx, 128, 10, 10, t["ramp"][i], rx=2)
        svg.text(lx + 16, 137.5, name, size=12, fill=t["ink2"])
        lx += 16 + width(name, 12) + 22
    y = 176
    x_bar = pad + 150
    for name, c in zip(s["ng_kinds"], counts):
        svg.text(pad, y + 8, name, size=13)
        segments(svg, x_bar, y, 0, 8, c, unit, 4, False)
        svg.text(x_bar + sum(c) * unit + 8, y + 8.5,
                 "  ·  ".join(str(v) for v in c), size=12, fill=t["ink2"])
        y += 32
    return svg


def main():
    made = 0
    for lang in ("en", "zh"):
        for theme in ("light", "dark"):
            suffix = ("" if lang == "en" else "-zh") + "-" + theme + ".svg"
            for name, fn in (("hero", hero), ("speedup", speedup), ("pipeline", pipeline), ("ngcc", ngcc)):
                fn(lang, theme).save(name + suffix)
                made += 1
    print("wrote %d figures to docs/assets" % made)


if __name__ == "__main__":
    main()
