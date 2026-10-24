#!/usr/bin/env python3
"""Acceptance run for the NetHack Chinese localization.

Builds the current working tree twice in a scratch directory (ZHLANG and
plain English), then:
  * collects compiler warnings and compares them with the English build
  * starts each build in wizard mode on a pty and runs the Lua checks
    (test/zh/zh_accept.lua, test/testwish.lua)
  * inspects the raw terminal output for broken UTF-8, English grammar
    glued onto Chinese text, impossible() messages and option errors
  * runs static checks and the untranslated-string scan

Gate ids (A-F) and check ids match doc/zh/TASKS.md and
doc/zh/ACCEPTANCE.md.  Exit status is 0 only when every gate named by
--require passes.

Usage:
    python3 test/zh/zh_accept.py [--require A,B,C,D,E,F] [--work DIR]
                                 [--report FILE] [--no-build]
Only macOS and Linux hosts are supported.
"""
import argparse
import json
import os
import platform
import pty
import re
import select
import shutil
import signal
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HERE = os.path.dirname(os.path.abspath(__file__))
UPSTREAM_BASE = "c2a332168"  # last upstream commit before localization
LUA_DIRS = ("lua-5.4.8",)

ANSI_RE = re.compile(rb"\x1b\[[0-9;?]*[ -/]*[@-~]|\x1b[()][0-9A-Za-z]|\x1b[=>78]")
CJK = "\u4e00-\u9fff"
MIXED_RE = re.compile(
    rf"[{CJK}](?:'s|s)\b|\b(?:the|a|an) [{CJK}]|[{CJK}]es\b")
# a lowercase English word stuck to Chinese text ("你的strengthis",
# "human女武神"); capitalized words are names (Odin, NetHack) and pass
GLUED_RE = re.compile(rf"[{CJK}][a-z]{{2,}}\b|\b[a-z]{{2,}}[{CJK}]")
# every status field must render with its value (task D2)
STATUS_ZH = (r"力:\S+", r"敏:\d+", r"体:\d+", r"智:\d+", r"慧:\d+", r"魅:\d+",
             r"生命:\d+\(\d+\)", r"魔力:\d+\(\d+\)", r"防御:-?\d+")
IMPOSSIBLE_RE = re.compile(r"程序出错了|Program in disorder|impossible:")
OPTERR_RE = re.compile(r"Unknown role|错误 在 NETHACKOPTIONS|errors? in NETHACKOPTIONS")
WARN_RE = re.compile(r"^(?:[\w./-]*/)?([\w-]+\.[ch]):(\d+):\d+: warning: (.*)$",
                     re.M)
HARD_WARN = ("-Wformat", "-Wembedded-directive")


class Report:
    def __init__(self):
        self.rows = []  # (gate, check, ok, detail)

    def add(self, gate, check, ok, detail="", info=False):
        # most details describe the failure; show them only when failing
        # unless the caller marks the detail as informational
        if ok and not info:
            detail = ""
        self.rows.append((gate, check, bool(ok), detail))

    def gate_ok(self, gate):
        rows = [r for r in self.rows if r[0] == gate]
        return bool(rows) and all(r[2] for r in rows)

    def render(self):
        out = ["# 汉化验收报告", ""]
        for gate in "ABCDEF":
            rows = [r for r in self.rows if r[0] == gate]
            if not rows:
                continue
            mark = "PASS" if self.gate_ok(gate) else "FAIL"
            out.append(f"## 门禁 {gate}: {mark}")
            out.append("")
            out.append("| 检查项 | 结果 | 说明 |")
            out.append("|---|---|---|")
            for _, check, ok, detail in rows:
                detail = detail.replace("|", "\\|").replace("\n", "<br>")
                out.append(f"| {check} | {'✅' if ok else '❌'} | {detail} |")
            out.append("")
        return "\n".join(out)


# ---------------------------------------------------------------- build

