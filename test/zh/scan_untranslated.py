#!/usr/bin/env python3
"""Count user-visible English message literals in src/*.c that are not
covered by a ZHLANG translation.

A call site counts as "untranslated" when a message-emitting call with an
English string literal sits outside both the `#ifdef ZHLANG` branch and its
`#else` branch.  Strings wrapped in the translation macro (see
doc/zh/TASKS.md, task E-0) are treated as translated.

Usage:
    scan_untranslated.py [--root DIR] [--json] [--list FILE]

This is the measuring stick for acceptance gate E; do not change the
matching rules without updating doc/zh/ACCEPTANCE.md and baseline.json.
"""
import argparse
import glob
import json
import os
import re
import sys

CALLS = (
    "pline|You|Your|You_feel|You_cant|You_hear|You_see|pline_The|There|"
    "verbalize|Norep|urgent_pline|custompline|Strcpy|Strcat|Sprintf|Snprintf|"
    "yn_function|ynq|getlin|add_menu|add_menu_str|end_menu|putstr|"
    "livelog_printf|com_pager|Tobjnam|The|enlght_out|you_are|you_have|"
    "you_have_X|you_can|you_have_been|enl_msg"
)
CALL_RE = re.compile(r'\b(' + CALLS + r')\s*\([^;]*?"([^"\\]*(?:\\.[^"\\]*)*)"')
# a literal that reads like English prose: at least one 3+ letter word
ENGLISH_RE = re.compile(r'[A-Za-z]{3,}')
CJK_RE = re.compile(r'[一-鿿]')
# files with no player-facing text (debug, platform glue, data tables)
SKIP_FILES = {
    "alloc.c", "calendar.c", "date.c", "dlb.c", "isaac64.c", "nhmd4.c",
    "revision.c", "rnd.c", "sfbase.c", "sfstruct.c", "strutil.c",
    "report.c", "mdlib.c", "version.c",
}
# lines that intentionally stay English
SKIP_MARK = "NOTRANS"
TR_RE = re.compile(r'\bTR\s*\(')


def scan_file(path):
    hits = []
    stack = []  # 'Z' = in #ifdef ZHLANG, 'E' = in its #else, 'O' = other
    in_comment = False
    with open(path, encoding="utf-8", errors="replace") as fh:
        for lineno, line in enumerate(fh, 1):
            s = line.strip()
            if in_comment:
                if "*/" in s:
                    in_comment = False
                continue
            if s.startswith("/*") and "*/" not in s:
                in_comment = True
                continue
            m = re.match(r'#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)', s)
            if m:
                kw, rest = m.group(1), m.group(2)
                if kw in ("ifdef", "if"):
                    stack.append("Z" if re.search(r'\bZHLANG\b', rest)
                                 and "!" not in rest else "O")
                elif kw == "ifndef":
                    stack.append("E" if re.search(r'\bZHLANG\b', rest)
                                 else "O")
                elif kw in ("else", "elif") and stack:
                    if stack[-1] == "Z":
                        stack[-1] = "E"
                elif kw == "endif" and stack:
                    stack.pop()
                continue
            if "Z" in stack or "E" in stack:
                continue
            if s.startswith(("*", "//")) or SKIP_MARK in line:
                continue
            if TR_RE.search(line):  # TR("english", "中文") counts as done
                continue
            for cm in CALL_RE.finditer(line):
                lit = cm.group(2)
                if ENGLISH_RE.search(lit) and not CJK_RE.search(lit):
                    hits.append((lineno, cm.group(1), lit))
                    break
    return hits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.join(os.path.dirname(__file__),
                                                   "..", ".."))
    ap.add_argument("--json", action="store_true",
                    help="print {file: count} as JSON")
    ap.add_argument("--list", metavar="FILE",
                    help="write every hit as file:line: call: literal")
    args = ap.parse_args()

    counts, allhits = {}, []
    for path in sorted(glob.glob(os.path.join(args.root, "src", "*.c"))):
        name = os.path.basename(path)
        if name in SKIP_FILES:
            continue
        hits = scan_file(path)
        if hits:
            counts[name] = len(hits)
            allhits += [(name, h) for h in hits]

    if args.list:
        with open(args.list, "w", encoding="utf-8") as out:
            for name, (ln, call, lit) in allhits:
                out.write(f"src/{name}:{ln}: {call}: {lit}\n")
    if args.json:
        json.dump({"total": sum(counts.values()), "files": counts},
                  sys.stdout, ensure_ascii=False, indent=1, sort_keys=True)
        print()
    else:
        for name, n in sorted(counts.items(), key=lambda x: -x[1]):
            print(f"{n:5d}  src/{name}")
        print(f"{sum(counts.values()):5d}  TOTAL")


if __name__ == "__main__":
    main()
