"""Helpers for precheck.sh. Read-only on the clone: nothing here writes into it.

usage: python -I precheck_lib.py <cmd> <clone> <base> [args]
  fmt      clang-format --dry-run --Werror on the lines added since <base> (same ranges as fmt_lines.py)
  braces   { } and ( ) balance per changed .cpp/.h, compared with the base version
  cmake    every test source test/CMakeLists.txt names exists, and no test name is listed twice
  names    denied words (PRECHECK_DENY_WORDS) in added lines; addresses / decompiled names in src/ comments
  groups   test groups of the changed test files; prints GROUPS=a,b and SRC_CHANGED=0|1
  cpps     changed .cpp under src/ and test/ (not deleted), one per line, as "src|test <path>"
Exit code: 0 pass, 1 findings.
"""
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import re_config  # noqa: E402

CODE = (".cpp", ".h", ".hpp", ".inl")


def git(clone, *args):
    return subprocess.run(["git", "-C", clone, *args], capture_output=True, text=True, encoding="utf-8",
                          errors="replace").stdout


def changed(clone, base):
    return [f for f in git(clone, "diff", "--name-only", "--diff-filter=d", base + "..HEAD").splitlines() if f]


def added_ranges(clone, base):
    diff = git(clone, "diff", "-U0", base + "..HEAD", "--", *("*" + e for e in CODE))
    ranges, cur = {}, None
    for line in diff.splitlines():
        if line.startswith("+++ "):
            cur = None if line.endswith("/dev/null") else line[6:]
        elif line.startswith("@@") and cur:
            m = re.search(r"\+(\d+)(?:,(\d+))?", line)
            start, n = int(m.group(1)), int(m.group(2) or 1)
            if n:
                ranges.setdefault(cur, []).append(f"--lines={start}:{start + n - 1}")
    return ranges


def find_clang_format():
    """CLANG_FORMAT, else clang-format on the PATH, else the binary of the clang-format pip package."""
    cf = re_config.get("CLANG_FORMAT") or shutil.which("clang-format")
    if not cf:
        try:
            import clang_format
            cf = os.path.join(os.path.dirname(clang_format.__file__), "data", "bin", "clang-format.exe")
        except ImportError:
            cf = None
    if not cf or not os.path.isfile(cf) and not shutil.which(cf):
        sys.exit("clang-format not found: set CLANG_FORMAT, put it on the PATH, or pip install clang-format "
                 "(the version the CI uses)")
    return cf


def cmd_fmt(clone, base):
    CF = find_clang_format()
    bad = []
    for f, r in added_ranges(clone, base).items():
        p = subprocess.run([CF, "--dry-run", "--Werror", "--style=file", *r, f], cwd=clone, capture_output=True,
                           text=True, encoding="utf-8", errors="replace")
        if p.returncode != 0:
            n = p.stderr.count("error:")
            bad.append(f)
            print(f"FMT {f}: {n} line(s) would change")
    print(f"fmt: {len(bad)} file(s) need clang-format")
    return 1 if bad else 0