def run(cmd, cwd, log, env=None):
    with open(log, "ab") as fh:
        fh.write(("\n$ " + " ".join(cmd) + "\n").encode())
        fh.flush()
        return subprocess.call(cmd, cwd=cwd, stdout=fh, stderr=subprocess.STDOUT,
                               env=env)


def snapshot(dest):
    """Copy tracked + untracked-but-not-ignored files of the working tree."""
    if os.path.exists(dest):
        shutil.rmtree(dest)
    files = subprocess.check_output(
        ["git", "ls-files", "-z", "-co", "--exclude-standard"], cwd=REPO
    ).split(b"\0")
    for f in files:
        if not f:
            continue
        src = os.path.join(REPO, f.decode())
        if not os.path.isfile(src):
            continue  # deleted in the working tree
        dst = os.path.join(dest, f.decode())
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)


def hints_name():
    return "macOS.500" if platform.system() == "Darwin" else "linux.500"


def strip_chinese_hints(tree):
    path = os.path.join(tree, "sys", "unix", "hints", hints_name())
    with open(path) as fh:
        text = fh.read()
    text = re.sub(r"[ \t]+chinese\b", "", text)
    text = text.replace("#-INCLUDE chinese.500\n", "")
    with open(path, "w") as fh:
        fh.write(text)


def build(tree, work, log):
    cache = os.path.join(work, "lua-cache")
    if run(["sh", "setup.sh", "hints/" + hints_name()],
           os.path.join(tree, "sys", "unix"), log):
        return False
    lib = os.path.join(tree, "lib")
    if all(os.path.isdir(os.path.join(cache, d)) for d in LUA_DIRS):
        os.makedirs(lib, exist_ok=True)
        for d in LUA_DIRS:
            shutil.copytree(os.path.join(cache, d), os.path.join(lib, d),
                            symlinks=True, dirs_exist_ok=True)
    else:
        if run(["make", "fetch-Lua"], tree, log):
            return False
        for d in LUA_DIRS:
            shutil.copytree(os.path.join(lib, d), os.path.join(cache, d),
                            symlinks=True, dirs_exist_ok=True)
    jobs = str(os.cpu_count() or 4)
    if run(["make", "-j" + jobs, "WANT_SOURCE_INSTALL=1", "all"], tree, log):
        return False
    if run(["make", "WANT_SOURCE_INSTALL=1", "install"], tree, log):
        return False
    sysconf = os.path.join(tree, "playground", "sysconf")
    with open(sysconf) as fh:
        text = fh.read()
    with open(sysconf, "w") as fh:
        fh.write(re.sub(r"^WIZARDS=.*$", "WIZARDS=*", text, flags=re.M))
    return True


def warnings(log):
    with open(log, errors="replace") as fh:
        text = fh.read()
    return sorted({(f, int(l), m) for f, l, m in WARN_RE.findall(text)})


# ---------------------------------------------------------------- play

class VT:
    """Just enough of a terminal to see what an 80x24 screen shows.

    CJK and other East Asian wide characters take two cells, as on a
    real terminal, so column bookkeeping errors in the tty port show up
    as overwritten text.
    """

    W, H = 80, 24
    CSI_RE = re.compile(r"\x1b\[([0-9;?]*)[ -/]*([@-~])")
    ESC_RE = re.compile(r"\x1b[()][0-9A-Za-z]|\x1b[=>78DME]")

    def __init__(self, raw):
        import unicodedata
        self.uw = unicodedata.east_asian_width
        self.g = [[" "] * self.W for _ in range(self.H)]
        self.x = self.y = 0
        self.feed(raw.decode("utf-8", "replace"))

    def feed(self, text):
        i = 0
        while i < len(text):
            c = text[i]
            if c == "\x1b":
                m = self.CSI_RE.match(text, i)
                if m:
                    self.csi(m.group(1), m.group(2))
                    i = m.end()
                    continue
                m = self.ESC_RE.match(text, i)
                i = m.end() if m else i + 1
                continue
            if c == "\r":
                self.x = 0
            elif c == "\n":
                self.y = min(self.y + 1, self.H - 1)
            elif c == "\b":
                self.x = max(0, self.x - 1)
            elif c >= " ":
                w = 2 if self.uw(c) in "WF" else 1
                if self.x + w > self.W:
                    self.x, self.y = 0, min(self.y + 1, self.H - 1)
                self.g[self.y][self.x] = c
                if w == 2 and self.x + 1 < self.W:
                    self.g[self.y][self.x + 1] = ""
                self.x += w
            i += 1

    def csi(self, args, cmd):
        a = [int(v) if v.isdigit() else 0
             for v in args.lstrip("?").split(";")] if args else []
        n = a[0] if a and a[0] else 1
        if cmd in "Hf":
            self.y = min(self.H - 1, n - 1)
            self.x = min(self.W - 1, (a[1] if len(a) > 1 and a[1] else 1) - 1)
        elif cmd == "A":
            self.y = max(0, self.y - n)
        elif cmd == "B":
            self.y = min(self.H - 1, self.y + n)
        elif cmd == "C":
            self.x = min(self.W - 1, self.x + n)
        elif cmd == "D":
            self.x = max(0, self.x - n)
        elif cmd == "K":
            mode = a[0] if a else 0
            cols = (range(self.x, self.W) if mode == 0 else
                    range(0, self.x + 1) if mode == 1 else range(self.W))
            for j in cols:
                self.g[self.y][j] = " "
        elif cmd == "J":
            mode = a[0] if a else 0
            if mode == 2:
                self.g = [[" "] * self.W for _ in range(self.H)]
            elif mode == 0:
                for j in range(self.x, self.W):
                    self.g[self.y][j] = " "
                for r in range(self.y + 1, self.H):
                    self.g[r] = [" "] * self.W

    def render(self):
        return ["".join(r).rstrip() for r in self.g]


class Game:
    """Minimal expect-style driver for the tty interface."""

    PROMPTS = (
        ("Destroy old game?", "y"),
        ("Shall I pick", "y"),
        ("Is this ok?", "y"),
        ("Hit return to continue", "\r"),
        ("In what direction?", "\x1b"),
        ("--More--", "\r"),
        ("(end)", "\r"),
        ("(1 of", "\r"), ("(2 of", "\r"), ("(3 of", "\r"),
    )

    def __init__(self, playground, options):
        for name in os.listdir(playground):
            if "wizard" in name:
                os.remove(os.path.join(playground, name))
        save = os.path.join(playground, "save")
        for name in os.listdir(save) if os.path.isdir(save) else []:
            os.remove(os.path.join(save, name))
        env = dict(os.environ, TERM="xterm-256color", LANG="en_US.UTF-8",
                   LC_ALL="en_US.UTF-8", NETHACKOPTIONS=options,
                   COLUMNS="80", LINES="24")
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.chdir(playground)
            os.execve(os.path.join(playground, "nethack"), ["nethack", "-D"],
                      env)
        self.raw = b""
        self.mark = 0

    def tail(self, n=400):
        return ANSI_RE.sub(b"", self.raw[-n * 4:]).decode("utf-8", "replace")

    def read(self, idle=1.0, limit=20.0):
        end = time.time() + limit
        last = time.time()
        while time.time() < end and time.time() - last < idle:
            r, _, _ = select.select([self.fd], [], [], 0.1)
            if r:
                try:
                    data = os.read(self.fd, 65536)
                except OSError:
                    return
                if not data:
                    return
                self.raw += data
                last = time.time()

    def answer_prompts(self, rounds=30):
        for _ in range(rounds):
            self.read()
            new = ANSI_RE.sub(b"", self.raw[self.mark:]).decode("utf-8",
                                                                "replace")
            hit = None
            for pat, key in self.PROMPTS:
                pos = new.rfind(pat)
                if pos >= 0 and (hit is None or pos > hit[0]):
                    hit = (pos, key)
            if not hit:
                return
            self.mark = len(self.raw)
            self.send(hit[1], settle=False)

    def send(self, keys, settle=True):
        os.write(self.fd, keys.encode())
        if settle:
            self.answer_prompts()

    def command(self, ext, arg=None):
        self.mark = len(self.raw)
        self.send("#" + ext + "\r", settle=False)
        self.read()
        if arg is not None:
            self.send(arg + "\r", settle=False)
        self.answer_prompts()

    def close(self):
        try:
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
        except OSError:
            pass

    def text(self):
        return ANSI_RE.sub(b"", self.raw).decode("utf-8", "replace")