STRIP = re.compile(r'R"([^(\s]*)\((?:.|\n)*?\)\1"|//[^\n]*|/\*(?:.|\n)*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'')


def counts(text):
    t = STRIP.sub(" ", text)
    return t.count("{") - t.count("}"), t.count("(") - t.count(")")


def cmd_braces(clone, base):
    bad = 0
    for f in changed(clone, base):
        if not f.endswith(CODE):
            continue
        now = counts(open(os.path.join(clone, f), encoding="utf-8", errors="replace").read())
        old_text = subprocess.run(["git", "-C", clone, "show", f"{base}:{f}"], capture_output=True, text=True,
                                  encoding="utf-8", errors="replace")
        old = counts(old_text.stdout) if old_text.returncode == 0 else (0, 0)
        if now != old:
            bad += 1
            print(f"BRACES {f}: braces {now[0]:+d} parens {now[1]:+d} (base {old[0]:+d} {old[1]:+d})")
    print(f"braces: {bad} file(s) unbalanced")
    return 1 if bad else 0


# words that must not appear in added lines (for example the names of private work areas): a regular expression,
# matched case-insensitively; none when unset
DENY = re_config.get("PRECHECK_DENY_WORDS")
SESSION = re.compile(r"\b(?:" + DENY + r")\b") if DENY else None
# runblack.exe addresses (0x4xxxxx..0x9xxxxx, with or without leading 00) and decompiled names; colours and other
# hex constants in comments are not addresses
ADDRESS = re.compile(r"\b0x(?:00)?[4-9][0-9A-Fa-f]{5}\b|\bfn_[0-9A-Fa-f]{8}\b")


def cmd_names(clone, base):
    hits = 0
    cur, lineno, in_block = None, 0, False
    for line in git(clone, "diff", "-U0", base + "..HEAD").splitlines():
        if line.startswith("+++ "):
            cur = None if line.endswith("/dev/null") else line[6:]
            in_block = False
            continue
        if line.startswith("@@"):
            lineno = int(re.search(r"\+(\d+)", line).group(1))
            continue
        if not line.startswith("+") or cur is None or cur.startswith("docs/wiki-html/"):
            continue
        text = line[1:]
        m = SESSION.search(text) if SESSION else None
        if m:
            hits += 1
            print(f"DENIED {cur}:{lineno}: {m.group(0)}: {text.strip()[:100]}")
        if cur.startswith("src/") and cur.endswith(CODE):
            # the comment part of the line: after //, inside /* */, or a continuation line of a block comment
            comment = ""
            s = text.lstrip()
            if in_block or s.startswith("*"):
                comment = text
            if "/*" in text:
                comment += text[text.index("/*"):]
            if "//" in text:
                comment += text[text.index("//"):]
            if "/*" in text and "*/" not in text[text.index("/*"):]:
                in_block = True
            if "*/" in text:
                in_block = False
            m = ADDRESS.search(comment)
            if m:
                hits += 1
                print(f"ADDRESS {cur}:{lineno}: {m.group(0)}: {text.strip()[:100]}")
        lineno += 1
    print(f"names: {hits} finding(s)")
    return 1 if hits else 0


def cmd_cmake(clone, base):
    """Every test source test/CMakeLists.txt names exists, and no test name is listed twice."""
    text = open(os.path.join(clone, "test", "CMakeLists.txt"), encoding="utf-8").read()
    tests = re.findall(r"openblack_setup_and_add(?:_json)?_test\(([a-z0-9_]+) ([^\s)]+)", text)
    bad = 0
    seen = set()
    for name, src in tests:
        if name in seen:
            bad += 1
            print(f"DUPLICATE test name {name}")
        seen.add(name)
        if not os.path.exists(os.path.join(clone, "test", src)):
            bad += 1
            print(f"MISSING test/{src}")
    print("cmake tests OK" if not bad else f"cmake tests: {bad} problem(s)")
    return 1 if bad else 0


def test_groups(clone):
    """test file (relative to test/) -> group, from test/CMakeLists.txt in order."""
    text = open(os.path.join(clone, "test", "CMakeLists.txt"), encoding="utf-8").read()
    text = re.sub(r"#[^\n]*", "", text)
    group, out = None, {}
    for m in re.finditer(r"openblack_test_group\(\s*(\w+)\s*\)|openblack_setup_and_add(?:_json)?_test\(\s*(\w+)\s+([^\s)]+)",
                         text):
        if m.group(1):
            group = m.group(1)
        elif group and not m.group(2).startswith("$"):
            out[m.group(3)] = group
    return out


def cmd_groups(clone, base):
    files = changed(clone, base)
    table = test_groups(clone)
    groups = set()
    for f in files:
        if not f.startswith("test/"):
            continue
        rel = f[5:]
        if rel in table:
            groups.add(table[rel])
            print(f"GROUP {f} -> {table[rel]}")
        elif f.endswith((".h", ".hpp")):
            # a test header: the groups of the test sources that include it
            users = git(clone, "grep", "-l", "-E", r'#include [<"](.*/)?' + re.escape(os.path.basename(f)) + r'[>"]',
                        "--", "test/*.cpp").split()
            gs = sorted({table[u[5:]] for u in users if u[5:] in table})
            groups.update(gs)
            print(f"GROUP {f} (header) -> {','.join(gs) or 'none'}")
        elif f.endswith(".cpp") and not f.startswith("test/support/"):
            print(f"GROUP {f} -> NOT REGISTERED in test/CMakeLists.txt")
    src = any(f.startswith(("src/", "components/")) and f.endswith(CODE) for f in files) or \
        any(f.startswith("test/support/") or f == "test/CMakeLists.txt" for f in files)
    print("GROUPS=" + ",".join(sorted(groups)))
    print("SRC_CHANGED=" + ("1" if src else "0"))
    return 0


def cmd_cpps(clone, base):
    for f in changed(clone, base):
        if f.endswith(".cpp") and f.startswith(("src/", "test/")):
            print(f.split("/", 1)[0], f)
    return 0


if __name__ == "__main__":
    cmd, clone, base = sys.argv[1], sys.argv[2], sys.argv[3]
    sys.exit(globals()["cmd_" + cmd](clone, base))