def play(playground, zh, rep, outdir):
    tag = "zh" if zh else "en"
    shutil.copy(os.path.join(REPO, "test", "testwish.lua"), playground)
    if zh:
        shutil.copy(os.path.join(HERE, "zh_accept.lua"), playground)
    # role given by its English name: config files written for the
    # English game must keep working (task C4)
    opts = ("role:Valkyrie,race:human,gender:female,align:neutral,"
            "!tutorial,!legacy,!autopickup,symset:plain,"
            "msg_window:single")
    g = Game(playground, opts)
    status_rows = []
    wish_raw = b""
    try:
        g.answer_prompts(60)
        g.send("\x12")  # ^R redraw, so the status line is freshly painted
        status_rows = VT(g.raw).render()[-2:]
        if zh:
            g.command("wizloadlua", "zh_accept")
            g.send("\x1b")
        g.command("wizloadlua", "testwish")
        g.send("\x1b")
        if not zh:
            g.send("i")  # class headings must match upstream (task A7)
            g.send("\x1b")
        if zh:
            g.command("overview")
            g.send("\x1b")
            g.mark = len(g.raw)
            g.send("i")
            g.send("\x1b")
            # Chinese typed into a getlin() prompt (task D4)
            start = len(g.raw)
            g.send("\x17", settle=False)
            g.read()
            g.send("银匕首\r")
            g.send("\x1b")
            wish_raw = g.raw[start:]
            # enlightenment pages (task B5)
            g.send("\x18")
            g.send("\x1b")
    finally:
        g.close()
    with open(os.path.join(outdir, f"tty-{tag}.raw"), "wb") as fh:
        fh.write(g.raw)
    text = g.text()
    with open(os.path.join(outdir, f"tty-{tag}.txt"), "w") as fh:
        fh.write(text)

    if not zh:
        rep.add("C", "C2-en-build-testwish", "testwish: OK" in text,
                "英文版 test/testwish.lua")
        # a Valkyrie starts with weapons, armor and food; the headings
        # must be the real class names, in that order
        heads = re.findall(r"\b(Illegal objects|Weapons|Armor|Comestibles|"
                           r"Tools|Rings)\b", text[text.rfind("testwish"):])
        rep.add("A", "A7-en-inventory-headers",
                heads[:3] == ["Weapons", "Armor", "Comestibles"],
                "英文版物品栏分类标题为: " + ", ".join(heads[:4]))
        return

    found = {}
    # the tty may render the result window without line breaks, so a
    # detail ends at the next "ZHT|" rather than at end of line
    for m in re.finditer(r"ZHT\|([\w-]+)\|(OK|FAIL)\|(.*?)(?=ZHT\||[\r\n]|$)",
                         text):
        found[m.group(1)] = (m.group(2) == "OK", m.group(3).strip())
    if "SUMMARY" not in text or not found:
        rep.add("A", "lua-run", False,
                "zh_accept.lua 没有跑完，见 tty-zh.txt")
    for cid, (ok, detail) in sorted(found.items()):
        rep.add(cid[0], cid, ok, detail)

    rep.add("C", "C2-zh-build-testwish", "testwish: OK" in text,
            "中文版跑上游 test/testwish.lua（英文许愿与英文标识符）")
    cant = re.findall(r"can't find (?:object|feature) \"[^\"]*\"", text)
    rep.add("A", "A2-appear-as-lookup", not cant, "；".join(cant))
    imp = IMPOSSIBLE_RE.findall(text)
    rep.add("A", "A6-no-impossible", not imp,
            f"出现 {len(imp)} 次 impossible()（含 Lua 报错），见 tty-zh.txt"
            if imp else "")
    opterr = OPTERR_RE.findall(text)
    rep.add("C", "C4-role-option", not opterr,
            "NETHACKOPTIONS role:Valkyrie 报错" if opterr else "")
    try:
        g.raw.decode("utf-8")
        rep.add("D", "D1-utf8-stream", True)
    except UnicodeDecodeError as e:
        ctx = g.raw[max(0, e.start - 40):e.start + 10]
        rep.add("D", "D1-utf8-stream", False,
                f"字节 {e.start} 处 UTF-8 被截断: {ctx!r}")
    mixed = sorted({m.group(0) for m in MIXED_RE.finditer(text)})
    rep.add("B", "B2-no-mixed-grammar", not mixed,
            "、".join(mixed[:12]))
    # the driver's own "#wizloadlua" / "#overview" echo sits next to the
    # Chinese prompt and is not game text
    glued = sorted({m.group(0) for m in GLUED_RE.finditer(text)
                    if not re.search(r"wizloadlua|overview", m.group(0))})
    rep.add("B", "B5-no-embedded-english", not glued,
            f"{len(glued)} 处，如 " + "、".join(glued[:15]))
    try:
        wish_raw.decode("utf-8")
        rep.add("D", "D4-getlin-echo", bool(wish_raw), "未捕获到许愿输入")
    except UnicodeDecodeError as e:
        rep.add("D", "D4-getlin-echo", False,
                f"输入中文时回显的 UTF-8 被拆开: {wish_raw[e.start - 20:e.start + 20]!r}")
    rows = "\n".join(status_rows)
    ok = all(re.search(p, rows) for p in STATUS_ZH)
    rep.add("D", "D2-status-line", ok,
            "状态栏渲染为: " + " / ".join(r.strip() for r in status_rows))
    ov = ""
    if "overview" in text:
        ov = text[text.rfind("overview"):]
        ov = ov[:ov.find("(end)")] if "(end)" in ov else ov
    rep.add("F", "F2-overview-dungeon-names",
            ov and "Dungeons of Doom" not in ov,
            "#overview 中仍出现 Dungeons of Doom" if ov else "未捕获到 #overview")


# ---------------------------------------------------------------- static

def read(path):
    with open(os.path.join(REPO, path), encoding="utf-8", errors="replace") as fh:
        return fh.read()


def static_checks(rep):
    # A3: Windows packaging lists the Chinese data files
    for f in ("sys/windows/GNUmakefile", "sys/windows/Makefile.nmake",
              "sys/windows/vs/files.props"):
        rep.add("A", f"A3-{os.path.basename(f)}", "help_zh" in read(f),
                f"{f} 未包含 *_zh 帮助文件")
    # A4: process_text_window keeps upstream glyph handling
    rep.add("A", "A4-wintty-glyph-path",
            "end_glyphout();" in read("win/tty/wintty.c").split(
                "process_text_window", 1)[-1][:6000],
            "process_text_window 的字形输出逻辑被删除，英文版受影响")
    # B3: quest text substitution is localized
    rep.add("B", "B3-questpgr", "ZHLANG" in read("src/questpgr.c").split(
        "convert_arg", 1)[-1][:5000], "questpgr.c convert_arg 未中文化")
    # E0: the dual-text macro exists and no new #ifdef sits inside a call
    rep.add("E", "E0-TR-macro", re.search(r"#\s*define\s+TR\s*\(",
                                          read("include/global.h") +
                                          read("include/hack.h")) is not None,
            "缺少 TR(en, zh) 宏")
    # F1: no byte-identical copy of dungeon.lua
    zh = os.path.join(REPO, "dat", "dungeon_zh.lua")
    rep.add("F", "F1-dungeon-zh-not-copy",
            not os.path.exists(zh) or read("dat/dungeon_zh.lua")
            != read("dat/dungeon.lua"), "dat/dungeon_zh.lua 与原文件完全相同")
    # F3: every player-visible English literal in the shared Lua level
    # scripts (messages, tutorial engravings) has an entry in the
    # English->Chinese table dat/msgs_zh.lua
    table_path = os.path.join(REPO, "dat", "msgs_zh.lua")
    table = read("dat/msgs_zh.lua") if os.path.exists(table_path) else ""
    keys = set(re.findall(r'\[\s*"((?:[^"\\]|\\.)*)"\s*\]\s*=', table))
    call_re = re.compile(r'(?:des\.message|nh\.pline|\bpline|nh\.verbalize|'
                         r'\bverbalize)\(\s*"((?:[^"\\]|\\.)*)"'
                         r'|\btext\s*=\s*"((?:[^"\\]|\\.)*)"')
    missing = []
    for path in sorted(os.listdir(os.path.join(REPO, "dat"))):
        if (not path.endswith(".lua") or "_zh" in path
                or path in ("quest.lua", "dungeon.lua")):
            continue
        for ln, line in enumerate(read("dat/" + path).splitlines(), 1):
            if line.lstrip().startswith("--"):
                continue
            for m in call_re.finditer(line):
                lit = m.group(1) or m.group(2) or ""
                if (re.search(r"[A-Za-z]{3,}", lit)
                        and not re.search(f"[{CJK}]", lit)
                        and lit not in keys):
                    missing.append(f"{path}:{ln}")
    tut = [m for m in missing if m.startswith("tut-")]
    rep.add("F", "F3-tutorial", not tut,
            f"{len(tut)} 条教程文字未进 dat/msgs_zh.lua，如 {', '.join(tut[:3])}")
    rest = [m for m in missing if not m.startswith("tut-")]
    rep.add("F", "F6-lua-messages", not rest,
            f"{len(rest)} 条关卡消息未进 dat/msgs_zh.lua，如 {', '.join(rest[:5])}")
    # F4: random text files have Chinese versions
    missing = [n for n in ("rumors.tru", "rumors.fal", "oracles.txt",
                           "engrave.txt", "epitaph.txt", "bogusmon.txt")
               if not os.path.exists(os.path.join(
                   REPO, "dat", n.replace(".", "_zh.", 1)))]
    rep.add("F", "F4-random-text", not missing, "缺少: " + ", ".join(missing))
    # F5: artifact names translated
    rep.add("F", "F5-artifacts", re.search(f"[{CJK}]", read(
        "include/artilist.h") + (read("include/artilist_zh.h") if
        os.path.exists(os.path.join(REPO, "include/artilist_zh.h")) else ""))
        is not None, "神器名未翻译")
    # C5: two different English object names must not share one Chinese
    # name (the files are line-aligned, so pair them line by line)
    first = re.compile(r'^\s*(?!GENERIC)[A-Z_]+\(\s*(?:OBJ\(\s*)?"([^"]+)"')
    to_en = {}
    for zl, el in zip(read("include/objects_zh.h").splitlines(),
                      read("include/objects.h").splitlines()):
        zm, em = first.match(zl), first.match(el)
        if zm and em:
            to_en.setdefault(zm.group(1), set()).add(em.group(1))
    # same word used for a ring/wand and a spellbook in English too
    allowed = {"睡眠", "挖掘", "驱散亡灵"}
    dup = sorted(f"{z}={'/'.join(sorted(e))}" for z, e in to_en.items()
                 if len(e) > 1 and z not in allowed)
    rep.add("C", "C5-unique-zh-names", not dup, "重名: " + "、".join(dup))


def untranslated(rep, final):
    out = subprocess.check_output([sys.executable,
                                   os.path.join(HERE, "scan_untranslated.py"),
                                   "--root", REPO, "--json"])
    now = json.loads(out)
    with open(os.path.join(HERE, "baseline.json")) as fh:
        base = json.load(fh)
    total, btotal = now["total"], base["total"]
    worst = sorted(now["files"].items(), key=lambda x: -x[1])[:5]
    detail = (f"{btotal} → {total}；最多: " +
              ", ".join(f"{f}={n}" for f, n in worst))
    limit_total = btotal // 10 if final else btotal
    rep.add("E", "E1-untranslated-total", total <= limit_total,
            detail + f"（上限 {limit_total}）", info=True)
    big = [f"{f}={n}" for f, n in now["files"].items() if n > 15]
    if final:
        rep.add("E", "E2-untranslated-per-file", not big,
                "超过 15 条: " + ", ".join(big))



# ---------------------------------------------------------------- English

CPP_FLAGS = ["-I../include", "-I../lib/lua-5.4.8/src", "-DDLB",
             '-DHACKDIR="x"', '-DDEFAULT_WINDOW_SYS="tty"', "-DSYSCF",
             '-DSYSCF_FILE="x"', "-DSECURE", "-DTIMED_DELAY", "-DNOMAIL",
             "-DTTY_GRAPHICS", "-DCURSES_GRAPHICS"]
TOKEN_RE = re.compile(r"'(?:[^'\\]|\\.)+'|" r'"(?:[^"\\]|\\.)*"')
# English text changes that are intended (file -> literals)
EN_ALLOWED = {
    "read.c": {'"something"'},
}


def string_literals(tree, name):
    """Multiset of the English string literals the preprocessed file has."""
    from collections import Counter
    r = subprocess.run(["cc", "-E", "-P"] + CPP_FLAGS + [name],
                       cwd=os.path.join(tree, "src"), capture_output=True,
                       text=True, errors="replace")
    if r.returncode:
        return None
    # adjacent literals are one string to the compiler ("a" "b" == "ab")
    lits, last_end = [], -1
    for m in TOKEN_RE.finditer(r.stdout):
        t = m.group(0)
        if (t.startswith('"') and lits and lits[-1].startswith('"')
                and not r.stdout[last_end:m.start()].strip()):
            lits[-1] = lits[-1][:-1] + t[1:]
        else:
            lits.append(t)
        last_end = m.end()
    return Counter(t for t in lits
                   if t.startswith('"') and re.search(r"[A-Za-z]{2,}", t))


def english_regression(rep, work):
    """A8: the English build must print exactly upstream's English text.
    Compare the string literals of every preprocessed src/*.c in the
    English tree with upstream (TR() expands to its English half)."""
    en = os.path.join(work, "en")
    up = os.path.join(work, "upstream")
    if not os.path.isdir(os.path.join(en, "lib", LUA_DIRS[0])):
        rep.add("A", "A8-en-text-matches-upstream", False, "英文版未构建")
        return
    if os.path.exists(up):
        shutil.rmtree(up)
    os.makedirs(up)
    tar = subprocess.Popen(["git", "archive", UPSTREAM_BASE], cwd=REPO,
                           stdout=subprocess.PIPE)
    subprocess.check_call(["tar", "-x", "-C", up], stdin=tar.stdout)
    tar.wait()
    shutil.copytree(os.path.join(en, "lib"), os.path.join(up, "lib"),
                    symlinks=True, dirs_exist_ok=True)
    for h in ("date.h", "nhlua.h"):
        if os.path.exists(os.path.join(en, "include", h)):
            shutil.copy(os.path.join(en, "include", h),
                        os.path.join(up, "include", h))
    problems = []
    for name in sorted(os.listdir(os.path.join(up, "src"))):
        if not name.endswith(".c") or not os.path.exists(
                os.path.join(en, "src", name)):
            continue
        u, e = string_literals(up, name), string_literals(en, name)
        if u is None or e is None:
            problems.append(f"{name}: 预处理失败")
            continue
        allowed = EN_ALLOWED.get(name, set())
        gone = [t for t in u if e[t] < u[t] and t not in allowed]
        new = [t for t in e if u[t] < e[t] and t not in allowed]
        for t in gone:
            problems.append(f"{name}: 缺少 {t[:60]}")
        for t in new:
            problems.append(f"{name}: 新增 {t[:60]}")
    rep.add("A", "A8-en-text-matches-upstream", not problems,
            f"{len(problems)} 处: " + "<br>".join(problems[:15]))

# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--require", default="A,B,C,D,E,F")
    ap.add_argument("--work", default="/tmp/nethack-zh-accept")
    ap.add_argument("--report", default=None)
    ap.add_argument("--no-build", action="store_true",
                    help="reuse the trees from the previous run")
    ap.add_argument("--interim", action="store_true",
                    help="gate E only requires no regression vs baseline")
    args = ap.parse_args()

    os.makedirs(args.work, exist_ok=True)
    rep = Report()
    trees = {"zh": os.path.join(args.work, "zh"),
             "en": os.path.join(args.work, "en")}
    built = {}
    for tag, tree in trees.items():
        log = os.path.join(args.work, f"build-{tag}.log")
        if not args.no_build:
            if os.path.exists(log):
                os.remove(log)
            print(f"[{tag}] 构建中 …", flush=True)
            snapshot(tree)
            if tag == "en":
                strip_chinese_hints(tree)
            built[tag] = build(tree, args.work, log)
        else:
            built[tag] = os.path.exists(os.path.join(tree, "playground",
                                                     "nethack"))
        rep.add("A", f"A0-build-{tag}", built[tag],
                "" if built[tag] else f"构建失败，见 {log}")

    if built.get("en"):
        with open(os.path.join(args.work, "build-en.log"), errors="replace") as fh:
            leaked = "-DZHLANG" in fh.read()
        rep.add("A", "A5-en-build-is-english", not leaked,
                "按 README 去掉 hints 里的 chinese 后仍带 -DZHLANG 编译"
                if leaked else "")
    if built.get("zh") and built.get("en"):
        wz = warnings(os.path.join(args.work, "build-zh.log"))
        we = warnings(os.path.join(args.work, "build-en.log"))
        hard = [w for w in wz if any(h in w[2] for h in HARD_WARN)]
        rep.add("A", "A1-no-format-warnings", not hard,
                "<br>".join(f"{f}:{l} {m}" for f, l, m in hard[:20]))
        enkeys = {(f, m) for f, _, m in we}
        new = [w for w in wz if (w[0], w[2]) not in enkeys]
        rep.add("A", "A1-no-new-warnings", not new,
                f"中文版比英文版多 {len(new)} 条警告: " +
                "<br>".join(f"{f}:{l} {m}" for f, l, m in new[:10]))
        if we:
            enfmt = [w for w in we if any(h in w[2] for h in HARD_WARN)]
            rep.add("A", "A1-en-build-clean", not enfmt,
                    "英文版出现格式警告: " +
                    "<br>".join(f"{f}:{l}" for f, l, _ in enfmt[:10]))

    for tag in ("zh", "en"):
        if built.get(tag):
            print(f"[{tag}] 运行游戏内检查 …", flush=True)
            play(os.path.join(trees[tag], "playground"), tag == "zh", rep,
                 args.work)

    if built.get("en"):
        english_regression(rep, args.work)
    static_checks(rep)
    untranslated(rep, final=not args.interim)

    text = rep.render()
    print(text)
    if args.report:
        with open(args.report, "w") as fh:
            fh.write(text + "\n")
    print(f"\n原始终端输出与构建日志: {args.work}")
    need = [g.strip() for g in args.require.split(",") if g.strip()]
    failed = [g for g in need if not rep.gate_ok(g)]
    if failed:
        print("未通过门禁: " + ", ".join(failed))
        sys.exit(1)
    print("全部要求的门禁通过")


if __name__ == "__main__":
    main()
